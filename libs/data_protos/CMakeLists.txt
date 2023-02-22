cmake_minimum_required(VERSION 3.10)

file(GLOB_RECURSE sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.proto)

foreach(proto ${sources})
    protobuf_generate_cpp(SRC HDR ${proto})
    list(APPEND PROTO_SRCS ${SRC})
    list(APPEND PROTO_HDRS ${HDR})
    protobuf_generate_python(PB_GEN ${proto})
    list(APPEND PYTHON_PB ${PB_GEN})
endforeach()

list(APPEND PUBLIC_HEADERS ${PROTO_HDRS})
set (PUBLIC_HEADERS  ${PUBLIC_HEADERS} PARENT_SCOPE)

define_lib(data_protos STATIC ${PROTO_SRCS} ${PROTO_HDRS})
