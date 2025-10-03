add_subdirectory(mod)

add_executable(cppRedisServer src/main.cpp)
target_link_libraries(cppRedisServer PRIVATE Logger db ThreadPool TcpServer)