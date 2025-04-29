zephyr_library()

set(ZMK_BASE "/build/source")

zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

zephyr_library_sources(behavior_cycle_string.c)

# Link against the main ZMK app library to access HID functions etc.
# zephyr_library_link_libraries(app)
zephyr_interface_link_libraries(zmk)

