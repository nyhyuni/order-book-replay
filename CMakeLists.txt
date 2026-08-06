cmake_minimum_required(VERSION 3.16)

project(OrderBookReplay)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(order_book_replay
    src/main.cpp
    src/csv_reader.cpp
)
