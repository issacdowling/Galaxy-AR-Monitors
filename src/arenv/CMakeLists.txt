cmake_minimum_required(VERSION 3.29)

project(galaxy LANGUAGES CXX)

add_executable(galaxy main.cpp)

target_link_libraries(galaxy PRIVATE
 raylib
 opencv_videoio
 opencv_core
)