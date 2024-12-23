cmake_minimum_required(VERSION 3.29)

project(course_work_client CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE client_sources src/*.cpp)

add_executable(${PROJECT_NAME} ${client_sources})

target_link_libraries(${PROJECT_NAME}
    wsock32
    ws2_32
)