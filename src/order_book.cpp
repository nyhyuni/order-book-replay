#include "order_book.h"

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
    Order order;

    order.id = message.order_id;
    order.price = message.price;
    order.size = message.size;
    order.side = message.side;

    m_orders[order.id] = order;
}

void OrderBook::cancel(const Message& message)
{
    m_orders.erase(message.order_id);
}

void OrderBook::fill(const Message& message)
{
    auto it = m_orders.find(message.order_id);

    if (it == m_orders.end())
        return;

    Order& order = it->second;

    if (message.size >= order.size)
    {
        m_orders.erase(it);
        return;
    }

    order.size -= message.size;
}

std::size_t OrderBook::activeOrders() const
{
    return m_orders.size();
}
