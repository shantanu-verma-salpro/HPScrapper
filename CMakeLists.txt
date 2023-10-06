cmake_minimum_required(VERSION 3.10)

project(MyProject)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(UV REQUIRED)

# Add all source files and header files to the executable
file(GLOB SOURCES "src/*.cpp" "src/*.hpp")

add_executable(${PROJECT_NAME} ${SOURCES})

# Link the libraries to your project
target_link_libraries(${PROJECT_NAME} 
                      PRIVATE 
                      CURL::libcurl
                      Threads::Threads
                      UV::UV)

# Assuming lexbor has CMake config files, you can use this:
find_package(lexbor CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE liblexbor::liblexbor)
