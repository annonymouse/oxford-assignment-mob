cmake_minimum_required(VERSION 2.8)

project(node C)

add_definitions(-std=gnu99 -o3 -Wall -Werror -g)

include_directories(inc)

add_subdirectory(src)
