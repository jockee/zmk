zephyr_library()

set(ZMK_BASE "/build/source")

zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

zephyr_library_sources(behavior_cycle_string.c)

# Link against the core Zephyr interface library
# This might provide access to lower-level HID functions or ensure correct linking order.
zephyr_library_link_libraries(zephyr_interface)

