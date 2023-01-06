add_executable(logger_demo logger_demo.cpp)
target_link_libraries(logger_demo TcpServer LogWriter StorageManagement Reader protos)

add_executable(client_demo client_demo.cpp)
target_link_libraries(client_demo LogClient protos)

add_executable(reader_demo reader_demo.cpp)
target_link_libraries(reader_demo LogClient protos)
