#pragma once

#include <cstdint>

enum class Action
{
    Add,
    Cancel,
    Modify,
    Trade,
    Fill,
    Clear,
    Unknown
};

enum class Side
{
    Bid,
    Ask,
    None
};

struct Message
{
    uint64_t ts_event{};
    uint64_t order_id{};
    int64_t price{};
    uint32_t size{};

    Action action{Action::Unknown};
    Side side{Side::None};
};
