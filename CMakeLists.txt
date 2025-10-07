cmake_minimum_required(VERSION 3.22)

project(CellularRaspberry)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/build/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/build/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/build/a")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")


# Add the executable target
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/utils")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/uart_service")

option(TEST_DEBUG "Enable test build and force Debug mode" OFF)

if(TEST_DEBUG)
    # Force Debug build type if not already set
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()

    message(STATUS "TEST_DEBUG enabled: including test subdirectory and setting build type to Debug")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test")
endif()
# add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/cmd_app")
