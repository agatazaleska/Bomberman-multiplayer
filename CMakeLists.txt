cmake_minimum_required(VERSION 3.16)
project(bombowe_roboty)

set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_FLAGS "-std=gnu++2a -Wall -Wextra -Wconversion -Werror -O2")

find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(robots-client robots-client.cpp options-parser.cpp definitions.h message-serializer.cpp game-handler.cpp)
add_executable(robots-server robots-server.cpp options-parser.cpp definitions.h message-serializer.cpp game-handler.cpp)

target_link_libraries(robots-client LINK_PUBLIC ${Boost_LIBRARIES} pthread)
target_link_libraries(robots-server LINK_PUBLIC ${Boost_LIBRARIES} pthread)
