cmake_minimum_required(VERSION 3.20)
project(lob CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(lob
    src/naive_book.cpp
    src/book.cpp
    src/replay.cpp
    src/gen.cpp
)
target_include_directories(lob PUBLIC include)

add_executable(lob_replay src/main.cpp)
target_link_libraries(lob_replay PRIVATE lob)

add_executable(lob_gen tools/gen_data.cpp)
target_link_libraries(lob_gen PRIVATE lob)

add_executable(lob_bench benchmarks/bench.cpp)
target_link_libraries(lob_bench PRIVATE lob)

enable_testing()
add_executable(lob_tests
    tests/test_main.cpp
    tests/test_book.cpp
    tests/test_properties.cpp
)
target_link_libraries(lob_tests PRIVATE lob)
add_test(NAME lob_tests COMMAND lob_tests)
