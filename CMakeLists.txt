cmake_minimum_required(VERSION 3.10)
project(DSM CXX)

set(CMAKE_CXX_STANDARD 17)

# Include headers
include_directories(headers)

# Collect all source files recursively
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

# Add main file
add_executable(main
    main.cpp
    ${SOURCES}
)

# Link Winsock (for Windows)
if(WIN32)
    target_link_libraries(main ws2_32)
endif()
