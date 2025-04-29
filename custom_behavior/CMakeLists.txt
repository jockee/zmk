# No separate library needed when adding sources directly to 'app'

set(ZMK_BASE "/build/source") # Assuming this path is correct within the Nix build

# Include necessary ZMK headers
zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

# Add the behavior source file directly to the main 'app' target
# only if the Kconfig option is enabled.
target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_CYCLE_STRING app PRIVATE behavior_cycle_string.c)

# No explicit linking needed as the source is compiled as part of 'app'

