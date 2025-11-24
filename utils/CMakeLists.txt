set(UTILS_SOURCES
    src/cmd_pipe.cpp
    src/error.cpp
    src/serial_interface.cpp
	src/options.cpp
)

# Create a shared library
add_library(cellular_utils SHARED ${UTILS_SOURCES})

find_package(yaml-cpp REQUIRED)

find_package(CURL REQUIRED)

# Public headers
target_include_directories(cellular_utils
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)


target_link_libraries(cellular_utils
	PUBLIC
	CURL::libcurl
	yaml-cpp
)

target_link_options(cellular_utils PRIVATE "-rdynamic")

install(TARGETS cellular_utils
	LIBRARY DESTINATION lib        # .so files
	ARCHIVE DESTINATION lib        # static libs, if any
	RUNTIME DESTINATION bin        # DLLs on Windows
)
