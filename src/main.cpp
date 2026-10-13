#include <iostream>
#include <stdexcept>
#include <string>

#include "replay.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <csv-file>|--demo [--snapshot <json-path>]"
                  << " [--delay-ms <0-10000>] [--events <count>]\n";
        return 1;
    }

    try
    {
        ReplayOptions options;
        options.synthetic = std::string(argv[1]) == "--demo";
        if (options.synthetic) options.delay_ms = 80;
        for (int i = 2; i < argc; ++i)
        {
            std::string option = argv[i];
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing value for " + option);
            std::string value = argv[++i];
            if (option == "--snapshot")
                options.snapshot_path = value;
            else if (option == "--delay-ms")
            {
                std::size_t parsed = 0;
                auto delay = std::stoll(value, &parsed);
                if (parsed != value.size() || delay < 0 || delay > 10000)
                    throw std::invalid_argument("Delay must be between 0 and 10000 ms.");
                options.delay_ms = static_cast<unsigned>(delay);
            }
            else if (option == "--events")
            {
                std::size_t parsed = 0;
                auto count = std::stoll(value, &parsed);
                if (parsed != value.size() || count < 1 || count > 1000000)
                    throw std::invalid_argument("Event count must be between 1 and 1000000.");
                options.max_events = static_cast<std::uint64_t>(count);
            }
            else
                throw std::invalid_argument("Unknown option: " + option);
        }
        if (!options.synthetic && options.max_events)
            throw std::invalid_argument("--events is only supported with --demo.");
        replay(argv[1], options);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
