#include "order_book.h"

#include <stdexcept>

void OrderBook::process(const Message& message)
{
    switch (message.action)
    {
        case Action::Add:
            add(message);
            break;

        case Action::Cancel:
            cancel(message);
            break;

        case Action::Fill:
            fill(message);
            break;

        default:
            break;
    }
}

void OrderBook::add(const Message& message)
{
    if (message.side != Side::Bid && message.side != Side::Ask)
        throw std::invalid_argument("An order must have a bid or ask side.");

    // Preserve replacement behavior without leaving the old ID in a price level.
    cancel(message);

    Order order;

    order.id = message.order_id;
    order.price = message.price;
    order.size = message.size;
    order.side = message.side;

    m_orders[order.id] = order;
    if (order.side == Side::Bid)
        m_bids[order.price].push_back(order.id);
    else
        m_asks[order.price].push_back(order.id);
}

void OrderBook::cancel(const Message& message)
{
    auto it = m_orders.find(message.order_id);
    if (it == m_orders.end())
        return;

    removeFromLevel(it->second);
    m_orders.erase(it);
}

void OrderBook::removeFromLevel(const Order& order)
{
    auto remove = [&](auto& levels) {
        auto level = levels.find(order.price);
        if (level == levels.end())
            return;

        level->second.remove(order.id);
        if (level->second.empty())
            levels.erase(level);
    };

    if (order.side == Side::Bid)
        remove(m_bids);
    else
        remove(m_asks);
}

void OrderBook::fill(const Message& message)
{
    auto it = m_orders.find(message.order_id);

    if (it == m_orders.end())
        return;

    Order& order = it->second;

    if (message.size >= order.size)
    {
        removeFromLevel(order);
        m_orders.erase(it);
        return;
    }

    order.size -= message.size;
}

std::size_t OrderBook::activeOrders() const
{
    return m_orders.size();
}

BookSnapshot OrderBook::snapshot() const
{
    BookSnapshot result;
    auto collect = [&](const auto& levels, auto& output) {
        for (const auto& [price, ids] : levels)
        {
            std::uint64_t quantity = 0;
            for (auto id : ids)
                quantity += m_orders.at(id).size;
            output.push_back({price, quantity, ids.size()});
        }
    };

    collect(m_bids, result.bids);
    collect(m_asks, result.asks);
    return result;
}
