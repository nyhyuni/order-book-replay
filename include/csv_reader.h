#pragma once

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
};
