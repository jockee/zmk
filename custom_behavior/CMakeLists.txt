# zmk-config/custom_behavior/CMakeLists.txt

zephyr_library()

# Hardcode the ZMK source path based on Nix build structure
set(ZMK_BASE "/build/source")

# Add necessary ZMK include directories
zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

zephyr_library_sources(${ZMK_BASE}/app/src/hid.c)

zephyr_library_link_libraries(${ZMK_BASE}/app/src)

# Add your behavior source files
zephyr_library_sources(behavior_cycle_string.c)


