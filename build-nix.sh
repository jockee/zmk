#!/usr/bin/env bash

# Script to build the Glove80 ZMK firmware locally using Nix,
# mirroring the GitHub Actions workflow.

set -euo pipefail

# --- Configuration ---
ZMK_REPO="https://github.com/moergo-sc/zmk.git"
ZMK_DIR="src"
# Use the first argument as the branch/tag, default to 'main' (matches GHA checkout)
BRANCH="${1:-main}"
# Pin Nixpkgs to the same channel used in CI
NIX_CHANNEL_URL="https://github.com/NixOS/nixpkgs/archive/nixos-22.05.tar.gz"
# SHA256 for the nixos-22.05 channel as of a specific commit.
# Update this if you need a newer commit from the channel or remove for less purity.
NIX_CHANNEL_SHA256="1k2zm3hz5g5ybbjmf9bbkd3zzg79f9q1z73f3h1z8y1qw16g51q9"
# Cachix configuration (matches CI)
CACHIX_NAME="moergo-glove80-zmk-dev"
CACHIX_URL="https://${CACHIX_NAME}.cachix.org"
CACHIX_PUBKEY="moergo-glove80-zmk-dev.cachix.org-1:k+JG24Z9F7OIoPZj2oLa1cvs7XN/Sfa8jAqkMEoIY0k="
# Build output configuration
RESULT_LINK="combined" # Matches CI nix-build output link name
OUTPUT_FILE="glove80.uf2" # Final firmware file name (matches CI copy step)

# --- Helper Functions ---
fetch_nixpkgs() {
    echo "Fetching Nixpkgs channel: $NIX_CHANNEL_URL" >&2
    # Use nix-prefetch-url to reliably fetch and unpack the tarball
    local nixpkgs_path
    nixpkgs_path=$(nix-prefetch-url --unpack "$NIX_CHANNEL_URL" --name nixpkgs --sha256 "$NIX_CHANNEL_SHA256")
    echo "$nixpkgs_path"
}

# --- Main Script ---

# 1. Ensure ZMK source code is available
if [ ! -d "$ZMK_DIR/.git" ]; then
    echo "Cloning ZMK repository ($ZMK_REPO) into $ZMK_DIR..." >&2
    # Clone only the specified branch/tag initially for speed
    git clone --depth 1 --branch "$BRANCH" "$ZMK_REPO" "$ZMK_DIR"
    # Fetch all tags after shallow clone to allow checking out tags later
    (cd "$ZMK_DIR" && git fetch --tags --depth=100) # Fetch recent tags
else
    echo "ZMK repository found in $ZMK_DIR." >&2
    # Fetch updates and checkout the desired branch/tag
    echo "Checking out branch/tag '$BRANCH' in $ZMK_DIR..." >&2
    (cd "$ZMK_DIR" && git fetch origin --tags && git checkout "$BRANCH" && git pull origin "$BRANCH" --ff-only --depth=100 || git checkout "$BRANCH") # Attempt pull if branch, otherwise just checkout tag/commit
    echo "Using ZMK commit:" >&2
    (cd "$ZMK_DIR" && git rev-parse HEAD)
fi


# 2. Fetch the correct Nixpkgs version
NIXPKGS_PATH=$(fetch_nixpkgs)
echo "Using Nixpkgs from: $NIXPKGS_PATH" >&2

# 3. Inform about Cachix (optional user setup)
echo "INFO: Using Cachix cache '$CACHIX_NAME' for potentially faster builds." >&2
echo "INFO: To configure Cachix globally (optional): cachix use $CACHIX_NAME" >&2

# 4. Run the Nix build command
echo "Building firmware using Nix..." >&2
# Set NIX_PATH to point to the fetched nixpkgs for this command
# Pass Cachix options directly to nix-build (matches GHA)
# Pass the zmk source path via --arg (similar to how Dockerfile does it implicitly)
NIX_PATH="nixpkgs=$NIXPKGS_PATH" \
nix-build config \
    -o "$RESULT_LINK" \
    --arg firmware "import $PWD/$ZMK_DIR {}" \
    --option substituters "https://cache.nixos.org/ $CACHIX_URL" \
    --option trusted-public-keys "cache.nixos.org-1:6NCHdD59X431o0gWypbMrAURkbJ16ZPMQFGspcDShjY= $CACHIX_PUBKEY" \
    --show-trace

