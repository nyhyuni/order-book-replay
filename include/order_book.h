#pragma once

#include <unordered_map>

#include "message.h"
#include "order.h"

class OrderBook
{
public:
    void process(const Message& message);

    size_t activeOrders() const;

private:
    void add(const Message& message);
    void cancel(const Message& message);

    std::unordered_map<uint64_t, Order> m_orders;
};
