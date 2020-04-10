# cmake for minibson

cmake_minimum_required(VERSION 3.12)

project(minibson)

include(cmake/build.cmake)

add_executable(test_0 test.cpp)
target_compile_features(test_0 PRIVATE cxx_std_17)
