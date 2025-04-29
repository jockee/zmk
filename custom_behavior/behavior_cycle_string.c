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

#include <drivers/behavior.h> // Main behavior header
#include <zmk/behavior.h>     // Main behavior header
// #include <zmk/behavior/macros.h> // New macro helpers - Removed
#include <zmk/keys.h> // For key definitions like MOD_LSFT

// Required includes for behavior functionality
// #include <zmk/behavior_queue.h> // No longer needed
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h> // For keycode events used by macros
#include <zmk/events/modifiers_state_changed.h> // For modifier events used by macros
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h> // For HID usage IDs and helper functions
#include <zmk/keymap.h>
#include <zmk/split.h> // For split role detection

// Helper to tap a usage ID
static inline void tap_usage(uint32_t usage) {
    // Raise key down event
    ZMK_EVENT_INIT(
        struct zmk_keycode_state_changed, key_down_ev,
        ((struct zmk_keycode_state_changed){
            .usage_page = HID_USAGE_KEY, // Assuming standard keyboard usage page
            .keycode = usage,
            .state = true, // Pressed
            .timestamp = k_uptime_get()}));
    ZMK_EVENT_RAISE(key_down_ev);

    // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if needed

    // Raise key up event
    ZMK_EVENT_INIT(
        struct zmk_keycode_state_changed, key_up_ev,
        ((struct zmk_keycode_state_changed){
            .usage_page = HID_USAGE_KEY, // Assuming standard keyboard usage page
            .keycode = usage,
            .state = false, // Released
            .timestamp = k_uptime_get()}));
    ZMK_EVENT_RAISE(key_up_ev);

    // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_TAP_MS); // Add delay if needed
}

// Simple ASCII to keycode helper (add more mappings as needed)
// Returns 0 if no mapping found
static zmk_key_t ascii_to_keycode(char character) {
  // Basic lowercase letters
  if (character >= 'a' && character <= 'z') {
    return HID_USAGE_KEY_KEYBOARD_A + (character - 'a');
  }
  // Add other mappings here if needed (numbers, symbols, etc.)
  // Example:
  // if (character >= '1' && character <= '9') {
  //     return HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION + (character - '1');
  // }
  // if (character == '0') {
  //     return HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS;
  // }
  // if (character == ' ') {
  //     return HID_USAGE_KEY_KEYBOARD_SPACEBAR;
  // }

  return 0; // No mapping found
}

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
  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
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
    // Use the tap_usage helper
    for (size_t i = 0; i < prev_len; ++i) {
      tap_usage(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE);
      // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if
      // needed
    }
  } else {
    LOG_DBG("First press in cycle, no backspace needed.");
  }

  // 2. Add key taps for the CURRENT string
  const char *current_string = cycle_strings[state->current_index];
  size_t current_len = strlen(current_string);
  LOG_DBG("Typing current string: '%s' (length %zu)", current_string,
          current_len);

  // Use the local helper function
  for (size_t i = 0; i < current_len; ++i) {
    zmk_key_t keycode = ascii_to_keycode(current_string[i]);
    if (keycode == 0) { // Check for 0, which indicates no mapping found
      LOG_ERR("Cannot map character '%c' to keycode", current_string[i]);
      continue; // Skip character if no mapping
    }
    // Directly tap the keycode, as shift handling is not needed for current
    // strings
    tap_usage(keycode);
    // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if
    // needed
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
  // const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  // // dev not needed struct behavior_cycle_string_state *state = dev->data; //
  // state not needed

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
