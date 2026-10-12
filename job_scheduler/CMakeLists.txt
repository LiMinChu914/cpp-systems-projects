cmake_minimum_required(VERSION 3.16)
project(job_scheduler LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(job_scheduler
    src/main.cpp
    src/scheduler.cpp
    src/tasks.cpp
    src/command.cpp
)
target_include_directories(job_scheduler PRIVATE include)
target_compile_options(job_scheduler PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(job_scheduler PRIVATE pthread)
