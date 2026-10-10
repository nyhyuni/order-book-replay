#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "message.h"
#include "order.h"

class OrderBook
{
public:
    void process(const Message& message);

    std::size_t activeOrders() const;

private:
    void add(const Message& message);
    void cancel(const Message& message);
    void fill(const Message& message);

    std::unordered_map<std::uint64_t, Order> m_orders;
};
