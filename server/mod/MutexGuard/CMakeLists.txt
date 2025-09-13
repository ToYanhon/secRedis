add_library(MutexGuard INTERFACE)
target_include_directories(MutexGuard INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)