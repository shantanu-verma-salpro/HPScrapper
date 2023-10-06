cmake_minimum_required(VERSION 3.10)

project(YourCPPProject)

set(CMAKE_CXX_STANDARD 17)

# Search for required packages
find_package(CURL REQUIRED)
find_package(Threads REQUIRED) # Some systems might require this for libuv

# Assuming libuv and lexbor use pkg-config, or you can adjust based on their setup
find_package(PkgConfig REQUIRED)
pkg_check_modules(UV REQUIRED libuv)
pkg_check_modules(LEXBOR REQUIRED lexbor)

# If you have multiple source files, list them all. For this example, I'll assume there's just a main.cpp
add_executable(YourCPPProject src/main.cpp)

# Link the required libraries
target_link_libraries(YourCPPProject 
    CURL::CURL 
    ${UV_LIBRARIES} 
    ${LEXBOR_LIBRARIES}
    Threads::Threads # If you need threading support
)

# Include the necessary directories
target_include_directories(YourCPPProject PRIVATE 
    ${UV_INCLUDE_DIRS} 
    ${LEXBOR_INCLUDE_DIRS}
)

# If you have tests using CTest
# enable_testing()
# add_test(NAME YourTestName COMMAND YourCPPProject)
