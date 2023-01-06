add_executable(data_logger logger_main.cpp)
target_link_libraries(data_logger TcpServer LogWriter StorageManagement Reader protos)

add_executable(client_demo client_demo.cpp)
target_link_libraries(client_demo LogClient protos)

add_executable(reader_demo reader_demo.cpp)
target_link_libraries(reader_demo LogClient protos)