# 5. Copy the result firmware file (matches GHA)
echo "Copying result to $OUTPUT_FILE..." >&2
if [ -f "$RESULT_LINK/$OUTPUT_FILE" ]; then
    # Try to copy the file, but handle permission errors
    if ! cp "$LEFT_BUILD_LINK/zmk.uf2" "./$LEFT_OUTPUT_FILE" 2>/dev/null; then
        echo "Permission denied when copying to ./$LEFT_OUTPUT_FILE" >&2

        # Create a user-specific directory in home folder
        USER_FIRMWARE_DIR="$HOME/firmware_output"
        mkdir -p "$USER_FIRMWARE_DIR"

        echo "Trying to copy to $USER_FIRMWARE_DIR/$LEFT_OUTPUT_FILE instead..." >&2

        # Remove existing file if it exists
        if [ -f "$USER_FIRMWARE_DIR/$LEFT_OUTPUT_FILE" ]; then
            rm -f "$USER_FIRMWARE_DIR/$LEFT_OUTPUT_FILE" 2>/dev/null
        fi

        if cp "$LEFT_BUILD_LINK/zmk.uf2" "$USER_FIRMWARE_DIR/$LEFT_OUTPUT_FILE"; then
            echo "LEFT Firmware copied to $USER_FIRMWARE_DIR/$LEFT_OUTPUT_FILE" >&2
            echo "You can flash this file directly or copy it with:" >&2
            echo "cp $USER_FIRMWARE_DIR/$LEFT_OUTPUT_FILE ./$LEFT_OUTPUT_FILE" >&2
        else
            echo "Failed to copy LEFT firmware to alternate location." >&2
            echo "You can access the LEFT firmware directly at:" >&2
            echo "$LEFT_BUILD_LINK/zmk.uf2" >&2
            echo "And copy it manually with:" >&2
            echo "sudo cp $LEFT_BUILD_LINK/zmk.uf2 ./$LEFT_OUTPUT_FILE" >&2
        fi
    else
        echo "Successfully copied LEFT firmware to ./$LEFT_OUTPUT_FILE" >&2
    fi
else
    echo "Error: Built LEFT firmware file not found at $LEFT_BUILD_LINK/zmk.uf2" >&2
    exit 1
fi

# 6. Clean up the result symlink
rm "$RESULT_LINK"

# 7. Copy the RIGHT result firmware file
echo "Copying RIGHT result to $RIGHT_OUTPUT_FILE..." >&2
if [ -f "$RIGHT_BUILD_LINK/zmk.uf2" ]; then
    if ! cp "$RIGHT_BUILD_LINK/zmk.uf2" "./$RIGHT_OUTPUT_FILE" 2>/dev/null; then
        echo "Permission denied when copying to ./$RIGHT_OUTPUT_FILE" >&2
        USER_FIRMWARE_DIR="$HOME/firmware_output" # Ensure dir exists from left build
        mkdir -p "$USER_FIRMWARE_DIR"
        echo "Trying to copy to $USER_FIRMWARE_DIR/$RIGHT_OUTPUT_FILE instead..." >&2
        if [ -f "$USER_FIRMWARE_DIR/$RIGHT_OUTPUT_FILE" ]; then
            rm -f "$USER_FIRMWARE_DIR/$RIGHT_OUTPUT_FILE" 2>/dev/null
        fi
        if cp "$RIGHT_BUILD_LINK/zmk.uf2" "$USER_FIRMWARE_DIR/$RIGHT_OUTPUT_FILE"; then
            echo "RIGHT Firmware copied to $USER_FIRMWARE_DIR/$RIGHT_OUTPUT_FILE" >&2
            echo "You can flash this file directly or copy it with:" >&2
            echo "cp $USER_FIRMWARE_DIR/$RIGHT_OUTPUT_FILE ./$RIGHT_OUTPUT_FILE" >&2
        else
            echo "Failed to copy RIGHT firmware to alternate location." >&2
            echo "You can access the RIGHT firmware directly at:" >&2
            echo "$RIGHT_BUILD_LINK/zmk.uf2" >&2
            echo "And copy it manually with:" >&2
            echo "sudo cp $RIGHT_BUILD_LINK/zmk.uf2 ./$RIGHT_OUTPUT_FILE" >&2
        fi
    else
        echo "Successfully copied RIGHT firmware to ./$RIGHT_OUTPUT_FILE" >&2
    fi
else
    echo "Error: Built RIGHT firmware file not found at $RIGHT_BUILD_LINK/zmk.uf2" >&2
    exit 1
fi

# 8. Clean up the result symlinks
rm "$LEFT_BUILD_LINK" "$RIGHT_BUILD_LINK"

echo "Build complete. Firmware files:"
echo "  Left: $LEFT_OUTPUT_FILE"
echo "  Right: $RIGHT_OUTPUT_FILE"
