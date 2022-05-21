cmake_minimum_required(VERSION 3.16)
project(bombowe_roboty)

set(CMAKE_CXX_STANDARD "20")
set(CMAKE_C_FLAGS "-g -std=gnu++2a -Wall -Wextra -Wconversion -Wshadow -Werror -02")

find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(robots-client robots-client.cpp err.h options-parser.cpp)
add_executable(robots-server robots-server.cpp err.h)

target_link_libraries(robots-client LINK_PUBLIC ${Boost_LIBRARIES} pthread)
target_link_libraries(robots-server LINK_PUBLIC ${Boost_LIBRARIES} pthread)

