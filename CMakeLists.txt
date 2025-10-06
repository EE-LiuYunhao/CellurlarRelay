cmake_minimum_required(VERSION 3.22)

project(CellularRaspberry)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/build/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/build/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/build/a")

option(SYSTEM_LOG "Enable system log buffer instead of stdout" ON)

# Add the executable target
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/utils")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/uart_service")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/cmd_app")