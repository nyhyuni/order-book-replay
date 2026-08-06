#pragma once

#include <cstdint>

#include "message.h"

struct Order
{
    uint64_t id;
    int64_t price;
    uint32_t size;

    Side side;
};
