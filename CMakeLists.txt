cmake_minimum_required(VERSION 2.8)
project(azsh)
set(CMAKE_C_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -std=c11 -Wall -g -D_GNU_SOURCE")
set(CMAKE_C_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 -std=c11")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")
add_subdirectory("src")

