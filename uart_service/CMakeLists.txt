set(UART_SERVICE_SOURCES
    src/email.cpp
    src/serial.cpp
    src/service.cpp
)

# Create the executable
add_executable(cellular_uart_service ${UART_SERVICE_SOURCES})

# Public headers
target_include_directories(cellular_uart_service
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

if(SYSTEM_LOG)
    target_compile_definitions(utils PUBLIC SYSTEM_LOG)
endif()

# Link against cellular_utils library
target_link_libraries(cellular_uart_service
    PRIVATE
    cellular_utils
)