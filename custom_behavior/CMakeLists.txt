zephyr_library()

set(ZMK_BASE "/build/source")

zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

zephyr_library_sources(behavior_cycle_string.c)

# --- THIS IS THE KEY ---
# Tell CMake to attach our behavior after Zephyr app target is created
zephyr_link_libraries_ifdef(CONFIG_ZMK app)

