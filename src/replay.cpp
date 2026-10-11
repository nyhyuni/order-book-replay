#include "replay.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "csv_reader.h"
#include "order_book.h"

namespace
{
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
}

void replay(const std::string& filename)
{
    MessageQueue queue;

    std::thread worker([&] {
        // Only this thread accesses the book, so the book needs no mutex.
        OrderBook book;
        Message message;
        while (queue.pop(message))
        {
            book.process(message);
            printEvent(message, book);
        }
        printDepth(book.snapshot());
    });

    std::thread reader([&] {
        CSVReader csv(filename);
        Message message;
        while (csv.next(message))
        {
            if (!queue.push(message))
                break;
        }
        // EOF: wake the worker so it can drain the queue and finish.
        queue.close();
    });

    reader.join();
    worker.join();
}
