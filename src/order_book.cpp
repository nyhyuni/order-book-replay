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
    auto it = m_orders.find(message.order_id);

    if (it == m_orders.end())
    {
        return;
    }

    m_orders.erase(it);
}

size_t OrderBook::activeOrders() const
{
    return m_orders.size();
}
