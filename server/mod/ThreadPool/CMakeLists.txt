add_library(ThreadPool SHARED src/ThreadPool.cpp)
target_include_directories(ThreadPool PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(ThreadPool PUBLIC Logger PRIVATE Threads::Threads)