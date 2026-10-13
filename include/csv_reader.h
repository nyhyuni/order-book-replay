#pragma once

#include <cstddef>
#include <string>
#include <fstream>

#include "message.h"

class CSVReader
{
public:
    explicit CSVReader(const std::string& filename);
    bool next(Message& message);
private:
    std::ifstream file_;
    std::size_t line_number_{1}; // The header is line 1.
};
