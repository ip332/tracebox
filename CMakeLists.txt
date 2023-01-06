cmake_minimum_required(VERSION 3.16)

project(data_logger)

# We need C++17 in order to use std::string_view
set(CMAKE_CXX_STANDARD 17)

# Standard macro to build a SHARED library.
include(tools/define_lib.cmake)

# We use protobuf to define a complex data type.
include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/protos)

# Add library with all protos includes.
add_subdirectory(libs/protos)
# Make protoc generated headers available for any project in this project
include_directories(${CMAKE_CURRENT_BINARY_DIR}/libs/protos)

include_directories(libs)

# List all subfolders which generate static libs
list(APPEND TARGETS
        libs/tcp_server
        libs/log_writer/storage_management
        libs/log_writer/writer
        libs/log_client
        libs/log_reader
        )

foreach(target ${TARGETS})
    include_directories(${target})
endforeach()

foreach(target ${TARGETS})
    add_subdirectory(${target})
endforeach()

add_subdirectory(apps)
