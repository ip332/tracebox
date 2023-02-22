define_lib(LogWriter SHARED)
add_dependencies(LogWriter StorageManagement TcpServer protos)
target_link_libraries(LogWriter StorageManagement TcpServer data_protos ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
