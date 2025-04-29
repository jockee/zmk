# Define this directory as a Zephyr library
zephyr_library()

set(ZMK_BASE "/build/source") # Assuming this path is correct within the Nix build

# Include necessary ZMK headers
zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

# Specify the source file for this library
zephyr_library_sources(behavior_cycle_string.c)

# Link this library against the main ZMK application library ('app')
# This *should* provide access to core ZMK functions like zmk_hid_press
zephyr_library_link_libraries(app)

