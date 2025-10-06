set(UTILS_SOURCES
    src/cmd_pipe.cpp
    src/logger.cpp
    src/serial_interface.cpp
)

# Create a shared library
add_library(cellular_utils SHARED ${UTILS_SOURCES})

# Public headers
target_include_directories(cellular_utils
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)