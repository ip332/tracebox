define_lib(LogReader SHARED)
add_dependencies(LogReader protos)
target_link_libraries(LogReader TcpServer data_protos ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})