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
#include <zmk/keys.h> // For key definitions and modifiers
// #include <zmk/split.h> // No longer needed for event-based approach
#include "cycle_string_lists.h"

// Helper to tap a usage ID by raising keycode state changed events
static inline void tap_usage(uint32_t usage) {
  // Prepare key down event data
  struct zmk_keycode_state_changed press_event = {
      .usage_page = HID_USAGE_KEY, // Assuming standard keyboard usage page
      .keycode = usage,
      .state = true, // Pressed
      .timestamp = k_uptime_get()};
  // Raise key down event using the event manager helper function
  int ret_down = raise_zmk_keycode_state_changed(press_event);
  if (ret_down != 0) {
    LOG_ERR("Failed to raise key down event: %d", ret_down);
  }

  // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS); // Add delay if
  // needed

  // Prepare key up event data
  struct zmk_keycode_state_changed release_event = {
      .usage_page = HID_USAGE_KEY, // Assuming standard keyboard usage page
      .keycode = usage,
      .state = false, // Released
      .timestamp = k_uptime_get()};
  // Raise key up event using the event manager helper function
  int ret_up = raise_zmk_keycode_state_changed(release_event);
  if (ret_up != 0) {
    LOG_ERR("Failed to raise key up event: %d", ret_up);
  }

  // Optional: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_TAP_MS); // Add delay if needed
}

// Helper to press Left Shift
static inline void press_shift() {
    struct zmk_keycode_state_changed shift_press = {
        .usage_page = HID_USAGE_KEY,
        .keycode = HID_USAGE_KEY_KEYBOARD_LEFTSHIFT, // Or RIGHTSHIFT if preferred
        .state = true,
        .timestamp = k_uptime_get()};
    raise_zmk_keycode_state_changed(shift_press);
}

// Helper to release Left Shift
static inline void release_shift() {
    struct zmk_keycode_state_changed shift_release = {
        .usage_page = HID_USAGE_KEY,
        .keycode = HID_USAGE_KEY_KEYBOARD_LEFTSHIFT, // Match the keycode used in press_shift
        .state = false,
        .timestamp = k_uptime_get()};
    raise_zmk_keycode_state_changed(shift_release);
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
  if (character == '\'') {
    // Map apostrophe to the keycode determined for the specific layout (e.g.,
    // Backslash on SE)
    return HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE;
  }
  // Add the mapping for period below
  if (character == '.') {
    return HID_USAGE_KEY_KEYBOARD_PERIOD_AND_GREATER_THAN;
  }
  // Map numbers
  if (character >= '1' && character <= '9') {
      return HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION + (character - '1');
  }
  if (character == '0') {
      return HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS;
  }


  return 0; // No mapping found
}

// State for the behavior instance
struct behavior_cycle_string_state {
  uint8_t current_index;
  bool active; // Track if the behavior is currently active (pressed)
  uint32_t last_list_index; // Track which list was last used
};

