cmake_minimum_required(VERSION 3.16)

project(data_logger)

# We need C++17 in order to use std::string_view
set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED BITBAKE AND NOT DEFINED catkin_FOUND)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif()

# Standard macro to build a SHARED or STATIC library.
include(tools/define_lib.cmake)

# We use protobuf to define a complex data type.
include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data_protos)

# in case of a standalone build the commons folder is expected to be at the same level
if (NOT DEFINED BITBAKE)
    # It may be built as a standalone project or using a catkin when it is used as a submodule in brain.
    get_filename_component(COMMONS_FULL_PATH "../commons" REALPATH)
    if (EXISTS "${COMMONS_FULL_PATH}" AND EXISTS "${COMMONS_FULL_PATH}/CMakeLists.txt")
        add_subdirectory(../commons ${CMAKE_CURRENT_BINARY_DIR}/commons)
        include_directories(../commons/data_bridge)
        include_directories(../commons/config_management)
        include_directories(../commons/logging)
        include_directories(../commons/ipc)
        include_directories(../commons/)
        include_directories(../commons/diagnostics)
        include_directories(${CMAKE_CURRENT_BINARY_DIR}/commons/protos)
        set (COMMONS_Found "true")
    else()
        message(ERROR "../commons folder not found, please clone it into a sibling folder.")
    endif()
else()
    # Bitbake recipe lists commons as a dependency.
    add_definitions("-DCOMMONS_FOUND")
    set (COMMONS_Found "true")
endif()

# Add library with all data_protos includes.
add_subdirectory(libs/data_protos)
# Make protoc generated headers available for any project in this project
include_directories(${CMAKE_CURRENT_BINARY_DIR}/libs/data_protos)

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
