# Standard macro to build a SHARED library.
macro(define_lib LIB_NAME TYPE)
    cmake_minimum_required(VERSION 3.16)
    project (${LIB_NAME})
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Werror -Wall -Wextra -Wno-unused-parameter")
    set (CMAKE_INCLUDE_CURRENT_DIR ON)
    file(GLOB SRCS "*.cpp")
    file(GLOB HDRS "*.h")
    list(APPEND PUBLIC_HEADERS ${HDRS})
    set (PUBLIC_HEADERS  ${PUBLIC_HEADERS} PARENT_SCOPE)
    add_library(${LIB_NAME} ${TYPE} ${HDRS} ${SRCS} ${ARGN})
endmacro()

