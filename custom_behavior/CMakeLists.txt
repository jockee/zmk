# No separate library needed when adding sources directly to 'app'

set(ZMK_BASE "/build/source") # Assuming this path is correct within the Nix build

# Include necessary ZMK headers
zephyr_include_directories(${ZMK_BASE}/include)
zephyr_include_directories(${ZMK_BASE}/app/include)

