cmake_minimum_required(VERSION 3.16)
project(http_parser CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aggressive optimisation flags for benchmarking
# -O2 for tests, -O3 for release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -fsanitize=address,undefined")

# Parser library (header + implementation)
add_library(http_parser_lib
    src/http_parser.cpp
)
target_include_directories(http_parser_lib PUBLIC include)

# Test executable
add_executable(run_tests
    tests/test_parser.cpp
)
target_link_libraries(run_tests PRIVATE http_parser_lib)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()