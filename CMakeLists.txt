cmake_minimum_required(VERSION 3.10)

project(HPScrapper)

# Specify the required C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories for libcurl, libuv, and lexbor
include_directories(/usr/include)

# Include directories for lexbor (adjust if the headers are located elsewhere)
include_directories(/usr/include/lexbor)

# Specify the source files for the main project (excluding examples)
file(GLOB MAIN_SOURCES "src/*.cpp")

# Create the executable for the main project
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})

# Link directories for libcurl and libuv (On Ubuntu, they are often in these directories)
link_directories(/usr/lib/x86_64-linux-gnu)

# Link directories for lexbor (adjust if the libraries are located elsewhere)
link_directories(/usr/lib)

# Link the required libraries to your main project
target_link_libraries(${PROJECT_NAME} curl uv lexbor)

# Separate targets for examples
# If you add more examples in the future, you can just add more add_executable and target_link_libraries commands as shown below.

# Example1
add_executable(Example1 examples/example1.cpp)
target_link_libraries(Example1 curl uv lexbor)

# Example2
add_executable(Example2 examples/example2.cpp)
target_link_libraries(Example2 curl uv lexbor)