// Configuration structure (empty, but needed for the driver definition macro)
struct behavior_cycle_string_config {};

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
    LOG_ERR("Invalid list index %d for behavior %s", list_index,
            binding->behavior_dev);
    return ZMK_BEHAVIOR_OPAQUE; // Consume event but do nothing
  }

  const cycle_list_t *current_list = &all_cycle_lists[list_index];

  LOG_DBG("Cycle string '%s' (list %d) pressed, current string index: %d",
          binding->behavior_dev, list_index, state->current_index);

  // 1. Add backspaces for the PREVIOUS string in the *selected list*, only if
  // not the first press of a cycle
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
    // Calculate the index of the string that was *just* typed from the selected
    // list
    uint8_t previous_index =
        (state->current_index + current_list->len - 1) % current_list->len;
    const char *prev_string = current_list->strings[previous_index];
    size_t prev_len = strlen(prev_string) + 1; // Add 1 for the space
    LOG_DBG("Backspacing previous string + space: '%s ' (length %zu)",
            prev_string, prev_len);
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
  LOG_DBG("Typing current string: '%s' (length %zu)", current_string,
          current_len);

  // Tap out the current string, handling multi-byte UTF-8 and special chars
  size_t i = 0;
  while (i < current_len) {
    char byte1 = current_string[i];
    zmk_key_t keycode = 0;
    bool special_handled =
        false; // Flag to track if character was handled specially

    // Check for 2-byte UTF-8 sequences (specifically for å, ä, ö)
    if ((byte1 & 0xE0) == 0xC0) { // Potential start of 2-byte sequence
      if (i + 1 < current_len) {
        char byte2 = current_string[i + 1];
        if (byte1 == 0xC3) {   // Common first byte for Latin Extended-A
          if (byte2 == 0xA5) { // å (0xC3 0xA5) -> maps to LBKT ([) on SE layout
            keycode = HID_USAGE_KEY_KEYBOARD_LEFT_BRACKET_AND_LEFT_BRACE;
            LOG_DBG("Mapping UTF-8 'å' to LBKT");
          } else if (byte2 ==
                     0xA4) { // ä (0xC3 0xA4) -> maps to SQT (') on SE layout
            keycode = HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE;
            LOG_DBG("Mapping UTF-8 'ä' to SQT");
          } else if (byte2 ==
                     0xB6) { // ö (0xC3 0xB6) -> maps to SEMI (;) on SE layout
            keycode = HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON;
            LOG_DBG("Mapping UTF-8 'ö' to SEMI");
          }
          // Add other C3 sequences here if needed

          if (keycode != 0) {
            tap_usage(keycode);
            i += 2; // Consume both bytes
            special_handled = true;
          } else {
            // Unknown C3 sequence, treat as error or skip? Skipping for now.
            LOG_ERR("Unknown UTF-8 sequence: 0xC3 0x%02X", byte2);
            i += 2;                 // Skip both bytes
            special_handled = true; // Mark as handled to prevent further
                                    // processing this iteration
          }
        } else {
          // Other 2-byte sequences (not starting with C3) - skip for now
          LOG_ERR("Unsupported 2-byte UTF-8 sequence start: 0x%02X", byte1);
          i += 2; // Skip both bytes
          special_handled = true;
        }
      } else {
        // Incomplete 2-byte sequence at end of string
        LOG_ERR("Incomplete UTF-8 sequence at end of string");
        i++; // Move past the first byte
        special_handled = true;
      }
    }
    // Check for '@' symbol (requires Right Alt on SE layout)
    else if (byte1 == '@') {
      struct zmk_keycode_state_changed ralt_press = {
          .usage_page = HID_USAGE_KEY,
          .keycode = HID_USAGE_KEY_KEYBOARD_RIGHTALT,
          .state = true,
          .timestamp = k_uptime_get()};
      raise_zmk_keycode_state_changed(ralt_press);

      tap_usage(HID_USAGE_KEY_KEYBOARD_2_AND_AT); // Tap '2'

      struct zmk_keycode_state_changed ralt_release = {
          .usage_page = HID_USAGE_KEY,
          .keycode = HID_USAGE_KEY_KEYBOARD_RIGHTALT,
          .state = false,
          .timestamp = k_uptime_get()};
      raise_zmk_keycode_state_changed(ralt_release);

      i++; // Consume the '@' character
      special_handled = true;
      // Optional delay: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS);
    }
    // Add handling for Colon (:) - Requires Shift + Semicolon
    else if (byte1 == ':') {
        press_shift();
        tap_usage(HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON);
        release_shift();
        i++; // Consume the ':' character
        special_handled = true;
        LOG_DBG("Mapped ':' to Shift + SEMI");
        // Optional delay: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS);
    }
    // Add handling for Plus (+) - Requires Shift + Slash key on SE layout
    else if (byte1 == '+') {
        press_shift();
        // Assuming SE layout where '+' is Shift + the key physically labeled "+ ? \"
        // which corresponds to HID_USAGE_KEY_KEYBOARD_SLASH_AND_QUESTION_MARK
        tap_usage(HID_USAGE_KEY_KEYBOARD_SLASH_AND_QUESTION_MARK);
        release_shift();
        i++; // Consume the '+' character
        special_handled = true;
        LOG_DBG("Mapped '+' to Shift + SLASH");
        // Optional delay: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS);
    }


    // If not a special UTF-8 sequence, '@', ':', or '+' handle as standard ASCII/Number
    if (!special_handled) {
      keycode = ascii_to_keycode(byte1);
      if (keycode != 0) {
        tap_usage(keycode);
      } else {
        LOG_ERR("Cannot map character '%c' (0x%02X) to keycode", byte1, byte1);
        // Optionally handle the error differently, e.g., skip character
      }
      i++; // Consume the single byte
    }
    // Optional delay between characters/sequences if needed
    // k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS);
  }

  // 3. Add a space after the typed string
  tap_usage(HID_USAGE_KEY_KEYBOARD_SPACEBAR);
  LOG_DBG("Tapped space after string '%s'", current_string);

  // Macro execution happens directly above, no queueing needed.

  // 4. Update state for the next press
  // Always advance the index for the *current* list after typing.
  state->current_index = (state->current_index + 1) % current_list->len;
  state->active =
      true; // Mark as active *after* potential backspacing and typing
  state->last_list_index = list_index; // Record the list index used this time

  return ZMK_BEHAVIOR_OPAQUE; // Consume the event
}

