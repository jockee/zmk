cmake_minimum_required(VERSION 3.19)

message(STATUS "Processing root CMakeLists.txt")

add_subdirectory(custom_behavior)
# You might have other add_subdirectory lines, keep them for now
add_subdirectory(config)
add_subdirectory(dts)
