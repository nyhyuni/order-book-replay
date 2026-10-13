#include "csv_reader.h"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
Action parseAction(const std::string& s)
{
    if (s == "A") return Action::Add;
    if (s == "C") return Action::Cancel;
    if (s == "M") return Action::Modify;
    if (s == "T") return Action::Trade;
    if (s == "F") return Action::Fill;
    if (s == "R") return Action::Clear;

    return Action::Unknown;
}

Side parseSide(const std::string& s)
{
    if (s == "A") return Side::Ask;
    if (s == "B") return Side::Bid;

    return Side::None;
}
}

CSVReader::CSVReader(const std::string& filename)
{
    file_.open(filename);
    if (!file_.is_open())
    {
        throw std::runtime_error("Could not open file.");
    }

    std::string header;
    std::getline(file_, header);
}

bool CSVReader::next(Message& message)
{
    std::string line;

    if (!std::getline(file_, line))
    {
        return false;
    }
    ++line_number_;

    std::stringstream ss(line);
    std::string field;
    std::vector<std::string> fields;

    while (std::getline(ss, field, ','))
    {
        fields.push_back(field);
    }

    if (fields.size() < 11)
    {
        throw std::runtime_error(
            "CSV line " + std::to_string(line_number_) +
            ": expected at least 11 columns, got " +
            std::to_string(fields.size()));
    }

    message.ts_event = std::stoull(fields[1]);
    message.action = parseAction(fields[5]);
    message.side = parseSide(fields[6]);
    message.price = std::stoll(fields[7]);
    message.size = std::stoul(fields[8]);
    message.order_id = std::stoull(fields[10]);

    return true;
}
