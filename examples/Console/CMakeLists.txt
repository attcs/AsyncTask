cmake_minimum_required(VERSION 3.10)

project(asynctask)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall")
set(CMAKE_VERBOSE_MAKEFILE ON)
include_directories(../../)
add_executable(${PROJECT_NAME} AsyncTask.cpp)