// Behavior release handler
static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
  const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
  // Removed unused state variable

  LOG_DBG("Cycle string '%s' released", binding->behavior_dev);

  // Deactivate the behavior on release. This prevents backspacing if another
  // key is pressed before the next cycle combo press.
  // again soon.
  // state->active = false; // REMOVED: Let the press handler manage active
  // state based on sequence We keep state->current_index and
  // state->last_list_index as they were, so the *next* press of the *same*
  // combo knows where it left off. The press handler will check if it's a new
  // sequence or not.

  // For now, let's keep the index persistent until the next *different* key is
  // pressed or a timeout occurs (timeout not implemented here). The
  // `state->active` flag helps differentiate the first press from subsequent
  // ones.

  // No queued actions to release with the new macro API

  return ZMK_BEHAVIOR_OPAQUE; // Consume the event
}

// Helper function to reset the state of a single behavior instance
static void reset_instance_state(const struct device *dev) {
  struct behavior_cycle_string_state *state = dev->data;
  if (state->active || state->last_list_index != UINT32_MAX) {
    // LOG_DBG("Resetting state for instance %s", dev->name); // Optional
    // logging
    state->active = false;
    state->last_list_index = UINT32_MAX;
  }
}

// Global listener to reset cycle state on any key press
static int cycle_string_keycode_state_changed_listener(const zmk_event_t *eh) {
  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev == NULL || !ev->state) { // Only act on key presses, not releases
    return ZMK_EV_EVENT_BUBBLE;
  }

  // Check if any instance was active *before* potentially resetting
  bool any_instance_was_active = false;
#define CHECK_ACTIVE_STATE(n)                                                  \
  do {                                                                         \
    const struct device *dev_check = DEVICE_DT_GET(DT_DRV_INST(n));            \
    struct behavior_cycle_string_state *state_check = dev_check->data;         \
    if (state_check->active) {                                                 \
      any_instance_was_active = true;                                          \
      /* Cannot easily break from DT_INST_FOREACH, so check all */             \
    }                                                                          \
  } while (0)
  DT_INST_FOREACH_STATUS_OKAY(CHECK_ACTIVE_STATE);
