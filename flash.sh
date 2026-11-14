#!/usr/bin/env bash
# Hands-off Glove80 flasher for macOS and Linux (NixOS).
#
#   ./flash.sh          wait for the LEFT half in bootloader mode, flash it, exit
#   ./flash.sh --both   flash left and right (either order), then exit
#
# Start it, then put a half into bootloader mode (hold Magic + tap Esc for the
# left half; the keyboard must be on USB to this machine). The script copies
# glove80.uf2 as soon as the bootloader drive shows up; the half reboots itself.
#
# macOS: never eject the bootloader drive, that wedges macOS until a restart.
# Linux: the drive is mounted with udisksctl. A polkit rule in the NixOS config
#        lets joakimekstrom mount GLV80LHBOOT/GLV80RHBOOT without a password,
#        also over SSH.

set -uo pipefail

UF2="$(cd "$(dirname "$0")" && pwd)/glove80.uf2"
TIMEOUT="${FLASH_TIMEOUT:-900}"
OS="$(uname -s)"
want_lh=1
want_rh=0
[[ "${1:-}" == "--both" ]] && want_rh=1

say_it() {
  echo "$(date +%H:%M:%S) $1"
  [[ -n "${FLASH_QUIET:-}" ]] && return 0 # terminal output only, no pop-ups
  if [[ "$OS" == Darwin ]]; then
    if command -v hs >/dev/null 2>&1; then
      hs -c "hs.alert.closeAll(0); hs.alert.show([[$1]], 4)" >/dev/null 2>&1 || true
    fi
  elif command -v notify-send >/dev/null 2>&1; then
    # Reach the desktop session's notification daemon even when run over SSH.
    DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$(id -u)/bus}" \
      notify-send -t 4000 "Glove80" "$1" >/dev/null 2>&1 || true
  fi
}

usb_has() { # $1 = regex matched against USB product names
  if [[ "$OS" == Darwin ]]; then
    ioreg -p IOUSB -w0 -l 2>/dev/null | grep -Eq "\"USB Product Name\" = \"$1\""
  else
    cat /sys/bus/usb/devices/*/product 2>/dev/null | grep -Eqx "$1"
  fi
}

# True while the bootloader drive of a half is attached.
present() {
  if [[ "$OS" == Darwin ]]; then
    mount | grep -q " on /Volumes/$1 "
  else
    [[ -e "/dev/disk/by-label/$1" ]]
  fi
}

# Print the directory the bootloader drive is mounted at, mounting it if needed.
mount_point() {
  if [[ "$OS" == Darwin ]]; then
    echo "/Volumes/$1"
    return 0
  fi
  local dev="/dev/disk/by-label/$1" mp out
  mp="$(findmnt -n -o TARGET "$dev" 2>/dev/null | head -1)"
  if [[ -z "$mp" ]]; then
    out="$(udisksctl mount -b "$dev" --no-user-interaction 2>&1)" || { echo "$out" >&2; return 1; }
    mp="$(findmnt -n -o TARGET "$dev" 2>/dev/null | head -1)"
  fi
  [[ -n "$mp" ]] || { echo "could not find where $dev is mounted" >&2; return 1; }
  echo "$mp"
}

flash() { # $1 = volume name, $2 = human name
  local vol="$1" name="$2" mp i
  say_it "$name half found. Flashing..."
  sleep 1 # let the FAT volume settle
  mp="$(mount_point "$vol")" || { say_it "$name half: could not mount the bootloader drive."; return 1; }
  # cp often reports an error at the very end because the board reboots as soon
  # as the last block lands, so success is judged by the drive disappearing.
  if [[ "$OS" == Darwin ]]; then
    # -X: skip xattrs (the Docker-built file carries some; FAT can't store them).
    cp -X "$UF2" "$mp/" 2>/tmp/glove80-flash-cp.err
  else
    cp "$UF2" "$mp/" 2>/tmp/glove80-flash-cp.err
    sync 2>/dev/null || true
  fi
  for i in $(seq 1 40); do
    if ! present "$vol"; then
      say_it "$name half flashed. It restarts by itself."
      return 0
    fi
    sleep 0.5
  done
  say_it "$name half: copy finished but the drive is still there. Flash failed?"
  cat /tmp/glove80-flash-cp.err
  return 1
}

[[ -f "$UF2" ]] || { echo "No firmware at $UF2. Build it first (./build.sh)."; exit 1; }
echo "Firmware: $UF2 ($(date -r "$UF2" '+%Y-%m-%d %H:%M'))"

if [[ "$OS" == Darwin ]]; then
  for vol in GLV80LHBOOT GLV80RHBOOT; do
    if [[ -e "/Volumes/$vol" ]] && ! present "$vol"; then
      echo "Stale /Volumes/$vol folder (not a real mount). Remove it with: sudo rmdir /Volumes/$vol"
      exit 1
    fi
  done
fi

say_it "Ready. Put the left half in bootloader mode."
start=$SECONDS
seen_boot=0
while (( want_lh || want_rh )); do
  if (( SECONDS - start > TIMEOUT )); then
    say_it "Gave up waiting for the Glove80 bootloader."
    exit 2
  fi
  if (( want_lh )) && present GLV80LHBOOT; then
    flash GLV80LHBOOT Left || exit 1
    want_lh=0
    (( want_rh )) && say_it "Now put the right half in bootloader mode."
    continue
  fi
  if (( want_rh )) && present GLV80RHBOOT; then
    flash GLV80RHBOOT Right || exit 1
    want_rh=0
    (( want_lh )) && say_it "Now put the left half in bootloader mode."
    continue
  fi
  if (( ! seen_boot )) && usb_has "Glove80 .H Bootloader"; then
    seen_boot=1
    echo "$(date +%H:%M:%S) Bootloader seen on USB, waiting for its drive..."
  fi
  sleep 0.5
done

# Confirm the keyboard came back (only visible when it is on USB).
for i in $(seq 1 30); do
  if usb_has "Glove80 Left"; then
    say_it "Glove80 is back. Done."
    exit 0
  fi
  sleep 0.5
done
echo "Flashed. Keyboard not seen on USB again within 15 s (fine if it is on Bluetooth)."
exit 0
