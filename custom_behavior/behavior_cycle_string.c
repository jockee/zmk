/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Based on behavior_caps_word.c
 */

#define DT_DRV_COMPAT chord_cycle_behavior_cycle_string

#include <zephyr/device.h>
#include <zephyr/kernel.h> // Include kernel for basic types if needed elsewhere
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/behavior.h> // Main behavior header
// #include <zmk/behavior/macros.h> // New macro helpers - Removed
#include <zmk/keys.h>            // For key definitions like MOD_LSFT

// Required includes for behavior functionality
// #include <zmk/behavior_queue.h> // No longer needed
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h> // For keycode events used by macros
#include <zmk/events/modifiers_state_changed.h> // For modifier events used by macros
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h> // For zmk_hid_get_keycode_t, HID usage IDs
#include <zmk/keymap.h>

// Define the strings to cycle through
static const char *cycle_strings[] = {"work", "working"};
static const size_t cycle_strings_len =
    sizeof(cycle_strings) / sizeof(cycle_strings[0]);

// State for the behavior instance
struct behavior_cycle_string_state {
  uint8_t current_index;
  bool active; // Track if the behavior is currently active (pressed)
};

// Configuration structure (empty for this simple behavior)
struct behavior_cycle_string_config {};

// Initialize the behavior
static int behavior_cycle_string_init(const struct device *dev) {
  // Initialize state if needed (already implicitly zeroed)
  return 0;
};

// Behavior press handler
static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  const struct device *dev = zmk_behavior_get_driver(binding->behavior_dev);
  struct behavior_cycle_string_state *state = dev->data;
  // const struct behavior_cycle_string_config *config = dev->config; // Not
  // used here

  LOG_DBG("Cycle string '%s' pressed, current index: %d", binding->behavior_dev,
          state->current_index);

  // 1. Add backspaces for the PREVIOUS string, only if not the first press of a
  // cycle
  //    Note: This assumes the user hasn't typed anything else between chord
  //    presses. A more robust implementation might need more complex state
  //    tracking.
  if (state->active) { // Only backspace if it was already active (i.e., repeat
                       // press)
    // Calculate the index of the string that was *just* typed
    uint8_t previous_index =
        (state->current_index + cycle_strings_len - 1) % cycle_strings_len;
    size_t prev_len = strlen(cycle_strings[previous_index]);
    LOG_DBG("Backspacing previous string: '%s' (length %zu)",
            cycle_strings[previous_index], prev_len);
    // Use standard ZMK key tap function
    for (size_t i = 0; i < prev_len; ++i) {
        zmk_keys_tap(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE);
        // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if needed
    }
  } else {
    LOG_DBG("First press in cycle, no backspace needed.");
  }

  // 2. Add key taps for the CURRENT string
  const char *current_string = cycle_strings[state->current_index];
  size_t current_len = strlen(current_string);
  LOG_DBG("Typing current string: '%s' (length %zu)", current_string,
          current_len);

  // Use new macro helpers directly
  for (size_t i = 0; i < current_len; ++i) {
    zmk_key_t keycode = zmk_hid_ascii_to_keycode(current_string[i]);
    if (keycode == ZMK_HID_NO_KEYCODE) {
      LOG_ERR("Cannot map character '%c' to keycode", current_string[i]);
      continue; // Skip character if no mapping
    }
    // Check if it's an uppercase character or symbol requiring shift
    if (zmk_hid_requires_shift(keycode)) {
        zmk_hid_register_mods(MOD_LSFT); // Press Shift
        zmk_keys_tap(zmk_hid_apply_basic_mods(keycode)); // Tap the base key
        zmk_hid_register_mods(0); // Release Shift (reset all mods)
    } else {
        zmk_keys_tap(zmk_hid_strip_mods(keycode)); // Tap the base key
    }
    // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if needed
  }

  // Macro execution happens directly above, no queueing needed.

  // 4. Update state for the next press
  state->current_index = (state->current_index + 1) % cycle_strings_len;
  state->active = true; // Mark as active

  return ZMK_BEHAVIOR_OPAQUE; // Consume the event
}

// Behavior release handler
static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  const struct device *dev = zmk_behavior_get_driver(binding->behavior_dev);
  struct behavior_cycle_string_state *state = dev->data;

  LOG_DBG("Cycle string '%s' released", binding->behavior_dev);

  // Deactivate the behavior. We reset the index on release.
  // Alternatively, you could add a timer to reset the index if not pressed
  // again soon. state->active = false; // Resetting index on release might be
  // simpler state->current_index = 0;

  // For now, let's keep the index persistent until the next *different* key is
  // pressed or a timeout occurs (timeout not implemented here). The
  // `state->active` flag helps differentiate the first press from subsequent
  // ones.

  // No queued actions to release with the new macro API

  return ZMK_BEHAVIOR_OPAQUE; // Consume the event
}

// Behavior driver API definition
static const struct behavior_driver_api behavior_cycle_string_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    // .locality_changed = NULL, // Not needed for this behavior
};

// Instantiate the behavior driver data and config
#define CYC_STR_INST(n)                                                        \
  static struct behavior_cycle_string_state behavior_cycle_string_state_##n;   \
  static const struct behavior_cycle_string_config                             \
      behavior_cycle_string_config_##n;                                        \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, behavior_cycle_string_init, NULL, /* Deinit function, not needed */   \
      &behavior_cycle_string_state_##n, &behavior_cycle_string_config_##n,     \
      POST_KERNEL, /* Initialize after kernel */                               \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_cycle_string_driver_api);

// Apply the instantiation macro for all device tree nodes matching the
// compatible
DT_INST_FOREACH_STATUS_OKAY(CYC_STR_INST)
