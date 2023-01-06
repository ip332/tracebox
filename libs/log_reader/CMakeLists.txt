define_lib(Reader SHARED)
add_dependencies(Reader protos)
target_link_libraries(Reader TcpServer protos)