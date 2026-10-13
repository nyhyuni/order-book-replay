#pragma once

#include <cstdint>
#include <map>
#include <random>

#include "message.h"
#include "order.h"

// Generates feed events, not matching decisions. Its private state keeps
// cancellations and fills tied to orders it has already emitted.
class SyntheticFeed
{
public:
    Message next();

private:
    std::mt19937 random_{42};
    std::map<std::uint64_t, Order> orders_;
    std::uint64_t next_id_{1};
    std::uint64_t sequence_{0};
    std::int64_t reference_price_{100000000000};
};
