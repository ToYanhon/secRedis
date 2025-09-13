cmake_minimum_required(VERSION 3.30)

project(redis LANGUAGES CXX)
# 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2 -Wall -Wextra -Wpedantic
 # -Werror
 )

add_subdirectory(client)
add_subdirectory(server)