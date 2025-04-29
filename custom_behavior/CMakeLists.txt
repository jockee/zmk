# zmk-config/custom_behavior/CMakeLists.txt

zephyr_library()

# Hardcode the ZMK source path based on Nix build structure
set(ZMK_BASE "/build/source")

# Add necessary ZMK include directories
zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

# Add your behavior source files
zephyr_library_sources(behavior_cycle_string.c)

# Link against the main ZMK app library to access HID functions etc.
zephyr_library_link_libraries(app)

