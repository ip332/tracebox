define_lib(LogClient SHARED)
add_dependencies(LogClient data_protos)

target_link_libraries(LogClient data_protos ${PROTOBUF_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

