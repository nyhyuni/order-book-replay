#include <iostream>

#include "csv_reader.h"

int main()
{
    try
    {
        CSVReader reader("data/xnas-itch-20220610.mbo.csv");

        Message message;

        int count = 0;

        while (reader.next(message))
        {
            std::cout
                << "Order ID: " << message.order_id
                << "  Price: " << message.price
                << "  Size: " << message.size
                << '\n';

            if (++count == 10)
                break;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
