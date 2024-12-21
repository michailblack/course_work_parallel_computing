cmake_minimum_required(VERSION 3.29)

project(course_work_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE server_sources src/*.cpp)

add_executable(${PROJECT_NAME} ${server_sources})

target_precompile_headers(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/pch.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
)

target_link_libraries(${PROJECT_NAME}
    wsock32
    ws2_32
    spdlog
)