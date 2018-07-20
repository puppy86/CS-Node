cmake_minimum_required(VERSION 3.4)

project(client)

add_executable(client main.cpp)
target_link_libraries (client net Solver)