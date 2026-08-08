cmake_minimum_required(VERSION 3.16)

project(data_logger)

option(DATA_LOGGER_BUILD_RUNTIME "Build network services and demo apps" ON)

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

# Commons is optional. The demo applications use gflags directly for command
# line parsing, and the logger libraries do not depend on commons.
if (NOT DEFINED BITBAKE)
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
    endif()
else()
    add_definitions("-DCOMMONS_FOUND")
endif()

# Add library with all data_protos includes.
add_subdirectory(libs/data_protos)
# Make protoc generated headers available for any project in this project
include_directories(${CMAKE_CURRENT_BINARY_DIR}/libs/data_protos)

include_directories(libs)

# List all subfolders which generate static libs
list(APPEND TARGETS
        libs/log_writer/storage_management
        )

if (DATA_LOGGER_BUILD_RUNTIME)
    find_package(gflags REQUIRED)
    if (TARGET gflags::gflags)
        set(DATA_LOGGER_GFLAGS_TARGET gflags::gflags)
    else()
        set(DATA_LOGGER_GFLAGS_TARGET gflags)
    endif()

    list(APPEND TARGETS
            libs/tcp_server
            libs/log_writer/writer
            libs/log_client
            libs/log_reader
            libs/tracebox_api
            )
endif()

foreach(target ${TARGETS})
    include_directories(${target})
endforeach()

foreach(target ${TARGETS})
    add_subdirectory(${target})
endforeach()

if (DATA_LOGGER_BUILD_RUNTIME)
    add_subdirectory(apps)
endif()

enable_testing()
add_subdirectory(tests)
