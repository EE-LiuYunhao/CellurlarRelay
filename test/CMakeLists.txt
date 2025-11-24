set(CMD_APP_SOURCES
    src/main.cpp
	../uart_service/src/sms.cpp
)

# Create the executable
add_executable(cellular_test ${CMD_APP_SOURCES})

# Public headers
target_include_directories(cellular_test
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../uart_service/include
)

# Link against cellular_utils library
target_link_libraries(cellular_test
    PRIVATE
    cellular_utils
)
