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
// #include <zmk/split.h> // No longer needed for event-based approach

// Helper to tap a usage ID by raising keycode state changed events
static inline void tap_usage(uint32_t usage) {
    // Prepare key down event data
    struct zmk_keycode_state_changed press_event = {
        .usage_page = HID_USAGE_KEY, // Assuming standard keyboard usage page
        .keycode = usage,
        .state = true, // Pressed
        .timestamp = k_uptime_get()
    };
    // Raise key down event using the event manager helper function
    int ret_down = raise_zmk_keycode_state_changed(press_event);
    if (ret_down != 0) {
        LOG_ERR("Failed to raise key down event: %d", ret_down);
    }

    // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if needed

    // Prepare key up event data
    struct zmk_keycode_state_changed release_event = {
        .usage_page = HID_USAGE_KEY, // Assuming standard keyboard usage page
        .keycode = usage,
        .state = false, // Released
        .timestamp = k_uptime_get()
    };
    // Raise key up event using the event manager helper function
    int ret_up = raise_zmk_keycode_state_changed(release_event);
    if (ret_up != 0) {
        LOG_ERR("Failed to raise key up event: %d", ret_up);
    }

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

// Define the different lists of strings to cycle through
static const char *work_strings[] = {"work", "working"};
static const char *test_strings[] = {"test", "testing", "tested"};
// Add more lists here as needed...
// static const char *another_list[] = {"one", "two", "three"};

// Structure to hold a cycle list and its length
typedef struct {
    const char **strings;
    const size_t len;
} cycle_list_t;

// Array containing all the cycle lists
static const cycle_list_t all_cycle_lists[] = {
    {.strings = work_strings, .len = ARRAY_SIZE(work_strings)},
    {.strings = test_strings, .len = ARRAY_SIZE(test_strings)},
    // Add corresponding entries for new lists here...
    // {.strings = another_list, .len = ARRAY_SIZE(another_list)},
};
static const size_t all_cycle_lists_len = ARRAY_SIZE(all_cycle_lists);

// State for the behavior instance
struct behavior_cycle_string_state {
  uint8_t current_index;
  bool active; // Track if the behavior is currently active (pressed)
  uint32_t last_list_index; // Track which list was last used
};

// Configuration structure is no longer needed as list is selected by param1
// struct behavior_cycle_string_config {};

// Initialize the behavior
static int behavior_cycle_string_init(const struct device *dev) {
  struct behavior_cycle_string_state *state = dev->data;
  state->current_index = 0;
  state->active = false;
  state->last_list_index = UINT32_MAX; // Initialize with an invalid index
  return 0;
};

// Behavior press handler
static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  struct behavior_cycle_string_state *state = dev->data;
  // Get the list index from the binding's parameter
  uint32_t list_index = binding->param1;

  if (list_index >= all_cycle_lists_len) {
      LOG_ERR("Invalid list index %d for behavior %s", list_index, binding->behavior_dev);
      return ZMK_BEHAVIOR_OPAQUE; // Consume event but do nothing
  }

  const cycle_list_t *current_list = &all_cycle_lists[list_index];

  LOG_DBG("Cycle string '%s' (list %d) pressed, current string index: %d", binding->behavior_dev, list_index,
          state->current_index);

  // 1. Add backspaces for the PREVIOUS string in the *selected list*, only if not the first press of a
  // cycle
  //    Note: This assumes the user hasn't typed anything else between chord
  //    presses. Resetting active on release helps with this.
  bool is_new_cycle_sequence = (list_index != state->last_list_index);

  if (is_new_cycle_sequence) {
      LOG_DBG("New cycle sequence started for list %d.", list_index);
      state->current_index = 0; // Start from the beginning of the new list
      // state->active remains false until after typing
  }

  // Only backspace if it's a repeat press of the *same* cycle sequence
  if (state->active && !is_new_cycle_sequence) {
    // Calculate the index of the string that was *just* typed from the selected list
    uint8_t previous_index =
        (state->current_index + current_list->len - 1) % current_list->len;
    const char *prev_string = current_list->strings[previous_index];
    size_t prev_len = strlen(prev_string) + 1; // Add 1 for the space
    LOG_DBG("Backspacing previous string + space: '%s ' (length %zu)", prev_string, prev_len);
    // Use the tap_usage helper
    for (size_t i = 0; i < prev_len; ++i) {
      tap_usage(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE);
      // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if
      // needed
    }
  } else {
    LOG_DBG("First press in cycle, no backspace needed.");
  }

  // 2. Add key taps for the CURRENT string from the *selected list*
  const char *current_string = current_list->strings[state->current_index];
  size_t current_len = strlen(current_string);
  LOG_DBG("Typing current string: '%s' (length %zu)", current_string, current_len);

  // Use the local helper function to tap out the string
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

  // 3. Add a space after the typed string
  tap_usage(HID_USAGE_KEY_KEYBOARD_SPACEBAR);
  LOG_DBG("Tapped space after string '%s'", current_string);

  // Macro execution happens directly above, no queueing needed.

  // 4. Update state for the next press
  // Always advance the index for the *current* list after typing.
  state->current_index = (state->current_index + 1) % current_list->len;
  state->active = true; // Mark as active *after* potential backspacing and typing
  state->last_list_index = list_index; // Record the list index used this time

  return ZMK_BEHAVIOR_OPAQUE; // Consume the event
}

// Behavior release handler
static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  struct behavior_cycle_string_state *state = dev->data;

  LOG_DBG("Cycle string '%s' released", binding->behavior_dev);

  // Deactivate the behavior on release. This prevents backspacing if another
  // key is pressed before the next cycle combo press.
  // again soon.
  // state->active = false; // REMOVED: Let the press handler manage active state based on sequence
  // We keep state->current_index and state->last_list_index as they were,
  // so the *next* press of the *same* combo knows where it left off.
  // The press handler will check if it's a new sequence or not.

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

// Instantiate the behavior driver data (state only, config removed)
#define CYC_STR_INST(n)                                                        \
  static struct behavior_cycle_string_state behavior_cycle_string_state_##n;   \
  /* Config struct removed */                                                  \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, behavior_cycle_string_init, NULL, /* Deinit function, not needed */   \
      &behavior_cycle_string_state_##n, NULL, /* No config */                  \
      POST_KERNEL, /* Initialize after kernel */                               \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_cycle_string_driver_api);

// Apply the instantiation macro for all device tree nodes matching the
// compatible
DT_INST_FOREACH_STATUS_OKAY(CYC_STR_INST)
