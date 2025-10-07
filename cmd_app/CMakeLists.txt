set(CMD_APP_SOURCES
    src/main.cpp
)

# Create the executable
add_executable(cellular_cmd_app ${CMD_APP_SOURCES})

# Public headers
target_include_directories(cellular_cmd_app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Link against cellular_utils library
target_link_libraries(cellular_cmd_app
    PRIVATE
    cellular_utils
)