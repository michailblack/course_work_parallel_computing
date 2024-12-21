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

# Find Cppcheck static analysis tool
find_program(CPPCHECK_EXECUTABLE cppcheck)

if (CPPCHECK_EXECUTABLE)
    message(STATUS "Cppcheck found: ${CPPCHECK_EXECUTABLE}")
	set(CMAKE_CXX_CPPCHECK "cppcheck;--enable=warning,style,performance,portability;--inconclusive;--verbose;--check-level=exhaustiv;--language=c++;--std=c++20")
else()
    message(WARNING "Cppcheck not found. Static analysis will be skipped.")
endif()

add_subdirectory(server)
add_subdirectory(clients/cpp)