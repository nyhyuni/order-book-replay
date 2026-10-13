#include "synthetic_feed.h"

#include <iterator>
#include <algorithm>
#include <limits>

Message SyntheticFeed::next()
{
    Message message;
    message.ts_event = ++sequence_;
    const bool seed = next_id_ <= 40;
    const std::int64_t tick = 10000000; // One cent at the 1e9 price scale.
    if (!seed && sequence_ % 40 == 0)
    // After the initial 40 orders, move the reference price up or down one cent every 40 events.
        reference_price_ += random_() % 2 ? tick : -tick;
    const auto choice = random_() % 100;
    if (seed || orders_.empty() || (orders_.size() < 180 && choice < 55))
    {
        message.action = Action::Add;
        message.order_id = next_id_++;
        message.side = seed ? (message.order_id % 2 ? Side::Bid : Side::Ask)
                            : (random_() % 2 ? Side::Bid : Side::Ask);
        const auto distance = seed ? (message.order_id - 1) / 2 : random_() % 20;
        message.price = message.side == Side::Bid
            ? reference_price_ - static_cast<std::int64_t>(distance) * tick
            : reference_price_ + tick + static_cast<std::int64_t>(distance) * tick;
        // Keep synthetic adds on their own side of the existing spread.
        // Matching is still a separate, future feature.
        auto best_bid = std::numeric_limits<std::int64_t>::min();
        auto best_ask = std::numeric_limits<std::int64_t>::max();
        for (const auto& [id, order] : orders_)
        {
            if (order.side == Side::Bid) best_bid = std::max(best_bid, order.price);
            else best_ask = std::min(best_ask, order.price);
        }
        if (message.side == Side::Bid && best_ask != std::numeric_limits<std::int64_t>::max())
            message.price = std::min(message.price, best_ask - tick);
        if (message.side == Side::Ask && best_bid != std::numeric_limits<std::int64_t>::min())
            message.price = std::max(message.price, best_bid + tick);
        message.size = (1 + random_() % 40) * 10;
        orders_.emplace(message.order_id,
                        Order{message.order_id, message.price, message.size, message.side});
    }
    else
    {
        auto it = orders_.begin();
        std::advance(it, random_() % orders_.size());
        auto& order = it->second;
        message.order_id = order.id;
        message.price = order.price;
        message.side = order.side;
        if (choice < 75 || orders_.size() >= 180)
        {
            message.action = Action::Cancel;
            message.size = order.size;
            orders_.erase(it);
        }
        else
        {
            message.action = Action::Fill;
            message.size = 1 + random_() % order.size;
            order.size -= message.size;
            if (order.size == 0) orders_.erase(it);
        }
    }
    return message;
}
