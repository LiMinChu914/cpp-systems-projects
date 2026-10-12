cmake_minimum_required(VERSION 3.16)
project(process_supervisor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(process_supervisor
    src/main.cpp
    src/process.cpp
    src/process_manager.cpp
    src/commands.cpp
    src/parser.cpp
)
target_include_directories(process_supervisor PRIVATE include)
target_compile_options(process_supervisor PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(process_supervisor PRIVATE pthread)
