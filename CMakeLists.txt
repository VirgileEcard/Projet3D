cmake_minimum_required(VERSION 3.10)
project(WFC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(glad STATIC glad.c) 
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include") 

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(WFC main.cpp)

target_link_libraries(WFC PRIVATE glad glfw glm::glm)

if(UNIX AND NOT APPLE)
    target_link_libraries(WFC PRIVATE pthread dl)
endif()