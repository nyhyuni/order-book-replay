#include <iostream>

#include "csv_reader.h"
#include "order_book.h"

int main()
{
    try
    {
        CSVReader reader("data/xnas-itch-20220610.mbo.csv");

        OrderBook book;
        Message message;

        while (reader.next(message))
        {
            book.process(message);
        }

        std::cout << "Active Orders: "
                  << book.activeOrders()
                  << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
