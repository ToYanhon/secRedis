add_library(TcpClient INTERFACE)
target_include_directories(TcpClient INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 添加可执行文件
add_executable(client src/main.cpp)
target_link_libraries(client PUBLIC TcpClient)