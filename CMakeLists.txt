cmake_minimum_required(VERSION 3.16)

project(OrderBookReplay)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

find_package(Threads REQUIRED)

add_executable(order_book_replay
    src/main.cpp
    src/csv_reader.cpp
    src/order_book.cpp
    src/replay.cpp
    src/synthetic_feed.cpp
)

target_link_libraries(order_book_replay PRIVATE Threads::Threads)

include(CTest)
if(BUILD_TESTING)
    add_executable(order_book_levels_test
        tests/order_book_levels_test.cpp
        src/order_book.cpp
    )
    add_test(NAME order_book_levels COMMAND order_book_levels_test)
endif()
