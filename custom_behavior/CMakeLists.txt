# Don't create a separate library for this behavior

set(ZMK_BASE "/build/source")

zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

# Add the source file directly to the main 'app' target
target_sources(app PRIVATE behavior_cycle_string.c)

# No linking needed as sources are compiled directly into 'app'

