add_library(db SHARED src/db.cpp)
target_include_directories(db PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(db PUBLIC 
    Logger
)