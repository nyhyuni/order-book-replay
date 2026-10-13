#pragma once

#include <string>
#include <cstdint>

struct ReplayOptions
{
    std::string snapshot_path;
    unsigned delay_ms{0};
    bool synthetic{false};
    std::uint64_t max_events{0}; // Zero means continuous synthetic generation.
};

void replay(const std::string& filename, const ReplayOptions& options = {});
