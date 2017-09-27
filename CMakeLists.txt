cmake_minimum_required(VERSION 2.8)

project(SimpleECS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(CMAKE_BUILD_TYPE Debug)

include_directories(src)
add_executable(${PROJECT_NAME} src/main.cpp src/ECS.hpp)
