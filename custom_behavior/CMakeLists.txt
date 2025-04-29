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

# Add the behavior source file directly to the main 'app' target
# only if the Kconfig option is enabled.
target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_CYCLE_STRING app PRIVATE behavior_cycle_string.c)
message(STATUS "Adding source ${CMAKE_CURRENT_SOURCE_DIR}/behavior_cycle_string.c to app target")

# No explicit linking needed as the source is compiled as part of 'app'
