#include <iostream>

#include "csv_reader.h"
#include "order_book.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <csv-file>\n";
        return 1;
    }

    try
    {
        CSVReader reader(argv[1]);

        OrderBook book;
        Message message;

        while (reader.next(message))
        {
            book.process(message);

            const char* action = "Unknown";
            switch (message.action)
            {
                case Action::Add:
                    action = "Add";
                    break;
                case Action::Cancel:
                    action = "Cancel";
                    break;
                case Action::Fill:
                    action = "Fill";
                    break;
                case Action::Trade:
                    action = "Trade";
                    break;
                default:
                    break;
            }

            std::cout
                << action
                << " | Order ID: " << message.order_id
                << " | Size: " << message.size
                << " | Active Orders: " << book.activeOrders()
                << '\n';
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
