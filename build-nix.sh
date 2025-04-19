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
    # Using fetchTarball ensures we get the specific channel version
    local nixpkgs_path
    nixpkgs_path=$(nix-build --no-out-link "<nixpkgs>" -A fetchTarball --argstr url "$NIX_CHANNEL_URL" --argstr sha256 "$NIX_CHANNEL_SHA256")
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
    if ! cp "$RESULT_LINK/$OUTPUT_FILE" "./$OUTPUT_FILE" 2>/dev/null; then
        echo "Permission denied when copying to ./$OUTPUT_FILE" >&2
        echo "Trying to copy to /tmp/$OUTPUT_FILE instead..." >&2
        cp "$RESULT_LINK/$OUTPUT_FILE" "/tmp/$OUTPUT_FILE"
        echo "Firmware copied to /tmp/$OUTPUT_FILE" >&2
        echo "You can copy it from there with: sudo cp /tmp/$OUTPUT_FILE ./$OUTPUT_FILE" >&2
    else
        echo "Successfully copied firmware to ./$OUTPUT_FILE" >&2
    fi
else
    echo "Error: Built firmware file not found at $RESULT_LINK/$OUTPUT_FILE" >&2
    exit 1
fi

# 6. Clean up the result symlink
rm "$RESULT_LINK"

echo "Build complete: $OUTPUT_FILE"
