cmake_minimum_required(VERSION 3.29)

project(galaxy LANGUAGES CXX)

add_executable(galaxy main.cpp)

target_link_libraries(galaxy PRIVATE
 raylib
)

# libscreencapture-wayland additions. To prevent Doxygen errors, install (Arch: doxygen graphviz)
set(BUILD_SHARED_LIBS ON)
set(ENABLE_PORTAL_MODULE ON)
set(ENABLE_PIPEWIRE_MODULE ON)
set(ENABLE_FFMPEG_MODULE OFF)
set(ENABLE_GSTREAMER_MODULE OFF)
set(EXPORT_CPP_INTERFACE ON)
add_subdirectory(include/libscreencapture-wayland)

target_link_libraries(galaxy PRIVATE screencapture-wayland)
