cmake_minimum_required(VERSION 3.12.0)
find_package(Zephyr REQUIRED HINTS ${ZEPHYR_BASE})
project(zmk_behavior_cycle_string)

# Define the ZMK base directory
set(ZMK_BASE "/build/source")
message(STATUS "ZMK_BASE: ${ZMK_BASE}")

# Include necessary ZMK include directories
zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)
zephyr_include_directories(${ZMK_BASE}/app/drivers)
zephyr_include_directories(${ZEPHYR_KERNEL_BASE}/include)

# Specify the source file
zephyr_library_sources(behavior_cycle_string.c)
message(STATUS "Source files: ${CMAKE_CURRENT_SOURCE_DIR}/behavior_cycle_string.c")

# Link against the 'app' target and potentially the 'zmk-app' target directly
zephyr_library_link_libraries(app)
message(STATUS "Linking against: app")
