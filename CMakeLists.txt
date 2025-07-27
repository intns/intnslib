cmake_minimum_required(VERSION 3.15)
project(intnslib LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Gather all source files
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# Create executable
add_executable(intnslib ${SRC_FILES})

# Set maximum warnings and optimizations per compiler
if(MSVC)
    target_compile_options(intnslib PRIVATE /W4 /permissive-)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    target_compile_options(intnslib PRIVATE -Wall -Wextra -pedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# Multi-config support (Release/Debug)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Output directories
set_target_properties(intnslib PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)