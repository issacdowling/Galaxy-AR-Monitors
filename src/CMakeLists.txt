cmake_minimum_required(VERSION 3.29)

project(galaxy LANGUAGES CXX)

add_executable(galaxy main.cpp)

target_link_libraries(galaxy PRIVATE
 raylib
 opencv_imgproc
 opencv_core
)

# libscreencapture-wayland additions. To prevent Doxygen errors, install (Arch: doxygen graphviz)
set(BUILD_SHARED_LIBS OFF)
set(ENABLE_PORTAL_MODULE ON)
set(ENABLE_PIPEWIRE_MODULE ON)
set(ENABLE_FFMPEG_MODULE OFF)
set(ENABLE_GSTREAMER_MODULE OFF)
set(EXPORT_CPP_INTERFACE ON)
add_subdirectory(include/libscreencapture-wayland)

target_link_libraries(galaxy PRIVATE screencapture-wayland)

## Build Xreal Air Linux Driver too

project(xrealAirLinuxDriver C)

set(CMAKE_C_STANDARD 17)

add_subdirectory(include/xrealAirLinuxDriver/interface_lib)
# add_subdirectory(examples)

add_executable(xrealAirLinuxDriver	include/xrealAirLinuxDriver/src/driver.c)

target_include_directories(xrealAirLinuxDriver
		BEFORE PUBLIC include/xrealAirLinuxDriver/interface_lib/include
)

target_link_libraries(xrealAirLinuxDriver
		xrealAirLibrary
)
