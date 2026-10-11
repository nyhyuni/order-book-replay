#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "message.h"
#include "order.h"

struct PriceLevel
{
    std::int64_t price;
    std::uint64_t quantity;
    std::size_t order_count;
};

struct BookSnapshot
{
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

class OrderBook
{
public:
    void process(const Message& message);

    std::size_t activeOrders() const;
    BookSnapshot snapshot() const;

private:
    void add(const Message& message);
    void cancel(const Message& message);
    void fill(const Message& message);
    void removeFromLevel(const Order& order);

    // IDs at each price are kept in arrival order for future FIFO matching.
    using OrderIds = std::list<std::uint64_t>;
    std::map<std::int64_t, OrderIds, std::greater<std::int64_t>> m_bids;
    std::map<std::int64_t, OrderIds> m_asks;
    std::unordered_map<std::uint64_t, Order> m_orders;
};
