#include "order_book.h"

#include <cassert>
#include <cstdint>
#include <limits>

namespace
{
void send(OrderBook& book, Action action, std::uint64_t id,
          Side side = Side::None, std::int64_t price = 0, std::uint32_t size = 0)
{
    Message message;
    message.action = action;
    message.order_id = id;
    message.side = side;
    message.price = price;
    message.size = size;
    book.process(message);
}
}

int main()
{
    OrderBook book;
    assert(book.snapshot().bids.empty() && book.snapshot().asks.empty());
    send(book, Action::Add, 1, Side::Bid, 99, 10);
    send(book, Action::Add, 2, Side::Bid, 100, 20);
    send(book, Action::Add, 3, Side::Bid, 100, 30);
    send(book, Action::Add, 4, Side::Ask, 102, 40);
    send(book, Action::Add, 5, Side::Ask, 101, 50);
    auto depth = book.snapshot();
    assert(depth.bids.size() == 2 && depth.asks.size() == 2);
    assert(depth.bids[0].price == 100 && depth.bids[1].price == 99);
    assert(depth.asks[0].price == 101 && depth.asks[1].price == 102);
    assert(depth.bids[0].quantity == 50 && depth.bids[0].order_count == 2);

    send(book, Action::Fill, 2, Side::None, 0, 5);
    assert(book.snapshot().bids[0].quantity == 45);
    assert(depth.bids[0].quantity == 50); // Snapshot owns its data.
    send(book, Action::Cancel, 3);
    assert(book.snapshot().bids[0].quantity == 15);
    assert(book.snapshot().bids[0].order_count == 1);
    send(book, Action::Fill, 2, Side::None, 0, 15);
    assert(book.snapshot().bids.size() == 1);
    assert(book.snapshot().bids[0].price == 99);

    // A reused ID moves between sides without leaving stale level membership.
    send(book, Action::Add, 1, Side::Ask, 103, 60);
    assert(book.snapshot().bids.empty());
    assert(book.snapshot().asks.back().price == 103);
    assert(book.activeOrders() == 3);
    send(book, Action::Cancel, 999);
    send(book, Action::Fill, 999, Side::None, 0, 10);
    assert(book.activeOrders() == 3);
    send(book, Action::Fill, 1, Side::None, 0, 61);
    assert(book.snapshot().asks.size() == 2);
    send(book, Action::Cancel, 4);
    send(book, Action::Cancel, 5);
    assert(book.activeOrders() == 0 && book.snapshot().asks.empty());

    // Aggregated quantities can exceed one order's 32-bit quantity.
    auto large = std::numeric_limits<std::uint32_t>::max();
    send(book, Action::Add, 10, Side::Bid, 100, large);
    send(book, Action::Add, 11, Side::Bid, 100, large);
    assert(book.snapshot().bids[0].quantity == std::uint64_t{large} * 2);
}
