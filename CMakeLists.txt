cmake_minimum_required(VERSION 3.10)

project(HPScrapper)

# Specify the required C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories for libcurl, libuv, and lexbor
include_directories(/usr/include)

# Include directories for lexbor (adjust if the headers are located elsewhere)
include_directories(/usr/include/lexbor)

# Specify the source files
file(GLOB SOURCES "src/*.cpp" "examples/*.cpp")

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link directories for libcurl and libuv (On Ubuntu, they are often in these directories)
link_directories(/usr/lib/x86_64-linux-gnu)

# Link directories for lexbor (adjust if the libraries are located elsewhere)
link_directories(/usr/lib)

# Link the required libraries to your project
target_link_libraries(${PROJECT_NAME} curl uv lexbor)
