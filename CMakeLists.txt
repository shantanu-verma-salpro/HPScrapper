cmake_minimum_required(VERSION 3.10)

project(HPScraper)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED) # Some systems might require this for libuv

# Assuming libuv and lexbor use pkg-config, or you can adjust based on their setup
find_package(PkgConfig REQUIRED)
pkg_check_modules(UV REQUIRED libuv)
pkg_check_modules(LEXBOR REQUIRED lexbor)

add_executable(HPScraper src/main.cpp) # and other source files
target_link_libraries(HPScraper CURL::CURL ${UV_LIBRARIES} ${LEXBOR_LIBRARIES})
target_include_directories(HPScraper PRIVATE ${UV_INCLUDE_DIRS} ${LEXBOR_INCLUDE_DIRS})
