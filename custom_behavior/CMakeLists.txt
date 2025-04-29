zephyr_library()

set(ZMK_BASE "/build/source")

zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

zephyr_library_sources(behavior_cycle_string.c)

# Link against the Zephyr USB HID interface library
zephyr_library_link_libraries(zephyr::usbd_hid)

