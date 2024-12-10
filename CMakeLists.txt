cmake_minimum_required(VERSION 3.29)

project(course_work CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "Only Windows is supported")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SPDLOG_ENABLE_PCH ON)
set(SPDLOG_USE_STD_FORMAT ON)
add_subdirectory(vendor/spdlog)

add_compile_definitions(
	$<$<CONFIG:Debug>:DEBUG>
	$<$<CONFIG:Debug>:ENABLE_ASSERTS>
	$<$<CONFIG:Release>:RELEASE>
)

add_compile_options(
	$<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
)

add_subdirectory(server)