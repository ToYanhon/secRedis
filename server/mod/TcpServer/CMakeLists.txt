add_library(TcpServer STATIC
    src/TcpServer.cpp
)
target_include_directories(TcpServer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(TcpServer PUBLIC
    Logger
    ThreadPool
    db
   PRIVATE Threads::Threads
)