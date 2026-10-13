#include "replay.h"

#include <condition_variable>
#include <atomic>
#include <csignal>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <memory>
#include <queue>
#include <thread>

#include "csv_reader.h"
#include "order_book.h"
#include "synthetic_feed.h"

namespace
{
std::atomic<bool> stop_requested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

void requestStop(int)
{
    // Lock-free atomic operations are safe in this signal handler.
    stop_requested.store(true, std::memory_order_relaxed);
}

class MessageQueue
{
public:
    bool push(const Message& message)
    {
        {
            std::lock_guard lock(mutex_);
            if (closed_)
                return false;
            messages_.push(message);
        }
        ready_.notify_one();
        return true;
    }

    bool pop(Message& message)
    {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return closed_ || !messages_.empty(); });

        // Closing ends consumption only after queued events have been processed.
        if (messages_.empty())
            return false;

        message = messages_.front();
        messages_.pop();
        return true;
    }

    void close()
    {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        ready_.notify_all();
    }

private:
    std::queue<Message> messages_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool closed_{false};
};

void printEvent(const Message& message, const OrderBook& book)
{
    const char* action = "Unknown";
    switch (message.action)
    {
        case Action::Add: action = "Add"; break;
        case Action::Cancel: action = "Cancel"; break;
        case Action::Fill: action = "Fill"; break;
        case Action::Trade: action = "Trade"; break;
        default: break;
    }

    std::cout
        << action
        << " | Order ID: " << message.order_id
        << " | Size: " << message.size
        << " | Active Orders: " << book.activeOrders()
        << '\n';
}

void printDepth(const BookSnapshot& snapshot)
{
    std::cout << "Final book depth (raw integer prices):\n";
    auto printSide = [](const char* side, const auto& levels) {
        for (const auto& level : levels)
        {
            std::cout << side
                      << " | Price: " << level.price
                      << " | Quantity: " << level.quantity
                      << " | Orders: " << level.order_count << '\n';
        }
    };
    printSide("Bid", snapshot.bids);
    printSide("Ask", snapshot.asks);
}

void writeSnapshot(const std::string& path, const OrderBook& book,
                   std::uint64_t processed, const Message& last, bool complete,
                   std::int64_t run_id, bool synthetic,
                   const std::deque<Message>& recent = {})
{
    auto snapshot = book.snapshot();
    // Write beside the destination, then rename: readers see one complete file.
    auto temporary = path + ".tmp";
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(temporary);
    output << "{\"run_id\":\"" << run_id << "\",\"processed\":" << processed
           << ",\"active_orders\":" << book.activeOrders()
           << ",\"complete\":" << (complete ? "true" : "false")
           << ",\"mode\":\"" << (synthetic ? "simulation" : "replay") << "\""
           << ",\"last_event\":{\"action\":\"";
    switch (last.action)
    {
        case Action::Add: output << "Add"; break;
        case Action::Cancel: output << "Cancel"; break;
        case Action::Fill: output << "Fill"; break;
        case Action::Trade: output << "Trade"; break;
        case Action::Modify: output << "Modify (ignored)"; break;
        case Action::Clear: output << "Clear (ignored)"; break;
        default: output << "None"; break;
    }
    output << "\",\"order_id\":\"" << last.order_id
           << "\",\"size\":" << last.size << "}";
    auto writeSide = [&](const char* name, const auto& levels) {
        output << ",\"" << name << "\":[";
        bool first = true;
        for (const auto& level : levels)
        {
            if (!first) output << ',';
            first = false;
            // Strings preserve 64-bit integers when JSON is read in JavaScript.
            output << "{\"price\":\"" << level.price
                   << "\",\"quantity\":\"" << level.quantity
                   << "\",\"orders\":" << level.order_count << '}';
        }
        output << ']';
    };
    writeSide("bids", snapshot.bids);
    writeSide("asks", snapshot.asks);
    output << ",\"recent_events\":[";
    bool first = true;
    for (const auto& event : recent)
    {
        if (!first) output << ',';
        first = false;
        const char* action = "Other";
        switch (event.action)
        {
            case Action::Add: action = "Add"; break;
            case Action::Cancel: action = "Cancel"; break;
            case Action::Fill: action = "Fill"; break;
            case Action::Trade: action = "Trade"; break;
            default: break;
        }
        output << "{\"action\":\"" << action
               << "\",\"side\":\"" << (event.side == Side::Bid ? "Bid" : event.side == Side::Ask ? "Ask" : "None")
               << "\",\"order_id\":\"" << event.order_id
               << "\",\"price\":\"" << event.price
               << "\",\"size\":" << event.size << '}';
    }
    output << ']';
    output << "}\n";
    output.close();
    std::filesystem::rename(temporary, path);
}
}

void replay(const std::string& filename, const ReplayOptions& options)
{
    // Open before starting threads so a missing input is reported by main().
    std::unique_ptr<CSVReader> csv;
    if (!options.synthetic) csv = std::make_unique<CSVReader>(filename);
    if (!options.snapshot_path.empty())
    {
        auto parent = std::filesystem::path(options.snapshot_path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        // Check the destination before launching threads.
        writeSnapshot(options.snapshot_path, OrderBook{}, 0, Message{}, false, 0, options.synthetic);
    }
    const auto run_id = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    MessageQueue queue;
    stop_requested.store(false, std::memory_order_relaxed);
    auto previous_interrupt = options.synthetic ? std::signal(SIGINT, requestStop) : SIG_DFL;
    auto previous_terminate = options.synthetic ? std::signal(SIGTERM, requestStop) : SIG_DFL;

    std::thread worker([&] {
        // Only this thread accesses the book, so the book needs no mutex.
        OrderBook book;
        Message message;
        std::uint64_t processed = 0;
        std::deque<Message> recent;
        auto last_publish = std::chrono::steady_clock::now();
        while (queue.pop(message))
        {
            book.process(message);
            ++processed;
            recent.push_back(message);
            if (recent.size() > 24) recent.pop_front();
            printEvent(message, book);
            auto now = std::chrono::steady_clock::now();
            if (!options.snapshot_path.empty() &&
                now - last_publish >= std::chrono::milliseconds(200))
            {
                writeSnapshot(options.snapshot_path, book, processed, message, false, run_id,
                              options.synthetic, recent);
                last_publish = now;
            }
            if (!options.synthetic && options.delay_ms)
                std::this_thread::sleep_for(std::chrono::milliseconds(options.delay_ms));
        }
        if (!options.snapshot_path.empty())
            writeSnapshot(options.snapshot_path, book, processed, message, true, run_id,
                          options.synthetic, recent);
        printDepth(book.snapshot());
    });

    std::thread reader([&] {
        if (options.synthetic)
        {
            SyntheticFeed feed;
            std::uint64_t generated = 0;
            while (!stop_requested.load(std::memory_order_relaxed) &&
                   (!options.max_events || generated < options.max_events))
            {
                if (!queue.push(feed.next())) break;
                ++generated;
                // Pace production, rather than slowing the book worker.
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    options.delay_ms ? options.delay_ms : 80));
            }
        }
        else
        {
            Message message;
            while (csv->next(message))
            {
                if (!queue.push(message)) break;
            }
        }
        // EOF: wake the worker so it can drain the queue and finish.
        queue.close();
    });

    reader.join();
    worker.join();
    if (options.synthetic)
    {
        std::signal(SIGINT, previous_interrupt);
        std::signal(SIGTERM, previous_terminate);
    }
}