#undef CHECK_ACTIVE_STATE

  // Define punctuation keycodes (HID Usages) - Add more as needed
  // Define punctuation keycodes (HID Usages) that DON'T require special shift handling here
  // These are the *base* keys that, when pressed *without* shift, might trigger space replacement.
  zmk_key_t punctuation_keys[] = {
      HID_USAGE_KEY_KEYBOARD_PERIOD_AND_GREATER_THAN, // .
      HID_USAGE_KEY_KEYBOARD_COMMA_AND_LESS_THAN,     // ,
      HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON,     // ; (Base for :)
      // Keys requiring explicit Shift or special handling are checked separately below:
      // !, ?, ', :, +
      // Note: Base key for '+' (SLASH on SE) is handled as a special case below, not in this array.
      // HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE, // Removed, handled below
      // HID_USAGE_KEY_KEYBOARD_SLASH_AND_QUESTION_MARK, // Removed, handled
      // below HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION,       // Removed,
      // handled below
  };

  // Check if the event keycode is punctuation
  bool is_punctuation = false;
  for (size_t i = 0; i < ARRAY_SIZE(punctuation_keys); ++i) {
    if (ev->keycode == punctuation_keys[i]) {
      is_punctuation = true;
      break;
    }
  }

  // Special handling flags for keys requiring explicit Shift or direct tapping
  bool is_exclamation =
      (ev->keycode == HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION); // Key '1'
  bool is_question_mark =
      (ev->keycode ==
       HID_USAGE_KEY_KEYBOARD_MINUS_AND_UNDERSCORE); // Key '-' (used for '?'
                                                     // with Shift)
  bool is_apostrophe =
      (ev->keycode ==
       HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE); // Key '\' (used for '' on SE layout)
  // Add more special cases here if needed
  bool is_colon =
      (ev->keycode == HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON); // Base key for ':'
  bool is_plus =
      (ev->keycode == HID_USAGE_KEY_KEYBOARD_SLASH_AND_QUESTION_MARK); // Base key for '+' on SE


  // Core logic: If an instance was active and the key is punctuation (from array) or a special case
  if (any_instance_was_active &&
      (is_punctuation || is_exclamation || is_question_mark || is_apostrophe || is_colon || is_plus)) { // Added is_colon, is_plus
    LOG_DBG("Punctuation/Special key (%d) pressed after active cycle string. Replacing space.",
            ev->keycode);

    // 1. Send Backspace
    tap_usage(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE);
    // Optional delay if needed: k_msleep(CONFIG_ZMK_MACRO_DEFAULT_WAIT_MS);

    // 2. Send the appropriate key sequence
    if (is_exclamation) {
      // Send Shift + 1
      struct zmk_keycode_state_changed shift_press = {
          .usage_page = HID_USAGE_KEY,
          .keycode = HID_USAGE_KEY_KEYBOARD_LEFTSHIFT,
          .state = true,
          .timestamp = k_uptime_get()};
      raise_zmk_keycode_state_changed(shift_press);
      tap_usage(HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION); // Tap '1'
      struct zmk_keycode_state_changed shift_release = {
          .usage_page = HID_USAGE_KEY,
          .keycode = HID_USAGE_KEY_KEYBOARD_LEFTSHIFT,
          .state = false,
          .timestamp = k_uptime_get()};
      raise_zmk_keycode_state_changed(shift_release);
    } else if (is_question_mark) {
      // Send Shift + /
      struct zmk_keycode_state_changed shift_press = {
          .usage_page = HID_USAGE_KEY,
          .keycode = HID_USAGE_KEY_KEYBOARD_LEFTSHIFT,
          .state = true,
          .timestamp = k_uptime_get()};
      raise_zmk_keycode_state_changed(shift_press);
      tap_usage(HID_USAGE_KEY_KEYBOARD_MINUS_AND_UNDERSCORE); // Tap '-'
      struct zmk_keycode_state_changed shift_release = {
          .usage_page = HID_USAGE_KEY,
          .keycode = HID_USAGE_KEY_KEYBOARD_LEFTSHIFT,
          .state = false,
          .timestamp = k_uptime_get()};
      raise_zmk_keycode_state_changed(shift_release);
    } else if (is_apostrophe) {
      // Send '\' keycode (which produces ' on SE layout)
      tap_usage(HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE);
    } else if (is_colon) {
      // Send Shift + ;
      press_shift();
      tap_usage(HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON);
      release_shift();
    } else if (is_plus) {
      // Send Shift + / (for SE layout)
      press_shift();
      tap_usage(HID_USAGE_KEY_KEYBOARD_SLASH_AND_QUESTION_MARK);
      release_shift();
    } else { // is_punctuation must be true here (., ,, ;)
      // For other punctuation defined in the array, just tap the keycode
      tap_usage(ev->keycode);
    }

    // 3. Add a space after the punctuation/special key, *unless* it was an
    // apostrophe
    if (!is_apostrophe) {
      tap_usage(HID_USAGE_KEY_KEYBOARD_SPACEBAR);
    }

    // 4. Reset state for ALL instances using the helper function via macro
#define RESET_CYCLE_STATE(n)                                                   \
  do {                                                                         \
    reset_instance_state(DEVICE_DT_GET(DT_DRV_INST(n)));                       \
  } while (0)
    DT_INST_FOREACH_STATUS_OKAY(RESET_CYCLE_STATE);
#undef RESET_CYCLE_STATE

    // 5. Capture the original event (preventing it from bubbling)
    return ZMK_EV_EVENT_CAPTURED;
  } else {
    // Original behavior: Reset state if any key (non-punctuation or punctuation
    // without active state) is pressed
    // Reset state for ALL instances using the helper function via macro
#define RESET_CYCLE_STATE(n)                                                   \
  do {                                                                         \
    reset_instance_state(DEVICE_DT_GET(DT_DRV_INST(n)));                       \
  } while (0)
    DT_INST_FOREACH_STATUS_OKAY(RESET_CYCLE_STATE);
#undef RESET_CYCLE_STATE

    // Allow the original key press event to bubble
    return ZMK_EV_EVENT_BUBBLE;
  }
}

// Register the listener
ZMK_LISTENER(behavior_cycle_string,
             cycle_string_keycode_state_changed_listener);
// Subscribe the listener
ZMK_SUBSCRIPTION(behavior_cycle_string, zmk_keycode_state_changed);

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
      behavior_cycle_string_config_##n = {}; /* Define empty config */         \
  BEHAVIOR_DT_INST_DEFINE(                                                     \
      n, behavior_cycle_string_init, NULL, /* Deinit function, not needed */   \
      &behavior_cycle_string_state_##n,                                        \
      &behavior_cycle_string_config_##n, /* Pass address of config */          \
      POST_KERNEL,                       /* Initialize after kernel */         \
      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_cycle_string_driver_api);

// Apply the instantiation macro for all device tree nodes matching the
// compatible
DT_INST_FOREACH_STATUS_OKAY(CYC_STR_INST)
