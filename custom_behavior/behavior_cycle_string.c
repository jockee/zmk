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

  return 0; // No mapping found
}

// Structure to hold a cycle list and its length
typedef struct {
  const char **strings;
  const size_t len;
} cycle_list_t;

static const cycle_list_t all_cycle_lists[] = {
    {// Index 0
     .strings = (const char *[]){"the"},
     .len = 1},
    {// Index 1
     .strings = (const char *[]){"be", "been", "being"},
     .len = 3},
    {// Index 2
     .strings = (const char *[]){"is"},
     .len = 1},
    {// Index 3
     .strings = (const char *[]){"are"},
     .len = 1},
    {// Index 4
     .strings = (const char *[]){"was", "were"},
     .len = 2},
    {// Index 5
     .strings = (const char *[]){"and"},
     .len = 1},
    {// Index 6
     .strings = (const char *[]){"a"},
     .len = 1},
    {// Index 7
     .strings = (const char *[]){"of"},
     .len = 1},
    {// Index 8
     .strings = (const char *[]){"to"},
     .len = 1},
    {// Index 9
     .strings = (const char *[]){"in"},
     .len = 1},
    {// Index 10
     .strings = (const char *[]){"i", "my", "me", "mine"},
     .len = 4},
    {// Index 11
     .strings = (const char *[]){"i've"},
     .len = 1},
    {// Index 12
     .strings = (const char *[]){"i'm"},
     .len = 1},
    {// Index 13
     .strings = (const char *[]){"you", "your", "yours"},
     .len = 3},
    {// Index 14
     .strings = (const char *[]){"it", "its"},
     .len = 2},
    {// Index 15
     .strings = (const char *[]){"have", "has", "had", "having"},
     .len = 4},
    {// Index 16
     .strings = (const char *[]){"that", "that's"},
     .len = 2},
    {// Index 17
     .strings = (const char *[]){"for"},
     .len = 1},
    {// Index 18
     .strings = (const char *[]){"do", "does", "doing", "did"},
     .len = 4},
    {// Index 19
     .strings = (const char *[]){"don't"},
     .len = 1},
    {// Index 20
     .strings = (const char *[]){"won't"},
     .len = 1},
    {// Index 21
     .strings = (const char *[]){"he", "he's"},
     .len = 2},
    {// Index 22
     .strings = (const char *[]){"with"},
     .len = 1},
    {// Index 23
     .strings = (const char *[]){"on"},
     .len = 1},
    {// Index 24
     .strings = (const char *[]){"this"},
     .len = 1},
    {// Index 25
     .strings = (const char *[]){"we", "our", "us", "ours"},
     .len = 4},
    {// Index 26
     .strings = (const char *[]){"not"},
     .len = 1},
    {// Index 27
     .strings = (const char *[]){"but"},
     .len = 1},
    {// Index 28
     .strings = (const char *[]){"they", "their", "them", "theirs"},
     .len = 4},
    {// Index 29
     .strings = (const char *[]){"they've"},
     .len = 1},
    {// Index 30
     .strings = (const char *[]){"you've"},
     .len = 1},
    {// Index 31
     .strings = (const char *[]){"say", "says", "saying", "said"},
     .len = 4},
    {// Index 32
     .strings = (const char *[]){"at"},
     .len = 1},
    {// Index 33
     .strings = (const char *[]){"what"},
     .len = 1},
    {// Index 34
     .strings = (const char *[]){"from"},
     .len = 1},
    {// Index 35
     .strings = (const char *[]){"go", "goes", "going", "went"},
     .len = 4},
    {// Index 36
     .strings = (const char *[]){"or"},
     .len = 1},
    {// Index 37
     .strings = (const char *[]){"by"},
     .len = 1},
    {// Index 38
     .strings = (const char *[]){"get", "gets", "getting", "got"},
     .len = 4},
    {// Index 39
     .strings = (const char *[]){"she", "she's"},
     .len = 2},
    {// Index 40
     .strings = (const char *[]){"her", "hers"},
     .len = 2},
    {// Index 41
     .strings = (const char *[]){"can", "could"},
     .len = 2},
    {// Index 42
     .strings = (const char *[]){"as"},
     .len = 1},
    {// Index 43
     .strings = (const char *[]){"know", "knows", "knowing", "knew"},
     .len = 4},
    {// Index 44
     .strings = (const char *[]){"if"},
     .len = 1},
    {// Index 45
     .strings = (const char *[]){"me", "my", "myself"},
     .len = 3},
    {// Index 46
     .strings = (const char *[]){"your", "yours"},
     .len = 2},
    {// Index 47
     .strings = (const char *[]){"all"},
     .len = 1},
    {// Index 48
     .strings = (const char *[]){"who"},
     .len = 1},
    {// Index 49
     .strings = (const char *[]){"about"},
     .len = 1},
    {// Index 50
     .strings = (const char *[]){"will"},
     .len = 1},
    {// Index 51
     .strings = (const char *[]){"so"},
     .len = 1},
    {// Index 52
     .strings = (const char *[]){"would"},
     .len = 1},
    {// Index 53
     .strings = (const char *[]){"make", "makes", "making", "made"},
     .len = 4},
    {// Index 54
     .strings = (const char *[]){"just"},
     .len = 1},
    {// Index 55
     .strings = (const char *[]){"up"},
     .len = 1},
    {// Index 56
     .strings = (const char *[]){"think", "thinks", "thinking", "thought"},
     .len = 4},
    {// Index 57
     .strings = (const char *[]){"time", "times"},
     .len = 2},
    {// Index 58
     .strings = (const char *[]){"there"},
     .len = 1},
    {// Index 59
     .strings = (const char *[]){"see", "sees", "seeing", "saw"},
     .len = 4},
    {// Index 60
     .strings = (const char *[]){"out"},
     .len = 1},
    {// Index 61
     .strings = (const char *[]){"one"},
     .len = 1},
    {// Index 62
     .strings = (const char *[]){"come", "comes", "coming", "came"},
     .len = 4},
    {// Index 63
     .strings = (const char *[]){"people"},
     .len = 1},
    {// Index 64
     .strings = (const char *[]){"take", "takes", "taking", "took"},
     .len = 4},
    {// Index 65
     .strings = (const char *[]){"year", "years"},
     .len = 2},
    {// Index 66
     .strings = (const char *[]){"him", "his"},
     .len = 2},
    {// Index 67
     .strings = (const char *[]){"them", "their"},
     .len = 2},
    {// Index 68
     .strings = (const char *[]){"some"},
     .len = 1},
    {// Index 69
     .strings = (const char *[]){"want", "wants", "wanting", "wanted"},
     .len = 4},
    {// Index 70
     .strings = (const char *[]){"how"},
     .len = 1},
    {// Index 71
     .strings = (const char *[]){"when"},
     .len = 1},
    {// Index 72
     .strings = (const char *[]){"which"},
     .len = 1},
    {// Index 73
     .strings = (const char *[]){"now"},
     .len = 1},
    {// Index 74
     .strings = (const char *[]){"like", "likes", "liking", "liked"},
     .len = 4},
    {// Index 75
     .strings = (const char *[]){"other", "others"},
     .len = 2},
    {// Index 76
     .strings = (const char *[]){"our", "ours"},
     .len = 2},
    {// Index 77
     .strings = (const char *[]){"into"},
     .len = 1},
    {// Index 78
     .strings = (const char *[]){"here"},
     .len = 1},
    {// Index 79
     .strings = (const char *[]){"then"},
     .len = 1},
    {// Index 80
     .strings = (const char *[]){"than"},
     .len = 1},
    {// Index 81
     .strings = (const char *[]){"look", "looks", "looking", "looked"},
     .len = 4},
    {// Index 82
     .strings = (const char *[]){"way", "ways"},
     .len = 2},
    {// Index 83
     .strings = (const char *[]){"more"},
     .len = 1},
    {// Index 84
     .strings = (const char *[]){"these"},
     .len = 1},
    {// Index 85
     .strings = (const char *[]){"no"},
     .len = 1},
    {// Index 86
     .strings = (const char *[]){"thing", "things"},
     .len = 2},
    {// Index 87
     .strings = (const char *[]){"well", "better", "best"},
     .len = 3},
    {// Index 88
     .strings = (const char *[]){"because"},
     .len = 1},
    {// Index 89
     .strings = (const char *[]){"also"},
     .len = 1},
    {// Index 90
     .strings = (const char *[]){"two"},
     .len = 1},
    {// Index 91
     .strings = (const char *[]){"use", "uses", "using", "used"},
     .len = 4},
    {// Index 92
     .strings = (const char *[]){"tell", "tells", "telling", "told"},
     .len = 4},
    {// Index 93
     .strings = (const char *[]){"good", "better", "best"},
     .len = 3},
    {// Index 94
     .strings = (const char *[]){"first"},
     .len = 1},
    {// Index 95
     .strings = (const char *[]){"man", "men"},
     .len = 2},
    {// Index 96
     .strings = (const char *[]){"day", "days"},
     .len = 2},
    {// Index 97
     .strings = (const char *[]){"find", "finds", "finding", "found"},
     .len = 4},
    {// Index 98
     .strings = (const char *[]){"give", "gives", "giving", "gave"},
     .len = 4},
    {// Index 99
     .strings = (const char *[]){"new", "newer", "newest"},
     .len = 3},
    {// Index 100
     .strings = (const char *[]){"us"},
     .len = 1},
    {// Index 101
     .strings = (const char *[]){"any"},
     .len = 1},
    {// Index 102
     .strings = (const char *[]){"those"},
     .len = 1},
    {// Index 103
     .strings = (const char *[]){"very"},
     .len = 1},
    {// Index 104
     .strings = (const char *[]){"need", "needs", "needing", "needed"},
     .len = 4},
    {// Index 105
     .strings = (const char *[]){"back", "backs", "backing", "backed"},
     .len = 4},
    {// Index 106
     .strings = (const char *[]){"should"},
     .len = 1},
    {// Index 107
     .strings = (const char *[]){"even"},
     .len = 1},
    {// Index 108
     .strings = (const char *[]){"only"},
     .len = 1},
    {// Index 109
     .strings = (const char *[]){"many"},
     .len = 1},
    {// Index 110
     .strings = (const char *[]){"really"},
     .len = 1},
    {// Index 111
     .strings = (const char *[]){"work", "works", "working", "worked"},
     .len = 4},
    {// Index 112
     .strings = (const char *[]){"life", "lives"},
     .len = 2},
    {// Index 113
     .strings = (const char *[]){"why"},
     .len = 1},
    {// Index 114
     .strings = (const char *[]){"right", "rights"},
     .len = 2},
    {// Index 115
     .strings = (const char *[]){"down"},
     .len = 1},
    {// Index 116
     .strings = (const char *[]){"try", "tries", "trying", "tried"},
     .len = 4},
    {// Index 117
     .strings = (const char *[]){"let", "lets", "letting", "let"},
     .len = 4},
    {// Index 118
     .strings = (const char *[]){"something"},
     .len = 1},
    {// Index 119
     .strings = (const char *[]){"too"},
     .len = 1},
    {// Index 120
     .strings = (const char *[]){"call", "calls", "calling", "called"},
     .len = 4},
    {// Index 121
     .strings = (const char *[]){"woman", "women"},
     .len = 2},
    {// Index 122
     .strings = (const char *[]){"may"},
     .len = 1},
    {// Index 123
     .strings = (const char *[]){"still"},
     .len = 1},
    {// Index 124
     .strings = (const char *[]){"through"},
     .len = 1},
    {// Index 125
     .strings = (const char *[]){"mean", "means", "meaning", "meant"},
     .len = 4},
    {// Index 126
     .strings = (const char *[]){"after"},
     .len = 1},
    {// Index 127
     .strings = (const char *[]){"never"},
     .len = 1},
    {// Index 128
     .strings = (const char *[]){"world", "worlds"},
     .len = 2},
    {// Index 129
     .strings = (const char *[]){"feel", "feels", "feeling", "felt"},
     .len = 4},
    {// Index 130
     .strings = (const char *[]){"great", "greater", "greatest"},
     .len = 3},
    {// Index 131
     .strings = (const char *[]){"last"},
     .len = 1},
    {// Index 132
     .strings = (const char *[]){"child", "children"},
     .len = 2},
    {// Index 133
     .strings = (const char *[]){"oh"},
     .len = 1},
    {// Index 134
     .strings = (const char *[]){"ah"},
     .len = 1},
    {// Index 135
     .strings = (const char *[]){"over"},
     .len = 1},
    {// Index 136
     .strings = (const char *[]){"ask", "asks", "asking", "asked"},
     .len = 4},
    {// Index 137
     .strings = (const char *[]){"state", "states", "stated", "stating"},
     .len = 4},
    {// Index 138
     .strings = (const char *[]){"much"},
     .len = 1},
    {// Index 139
     .strings = (const char *[]){"talk", "talks", "talking", "talked"},
     .len = 4},
    {// Index 140
     .strings = (const char *[]){"keep", "keeps", "keeping", "kept"},
     .len = 4},
    {// Index 141
     .strings = (const char *[]){"leave", "leaves", "leaving", "left"},
     .len = 4},
    {// Index 142
     .strings = (const char *[]){"put", "puts", "putting", "put"},
     .len = 4},
    {// Index 143
     .strings = (const char *[]){"help", "helps", "helping", "helped"},
     .len = 4},
    {// Index 144
     .strings = (const char *[]){"big", "bigger", "biggest"},
     .len = 3},
    {// Index 145
     .strings = (const char *[]){"where"},
     .len = 1},
    {// Index 146
     .strings = (const char *[]){"same"},
     .len = 1},
    {// Index 147
     .strings = (const char *[]){"own", "owns", "owning", "owned"},
     .len = 4},
    {// Index 148
     .strings = (const char *[]){"while"},
     .len = 1},
    {// Index 149
     .strings = (const char *[]){"start", "starts", "starting", "started"},
     .len = 4},
    {// Index 150
     .strings = (const char *[]){"three"},
     .len = 1},
    {// Index 151
     .strings = (const char *[]){"high", "higher", "highest"},
     .len = 3},
    {// Index 152
     .strings = (const char *[]){"every"},
     .len = 1},
    {// Index 153
     .strings = (const char *[]){"another"},
     .len = 1},
    {// Index 154
     .strings = (const char *[]){"become", "becomes", "becoming", "became"},
     .len = 4},
    {// Index 155
     .strings = (const char *[]){"most"},
     .len = 1},
    {// Index 156
     .strings = (const char *[]){"between"},
     .len = 1},
    {// Index 157
     .strings = (const char *[]){"happen", "happens", "happening", "happened"},
     .len = 4},
    {// Index 158
     .strings = (const char *[]){"family", "families"},
     .len = 2},
    {// Index 159
     .strings = (const char *[]){"old", "older", "oldest"},
     .len = 3},
    {// Index 160
     .strings = (const char *[]){"yes"},
     .len = 1},
    {// Index 161
     .strings = (const char *[]){"house", "houses"},
     .len = 2},
    {// Index 162
     .strings = (const char *[]){"show", "shows", "showing", "showed"},
     .len = 4},
    {// Index 163
     .strings = (const char *[]){"again"},
     .len = 1},
    {// Index 164
     .strings = (const char *[]){"student", "students"},
     .len = 2},
    {// Index 165
     .strings = (const char *[]){"seem", "seems", "seeming", "seemed"},
     .len = 4},
    {// Index 166
     .strings = (const char *[]){"might"},
     .len = 1},
    {// Index 167
     .strings = (const char *[]){"part", "parts"},
     .len = 2},
    {// Index 168
     .strings = (const char *[]){"hear", "hears", "hearing", "heard"},
     .len = 4},
    {// Index 169
     .strings = (const char *[]){"its", "it's"},
     .len = 2},
    {// Index 170
     .strings = (const char *[]){"place", "places", "placing", "placed"},
     .len = 4},
    {// Index 171
     .strings = (const char *[]){"problem", "problems"},
     .len = 2},
    {// Index 172
     .strings =
         (const char *[]){"believe", "believes", "believing", "believed"},
     .len = 4},
    {// Index 173
     .strings = (const char *[]){"country", "countries"},
     .len = 2},
    {// Index 174
     .strings = (const char *[]){"always"},
     .len = 1},
    {// Index 175
     .strings = (const char *[]){"week", "weeks"},
     .len = 2},
    {// Index 176
     .strings = (const char *[]){"point", "points", "pointing", "pointed"},
     .len = 4},
    {// Index 177
     .strings = (const char *[]){"hand", "hands", "handing", "handed"},
     .len = 4},
    {// Index 178
     .strings = (const char *[]){"off"},
     .len = 1},
    {// Index 179
     .strings = (const char *[]){"play", "plays", "playing", "played"},
     .len = 4},
    {// Index 180
     .strings = (const char *[]){"turn", "turns", "turning", "turned"},
     .len = 4},
    {// Index 181
     .strings = (const char *[]){"few", "fewer", "fewest"},
     .len = 3},
    {// Index 182
     .strings = (const char *[]){"group", "groups", "grouping", "grouped"},
     .len = 4},
    {// Index 183
     .strings = (const char *[]){"such"},
     .len = 1},
    {// Index 184
     .strings = (const char *[]){"fact", "facts"},
     .len = 2},
    {// Index 185
     .strings = (const char *[]){"against"},
     .len = 1},
    {// Index 186
     .strings = (const char *[]){"case", "cases"},
     .len = 2},
    {// Index 187
     .strings = (const char *[]){"around"},
     .len = 1},
    {// Index 188
     .strings = (const char *[]){"company", "companies"},
     .len = 2},
    {// Index 189
     .strings = (const char *[]){"system", "systems"},
     .len = 2},
    {// Index 190
     .strings = (const char *[]){"each"},
     .len = 1},
    {// Index 191
     .strings = (const char *[]){"program", "programs"},
     .len = 2},
    {// Index 192
     .strings = (const char *[]){"question", "questions"},
     .len = 2},
    {// Index 193
     .strings = (const char *[]){"during"},
     .len = 1},
    {// Index 194
     .strings = (const char *[]){"run", "runs", "running", "ran"},
     .len = 4},
    {// Index 195
     .strings = (const char *[]){"small", "smaller", "smallest"},
     .len = 3},
    {// Index 196
     .strings = (const char *[]){"number", "numbers"},
     .len = 2},
    {// Index 197
     .strings = (const char *[]){"move", "moves", "moving", "moved"},
     .len = 4},
    {// Index 198
     .strings = (const char *[]){"night", "nights"},
     .len = 2},
    {// Index 199
     .strings = (const char *[]){"live", "lives", "living", "lived"},
     .len = 4},
    {// Index 200
     .strings =
         (const char *[]){"replace", "replaces", "replacing", "replaced"},
     .len = 4},
    {// Index 201
     .strings = (const char *[]){"hold", "holds", "holding", "held"},
     .len = 4},
    {// Index 202
     .strings = (const char *[]){"today"},
     .len = 1},
    {// Index 203
     .strings = (const char *[]){"bring", "brings", "bringing", "brought"},
     .len = 4},
    {// Index 204
     .strings = (const char *[]){"next"},
     .len = 1},
    {// Index 205
     .strings = (const char *[]){"without"},
     .len = 1},
    {// Index 206
     .strings = (const char *[]){"before"},
     .len = 1},
    {// Index 207
     .strings = (const char *[]){"large", "larger", "largest"},
     .len = 3},
    {// Index 208
     .strings = (const char *[]){"must"},
     .len = 1},
    {// Index 209
     .strings = (const char *[]){"home", "homes"},
     .len = 2},
    {// Index 210
     .strings = (const char *[]){"under"},
     .len = 1},
    {// Index 211
     .strings = (const char *[]){"water"},
     .len = 1},
    {// Index 212
     .strings = (const char *[]){"room", "rooms"},
     .len = 2},
    {// Index 213
     .strings = (const char *[]){"write", "writes", "writing", "wrote"},
     .len = 4},
    {// Index 214
     .strings = (const char *[]){"area", "areas"},
     .len = 2},
    {// Index 215
     .strings = (const char *[]){"national"},
     .len = 1},
    {// Index 216
     .strings = (const char *[]){"money"},
     .len = 1},
    {// Index 217
     .strings = (const char *[]){"story", "stories"},
     .len = 2},
    {// Index 218
     .strings = (const char *[]){"young", "younger", "youngest"},
     .len = 3},
    {// Index 219
     .strings = (const char *[]){"month", "months"},
     .len = 2},
    {// Index 220
     .strings = (const char *[]){"different"},
     .len = 1},
    {// Index 221
     .strings = (const char *[]){"lot", "lots"},
     .len = 2},
    {// Index 222
     .strings = (const char *[]){"study", "studies", "studying", "studied"},
     .len = 4},
    {// Index 223
     .strings = (const char *[]){"book", "books"},
     .len = 2},
    {// Index 224
     .strings = (const char *[]){"job", "jobs"},
     .len = 2},
    {// Index 225
     .strings = (const char *[]){"word", "words"},
     .len = 2},
    {// Index 226
     .strings = (const char *[]){"though"},
     .len = 1},
    {// Index 227
     .strings = (const char *[]){"business", "businesses"},
     .len = 2},
    {// Index 228
     .strings = (const char *[]){"issue", "issues"},
     .len = 2},
    {// Index 229
     .strings = (const char *[]){"side", "sides"},
     .len = 2},
    {// Index 230
     .strings = (const char *[]){"kind", "kinds"},
     .len = 2},
    {// Index 231
     .strings = (const char *[]){"four"},
     .len = 1},
    {// Index 232
     .strings = (const char *[]){"head", "heads", "heading", "headed"},
     .len = 4},
    {// Index 233
     .strings = (const char *[]){"far", "farther", "farthest"},
     .len = 3},
    {// Index 234
     .strings = (const char *[]){"black", "blacks", "blacked", "blacking"},
     .len = 4},
    {// Index 235
     .strings = (const char *[]){"long", "longer", "longest"},
     .len = 3},
    {// Index 236
     .strings = (const char *[]){"both"},
     .len = 1},
    {// Index 237
     .strings = (const char *[]){"little", "less", "least"},
     .len = 3},
    {// Index 238
     .strings = (const char *[]){"since"},
     .len = 1},
    {// Index 239
     .strings =
         (const char *[]){"provide", "provides", "providing", "provided"},
     .len = 4},
    {// Index 240
     .strings = (const char *[]){"service", "services"},
     .len = 2},
    {// Index 241
     .strings = (const char *[]){"friend", "friends", "friending", "friended"},
     .len = 4},
    {// Index 242
     .strings = (const char *[]){"important"},
     .len = 1},
    {// Index 243
     .strings = (const char *[]){"father", "fathers"},
     .len = 2},
    {// Index 244
     .strings = (const char *[]){"sit", "sits", "sitting", "sat"},
     .len = 4},
    {// Index 245
     .strings = (const char *[]){"away"},
     .len = 1},
    {// Index 246
     .strings = (const char *[]){"until"},
     .len = 1},
    {// Index 247
     .strings = (const char *[]){"power", "powers", "powering", "powered"},
     .len = 4},
    {// Index 248
     .strings = (const char *[]){"hour", "hours"},
     .len = 2},
    {// Index 249
     .strings = (const char *[]){"game", "games"},
     .len = 2},
    {// Index 250
     .strings = (const char *[]){"often"},
     .len = 1},
    {// Index 251
     .strings = (const char *[]){"yet"},
     .len = 1},
    {// Index 252
     .strings = (const char *[]){"line", "lines", "lining", "lined"},
     .len = 4},
    {// Index 253
     .strings = (const char *[]){"end", "ends", "ending", "ended"},
     .len = 4},
    {// Index 254
     .strings = (const char *[]){"among"},
     .len = 1},
    {// Index 255
     .strings = (const char *[]){"ever"},
     .len = 1},
    {// Index 256
     .strings = (const char *[]){"stand", "stands", "standing", "stood"},
     .len = 4},
    {// Index 257
     .strings = (const char *[]){"bad", "worse", "worst"},
     .len = 3},
    {// Index 258
     .strings = (const char *[]){"lose", "loses", "losing", "lost"},
     .len = 4},
    {// Index 259
     .strings = (const char *[]){"however"},
     .len = 1},
    {// Index 260
     .strings = (const char *[]){"member", "members"},
     .len = 2},
    {// Index 261
     .strings = (const char *[]){"pay", "pays", "paying", "paid"},
     .len = 4},
    {// Index 262
     .strings = (const char *[]){"law", "laws"},
     .len = 2},
    {// Index 263
     .strings = (const char *[]){"meet", "meets", "meeting", "met"},
     .len = 4},
    {// Index 264
     .strings = (const char *[]){"car", "cars"},
     .len = 2},
    {// Index 265
     .strings = (const char *[]){"city", "cities"},
     .len = 2},
    {// Index 266
     .strings = (const char *[]){"almost"},
     .len = 1},
    {// Index 267
     .strings =
         (const char *[]){"include", "includes", "including", "included"},
     .len = 4},
    {// Index 268
     .strings =
         (const char *[]){"continue", "continues", "continuing", "continued"},
     .len = 4},
    {// Index 269
     .strings = (const char *[]){"set", "sets", "setting", "set"},
     .len = 4},
    {// Index 270
     .strings = (const char *[]){"later"},
     .len = 1},
    {// Index 271
     .strings = (const char *[]){"community", "communities"},
     .len = 2},
    {// Index 272
     .strings = (const char *[]){"name", "names", "naming", "named"},
     .len = 4},
    {// Index 273
     .strings = (const char *[]){"five"},
     .len = 1},
    {// Index 274
     .strings = (const char *[]){"once"},
     .len = 1},
    {// Index 275
     .strings = (const char *[]){"white", "whiter", "whitest"},
     .len = 3},
    {// Index 276
     .strings = (const char *[]){"least"},
     .len = 1},
    {// Index 277
     .strings = (const char *[]){"learn", "learns", "learning", "learned"},
     .len = 4},
    {// Index 278
     .strings = (const char *[]){"real"},
     .len = 1},
    {// Index 279
     .strings = (const char *[]){"change", "changes", "changing", "changed"},
     .len = 4},
    {// Index 280
     .strings = (const char *[]){"team", "teams"},
     .len = 2},
    {// Index 281
     .strings = (const char *[]){"minute", "minutes"},
     .len = 2},
    {// Index 282
     .strings = (const char *[]){"several"},
     .len = 1},
    {// Index 283
     .strings = (const char *[]){"idea", "ideas"},
     .len = 2},
    {// Index 284
     .strings = (const char *[]){"kid", "kids"},
     .len = 2},
    {// Index 285
     .strings = (const char *[]){"body", "bodies"},
     .len = 2},
    {// Index 286
     .strings = (const char *[]){"information"},
     .len = 1},
    {// Index 287
     .strings = (const char *[]){"nothing"},
     .len = 1},
    {// Index 288
     .strings = (const char *[]){"ago"},
     .len = 1},
    {// Index 289
     .strings = (const char *[]){"lead", "leads", "leading", "led"},
     .len = 4},
    {// Index 290
     .strings = (const char *[]){"social"},
     .len = 1},
    {// Index 291
     .strings = (const char *[]){"understand", "understands", "understanding",
                                 "understood"},
     .len = 4},
    {// Index 292
     .strings = (const char *[]){"whether"},
     .len = 1},
    {// Index 293
     .strings = (const char *[]){"watch", "watches", "watching", "watched"},
     .len = 4},
    {// Index 294
     .strings = (const char *[]){"together"},
     .len = 1},
    {// Index 295
     .strings = (const char *[]){"follow", "follows", "following", "followed"},
     .len = 4},
    {// Index 296
     .strings = (const char *[]){"parent", "parents"},
     .len = 2},
    {// Index 297
     .strings = (const char *[]){"stop", "stops", "stopping", "stopped"},
     .len = 4},
    {// Index 298
     .strings = (const char *[]){"face", "faces", "facing", "faced"},
     .len = 4},
    {// Index 299
     .strings = (const char *[]){"create", "creates", "creating", "created"},
     .len = 4},
    {// Index 300
     .strings = (const char *[]){"public"},
     .len = 1},
    {// Index 301
     .strings = (const char *[]){"already"},
     .len = 1},
    {// Index 302
     .strings = (const char *[]){"speak", "speaks", "speaking", "spoke"},
     .len = 4},
    {// Index 303
     .strings = (const char *[]){"read", "reads", "reading", "read"},
     .len = 4},
    {// Index 304
     .strings = (const char *[]){"level", "levels", "leveling", "leveled"},
     .len = 4},
    {// Index 305
     .strings = (const char *[]){"allow", "allows", "allowing", "allowed"},
     .len = 4},
    {// Index 306
     .strings = (const char *[]){"add", "adds", "adding", "added"},
     .len = 4},
    {// Index 307
     .strings = (const char *[]){"office", "offices"},
     .len = 2},
    {// Index 308
     .strings = (const char *[]){"spend", "spends", "spending", "spent"},
     .len = 4},
    {// Index 309
     .strings = (const char *[]){"door", "doors"},
     .len = 2},
    {// Index 310
     .strings = (const char *[]){"health"},
     .len = 1},
    {// Index 311
     .strings = (const char *[]){"person", "people"},
     .len = 2},
    {// Index 312
     .strings = (const char *[]){"sure", "surer", "surest"},
     .len = 3},
    {// Index 313
     .strings = (const char *[]){"history", "histories"},
     .len = 2},
    {// Index 314
     .strings = (const char *[]){"party", "parties"},
     .len = 2},
    {// Index 315
     .strings = (const char *[]){"within"},
     .len = 1},
    {// Index 316
     .strings = (const char *[]){"grow", "grows", "growing", "grew"},
     .len = 4},
    {// Index 317
     .strings = (const char *[]){"result", "results", "resulting", "resulted"},
     .len = 4},
    {// Index 318
     .strings = (const char *[]){"open", "opens", "opening", "opened"},
     .len = 4},
    {// Index 319
     .strings = (const char *[]){"morning", "mornings"},
     .len = 2},
    {// Index 320
     .strings = (const char *[]){"walk", "walks", "walking", "walked"},
     .len = 4},
    {// Index 321
     .strings = (const char *[]){"reason", "reasons", "reasoning", "reasoned"},
     .len = 4},
    {// Index 322
     .strings = (const char *[]){"low", "lower", "lowest"},
     .len = 3},
    {// Index 323
     .strings = (const char *[]){"win", "wins", "winning", "won"},
     .len = 4},
    {// Index 324
     .strings = (const char *[]){"research", "researches", "researching",
                                 "researched"},
     .len = 4},
    {// Index 325
     .strings = (const char *[]){"girl", "girls"},
     .len = 2},
    {// Index 326
     .strings = (const char *[]){"guy", "guys"},
     .len = 2},
    {// Index 327
     .strings = (const char *[]){"early", "earlier", "earliest"},
     .len = 3},
    {// Index 328
     .strings = (const char *[]){"food", "foods"},
     .len = 2},
    {// Index 329
     .strings = (const char *[]){"moment", "moments"},
     .len = 2},
    {// Index 330
     .strings = (const char *[]){"himself"},
     .len = 1},
    {// Index 331
     .strings = (const char *[]){"air", "airs", "airing", "aired"},
     .len = 4},
    {// Index 332
     .strings = (const char *[]){"force", "forces", "forcing", "forced"},
     .len = 4},
    {// Index 333
     .strings = (const char *[]){"offer", "offers", "offering", "offered"},
     .len = 4},
    {// Index 334
     .strings = (const char *[]){"enough"},
     .len = 1},
    {// Index 335
     .strings = (const char *[]){"although"},
     .len = 1},
    {// Index 336
     .strings =
         (const char *[]){"remember", "remembers", "remembering", "remembered"},
     .len = 4},
    {// Index 337
     .strings = (const char *[]){"second", "seconds"},
     .len = 2},
    {// Index 338
     .strings = (const char *[]){"boy", "boys"},
     .len = 2},
    {// Index 339
     .strings = (const char *[]){"maybe"},
     .len = 1},
    {// Index 340
     .strings = (const char *[]){"toward"},
     .len = 1},
    {// Index 341
     .strings = (const char *[]){"able"},
     .len = 1},
    {// Index 342
     .strings = (const char *[]){"age", "ages", "aging", "aged"},
     .len = 4},
    {// Index 343
     .strings = (const char *[]){"everything"},
     .len = 1},
    {// Index 344
     .strings = (const char *[]){"love", "loves", "loving", "loved"},
     .len = 4},
    {// Index 345
     .strings =
         (const char *[]){"process", "processes", "processing", "processed"},
     .len = 4},
    {// Index 346
     .strings = (const char *[]){"input", "inputs"},
     .len = 2},
    {// Index 347
     .strings = (const char *[]){"output", "outputs"},
     .len = 2},
    {// Index 348
     .strings = (const char *[]){"qualifier"},
     .len = 1},
    {// Index 349
     .strings = (const char *[]){"error", "errors"},
     .len = 2},
    {// Index 350
     .strings = (const char *[]){"code", "coding", "codes", "coded"},
     .len = 4},
    {// Index 351
     .strings = (const char *[]){"method", "methods"},
     .len = 2},
    {// Index 352
     .strings = (const char *[]){"object", "objects"},
     .len = 2},
    {// Index 353
     .strings = (const char *[]){"variable", "variables"},
     .len = 2},
    {// Index 354
     .strings = (const char *[]){"array", "arrays"},
     .len = 2},
    {// Index 355
     .strings = (const char *[]){"request", "requests", "requested"},
     .len = 3},
    {// Index 356
     .strings = (const char *[]){"response", "responses", "responded"},
     .len = 3},
    {// Index 357
     .strings = (const char *[]){"access", "accessed", "accesses"},
     .len = 3},
    {// Index 358
     .strings = (const char *[]){"database", "databases"},
     .len = 2},
    {// Index 359
     .strings = (const char *[]){"server", "servers"},
     .len = 2},
    {// Index 360
     .strings = (const char *[]){"log", "login"},
     .len = 2},
    {// Index 361
     .strings = (const char *[]){"connect", "connects", "connecting"},
     .len = 3},
    {// Index 362
     .strings =
         (const char *[]){"config", "configuration", "configure", "configured"},
     .len = 4},
    {// Index 363
     .strings = (const char *[]){"load", "loading", "loads", "loaded"},
     .len = 4},
    {// Index 364
     .strings = (const char *[]){"save", "saved", "saving", "saves"},
     .len = 4},
    {// Index 365
     .strings = (const char *[]){"user", "users"},
     .len = 2},
    {// Index 366
     .strings = (const char *[]){"admin", "admins"},
     .len = 2},
    {// Index 367
     .strings = (const char *[]){"deploy"},
     .len = 1},
    {// Index 368
     .strings = (const char *[]){"module", "modules"},
     .len = 2},
    {// Index 369
     .strings = (const char *[]){"fail", "fails", "failing", "failed"},
     .len = 4},
    {// Index 370
     .strings = (const char *[]){"music"},
     .len = 1},
    {// Index 371
     .strings =
         (const char *[]){"consider", "considers", "considering", "considered"},
     .len = 4},
    {// Index 372
     .strings = (const char *[]){"appear", "appears", "appearing", "appeared"},
     .len = 4},
    {// Index 373
     .strings = (const char *[]){"buy", "buys", "buying", "bought"},
     .len = 4},
    {// Index 374
     .strings = (const char *[]){"probably"},
     .len = 1},
    {// Index 375
     .strings = (const char *[]){"human", "humans"},
     .len = 2},
    {// Index 376
     .strings = (const char *[]){"wait", "waits", "waiting", "waited"},
     .len = 4},
    {// Index 377
     .strings = (const char *[]){"serve", "serves", "serving", "served"},
     .len = 4},
    {// Index 378
     .strings = (const char *[]){"market", "markets", "marketing", "marketed"},
     .len = 4},
    {// Index 379
     .strings = (const char *[]){"die", "dies", "dying", "died"},
     .len = 4},
    {// Index 380
     .strings = (const char *[]){"send", "sends", "sending", "sent"},
     .len = 4},
    {// Index 381
     .strings = (const char *[]){"expect", "expects", "expecting", "expected"},
     .len = 4},
    {// Index 382
     .strings = (const char *[]){"sense", "senses", "sensing", "sensed"},
     .len = 4},
    {// Index 383
     .strings = (const char *[]){"build", "builds", "building", "built"},
     .len = 4},
    {// Index 384
     .strings = (const char *[]){"stay", "stays", "staying", "stayed"},
     .len = 4},
    {// Index 385
     .strings = (const char *[]){"fall", "falls", "falling", "fell"},
     .len = 4},
    {// Index 386
     .strings = (const char *[]){"plan", "plans", "planning", "planned"},
     .len = 4},
    {// Index 387
     .strings = (const char *[]){"cut", "cuts", "cutting", "cut"},
     .len = 4},
    {// Index 388
     .strings =
         (const char *[]){"interest", "interests", "interesting", "interested"},
     .len = 4},
    {// Index 389
     .strings = (const char *[]){"death", "deaths"},
     .len = 2},
    {// Index 390
     .strings = (const char *[]){"course", "courses", "coursing", "coursed"},
     .len = 4},
    {// Index 391
     .strings = (const char *[]){"someone"},
     .len = 1},
    {// Index 392
     .strings = (const char *[]){"experience", "experiences", "experiencing",
                                 "experienced"},
     .len = 4},
    {// Index 393
     .strings = (const char *[]){"behind"},
     .len = 1},
    {// Index 394
     .strings = (const char *[]){"reach", "reaches", "reaching", "reached"},
     .len = 4},
    {// Index 395
     .strings = (const char *[]){"local"},
     .len = 1},
    {// Index 396
     .strings = (const char *[]){"kill", "kills", "killing", "killed"},
     .len = 4},
    {// Index 397
     .strings = (const char *[]){"six"},
     .len = 1},
    {// Index 398
     .strings = (const char *[]){"weather", "weather"},
     .len = 2},
    {// Index 399
     .strings = (const char *[]){"marathon", "marathons"},
     .len = 2},
    {// Index 400
     .strings = (const char *[]){"chess"},
     .len = 1},
    {// Index 401
     .strings = (const char *[]){"remain", "remains", "remaining", "remained"},
     .len = 4},
    {// Index 402
     .strings = (const char *[]){"effect", "effects", "effecting", "effected"},
     .len = 4},
    {// Index 403
     .strings =
         (const char *[]){"suggest", "suggests", "suggesting", "suggested"},
     .len = 4},
    {// Index 404
     .strings = (const char *[]){"class", "classes"},
     .len = 2},
    {// Index 405
     .strings =
         (const char *[]){"control", "controls", "controlling", "controlled"},
     .len = 4},
    {// Index 406
     .strings = (const char *[]){"raise", "raises", "raising", "raised"},
     .len = 4},
    {// Index 407
     .strings = (const char *[]){"care", "cares", "caring", "cared"},
     .len = 4},
    {// Index 408
     .strings = (const char *[]){"perhaps"},
     .len = 1},
    {// Index 409
     .strings = (const char *[]){"late", "later", "latest"},
     .len = 3},
    {// Index 410
     .strings = (const char *[]){"hard", "harder", "hardest"},
     .len = 3},
    {// Index 411
     .strings = (const char *[]){"field", "fields"},
     .len = 2},
    {// Index 412
     .strings = (const char *[]){"else"},
     .len = 1},
    {// Index 413
     .strings = (const char *[]){"pass", "passes", "passing", "passed"},
     .len = 4},
    {// Index 414
     .strings = (const char *[]){"former"},
     .len = 1},
    {// Index 415
     .strings = (const char *[]){"sell", "sells", "selling", "sold"},
     .len = 4},
    {// Index 416
     .strings = (const char *[]){"major"},
     .len = 1},
    {// Index 417
     .strings = (const char *[]){"sometimes"},
     .len = 1},
    {// Index 418
     .strings =
         (const char *[]){"require", "requires", "requiring", "required"},
     .len = 4},
    {// Index 419
     .strings = (const char *[]){"along"},
     .len = 1},
    {// Index 420
     .strings = (const char *[]){"development", "developments"},
     .len = 2},
    {// Index 421
     .strings = (const char *[]){"themselves"},
     .len = 1},
    {// Index 422
     .strings = (const char *[]){"report", "reports", "reporting", "reported"},
     .len = 4},
    {// Index 423
     .strings = (const char *[]){"role", "roles"},
     .len = 2},
    {// Index 424
     .strings = (const char *[]){"better"},
     .len = 1},
    {// Index 425
     .strings = (const char *[]){"economic"},
     .len = 1},
    {// Index 426
     .strings = (const char *[]){"effort", "efforts"},
     .len = 2},
    {// Index 427
     .strings = (const char *[]){"decide", "decides", "deciding", "decided"},
     .len = 4},
    {// Index 428
     .strings = (const char *[]){"rate", "rates", "rating", "rated"},
     .len = 4},
    {// Index 429
     .strings = (const char *[]){"strong", "stronger", "strongest"},
     .len = 3},
    {// Index 430
     .strings = (const char *[]){"possible"},
     .len = 1},
    {// Index 431
     .strings = (const char *[]){"leader", "leaders"},
     .len = 2},
    {// Index 432
     .strings =
         (const char *[]){"light", "lights", "lighting", "lighted", "lighter"},
     .len = 5},
    {// Index 433
     .strings = (const char *[]){"dark", "darker", "darkest"},
     .len = 3},
    {// Index 434
     .strings = (const char *[]){"voice", "voices", "voicing", "voiced"},
     .len = 4},
    {// Index 435
     .strings = (const char *[]){"whole"},
     .len = 1},
    {// Index 436
     .strings = (const char *[]){"finally"},
     .len = 1},
    {// Index 437
     .strings = (const char *[]){"pull", "pulls", "pulling", "pulled"},
     .len = 4},
    {// Index 438
     .strings = (const char *[]){"return", "returns", "returning", "returned"},
     .len = 4},
    {// Index 439
     .strings = (const char *[]){"free", "freer", "freest"},
     .len = 3},
    {// Index 440
     .strings = (const char *[]){"price", "prices", "pricing", "priced"},
     .len = 4},
    {// Index 441
     .strings = (const char *[]){"less", "lesser"},
     .len = 2},
    {// Index 442
     .strings = (const char *[]){"according"},
     .len = 1},
    {// Index 443
     .strings = (const char *[]){"decision", "decisions"},
     .len = 2},
    {// Index 444
     .strings =
         (const char *[]){"explain", "explains", "explaining", "explained"},
     .len = 4},
    {// Index 445
     .strings = (const char *[]){"hope", "hopes", "hoping", "hoped"},
     .len = 4},
    {// Index 446
     .strings =
         (const char *[]){"develop", "develops", "developing", "developed"},
     .len = 4},
    {// Index 447
     .strings = (const char *[]){"view", "views", "viewing", "viewed"},
     .len = 4},
    {// Index 448
     .strings = (const char *[]){"relationship", "relationships"},
     .len = 2},
    {// Index 449
     .strings = (const char *[]){"drive", "drives", "driving", "drove"},
     .len = 4},
    {// Index 450
     .strings = (const char *[]){"arm", "arms", "arming", "armed"},
     .len = 4},
    {// Index 451
     .strings = (const char *[]){"true", "truer", "truest"},
     .len = 3},
    {// Index 452
     .strings = (const char *[]){"break", "breaks", "breaking", "broke"},
     .len = 4},
    {// Index 453
     .strings = (const char *[]){"difference", "differences"},
     .len = 2},
    {// Index 454
     .strings = (const char *[]){"thank", "thanks", "thanking", "thanked"},
     .len = 4},
    {// Index 455
     .strings =
         (const char *[]){"receive", "receives", "receiving", "received"},
     .len = 4},
    {// Index 456
     .strings = (const char *[]){"value", "values", "valuing", "valued"},
     .len = 4},
    {// Index 457
     .strings = (const char *[]){"international"},
     .len = 1},
    {// Index 458
     .strings = (const char *[]){"action", "actions"},
     .len = 2},
    {// Index 459
     .strings = (const char *[]){"full", "fuller", "fullest"},
     .len = 3},
    {// Index 460
     .strings = (const char *[]){"function", "functions"},
     .len = 2},
    {// Index 461
     .strings = (const char *[]){"model", "models", "modeling", "modeled"},
     .len = 4},
    {// Index 462
     .strings = (const char *[]){"join", "joins", "joining", "joined"},
     .len = 4},
    {// Index 463
     .strings = (const char *[]){"season", "seasons"},
     .len = 2},
    {// Index 464
     .strings =
         (const char *[]){"position", "positions", "positioning", "positioned"},
     .len = 4},
    {// Index 465
     .strings = (const char *[]){"player", "players"},
     .len = 2},
    {// Index 466
     .strings = (const char *[]){"agree", "agrees", "agreeing", "agreed"},
     .len = 4},
    {// Index 467
     .strings = (const char *[]){"especially"},
     .len = 1},
    {// Index 468
     .strings = (const char *[]){"record", "records", "recording", "recorded"},
     .len = 4},
    {// Index 469
     .strings = (const char *[]){"pick", "picks", "picking", "picked"},
     .len = 4},
    {// Index 470
     .strings = (const char *[]){"wear", "wears", "wearing", "wore"},
     .len = 4},
    {// Index 471
     .strings = (const char *[]){"special"},
     .len = 1},
    {// Index 472
     .strings = (const char *[]){"space", "spaces", "spacing", "spaced"},
     .len = 4},
    {// Index 473
     .strings = (const char *[]){"ground", "grounds", "grounding", "grounded"},
     .len = 4},
    {// Index 474
     .strings = (const char *[]){"form", "forms", "forming", "formed"},
     .len = 4},
    {// Index 475
     .strings =
         (const char *[]){"support", "supports", "supporting", "supported"},
     .len = 4},
    {// Index 476
     .strings = (const char *[]){"event", "events"},
     .len = 2},
    {// Index 477
     .strings = (const char *[]){"official", "officials"},
     .len = 2},
    {// Index 478
     .strings = (const char *[]){"whose"},
     .len = 1},
    {// Index 479
     .strings = (const char *[]){"matter", "matters", "mattering", "mattered"},
     .len = 4},
    {// Index 480
     .strings = (const char *[]){"everyone"},
     .len = 1},
    {// Index 481
     .strings = (const char *[]){"center", "centers", "centering", "centered"},
     .len = 4},
    {// Index 482
     .strings = (const char *[]){"site", "sites", "siting", "sited"},
     .len = 4},
    {// Index 483
     .strings =
         (const char *[]){"project", "projects", "projecting", "projected"},
     .len = 4},
    {// Index 484
     .strings = (const char *[]){"hit", "hits", "hitting", "hit"},
     .len = 4},
    {// Index 485
     .strings = (const char *[]){"base", "bases", "basing", "based"},
     .len = 4},
    {// Index 486
     .strings = (const char *[]){"activity", "activities"},
     .len = 2},
    {// Index 487
     .strings = (const char *[]){"star", "stars", "starring", "starred"},
     .len = 4},
    {// Index 488
     .strings = (const char *[]){"table", "tables", "tabling", "tabled"},
     .len = 4},
    {// Index 489
     .strings =
         (const char *[]){"produce", "produces", "producing", "produced"},
     .len = 4},
    {// Index 490
     .strings = (const char *[]){"eat", "eats", "eating", "ate"},
     .len = 4},
    {// Index 491
     .strings = (const char *[]){"teach", "teaches", "teaching", "taught"},
     .len = 4},
    {// Index 492
     .strings = (const char *[]){"half", "halves"},
     .len = 2},
    {// Index 493
     .strings = (const char *[]){"situation", "situations"},
     .len = 2},
    {// Index 494
     .strings = (const char *[]){"easy", "easier", "easiest"},
     .len = 3},
    {// Index 495
     .strings = (const char *[]){"cost", "costs", "costing", "costed"},
     .len = 4},
    {// Index 496
     .strings = (const char *[]){"industry", "industries"},
     .len = 2},
    {// Index 497
     .strings = (const char *[]){"street", "streets"},
     .len = 2},
    {// Index 498
     .strings = (const char *[]){"image", "images", "imaging", "imaged"},
     .len = 4},
    {// Index 499
     .strings = (const char *[]){"itself"},
     .len = 1},
    {// Index 500
     .strings = (const char *[]){"phone", "phones", "phoning", "phoned"},
     .len = 4},
    {// Index 501
     .strings = (const char *[]){"either"},
     .len = 1},
    {// Index 502
     .strings = (const char *[]){"data"},
     .len = 1},
    {// Index 503
     .strings = (const char *[]){"cover", "covers", "covering", "covered"},
     .len = 4},
    {// Index 504
     .strings = (const char *[]){"quite"},
     .len = 1},
    {// Index 505
     .strings = (const char *[]){"clear", "clears", "clearing", "cleared"},
     .len = 4},
    {// Index 506
     .strings =
         (const char *[]){"practice", "practices", "practicing", "practiced"},
     .len = 4},
    {// Index 507
     .strings = (const char *[]){"piece", "pieces", "piecing", "pieced"},
     .len = 4},
    {// Index 508
     .strings = (const char *[]){"land", "lands", "landing", "landed"},
     .len = 4},
    {// Index 509
     .strings = (const char *[]){"recent"},
     .len = 1},
    {// Index 510
     .strings =
         (const char *[]){"describe", "describes", "describing", "described"},
     .len = 4},
    {// Index 511
     .strings = (const char *[]){"product", "products"},
     .len = 2},
    {// Index 512
     .strings = (const char *[]){"wall", "walls"},
     .len = 2},
    {// Index 513
     .strings = (const char *[]){"worker", "workers"},
     .len = 2},
    {// Index 514
     .strings = (const char *[]){"news"},
     .len = 1},
    {// Index 515
     .strings = (const char *[]){"test", "tests", "testing", "tested"},
     .len = 4},
    {// Index 516
     .strings = (const char *[]){"movie", "movies"},
     .len = 2},
    {// Index 517
     .strings = (const char *[]){"certain", "certainly"},
     .len = 2},
    {// Index 518
     .strings = (const char *[]){"north"},
     .len = 1},
    {// Index 519
     .strings = (const char *[]){"isn't"},
     .len = 1},
    {// Index 520
     .strings = (const char *[]){"peak"},
     .len = 1},
    {// Index 521
     .strings =
         (const char *[]){"suppose", "supposed", "supposes", "supposing"},
     .len = 4},
    {// Index 522
     .strings = (const char *[]){"jocke"},
     .len = 1},
    {// Index 523
     .strings = (const char *[]){"joakim@joakimekstrom.se"},
     .len = 1},
    {// Index 524
     .strings = (const char *[]){"instead"},
     .len = 1},
    {// Index 525
     .strings = (const char *[]){"och"},
     .len = 1},
    {// Index 526
     .strings = (const char *[]){"att"},
     .len = 1},
    {// Index 527
     .strings = (const char *[]){"det"},
     .len = 1},
    {// Index 528
     .strings = (const char *[]){"som"},
     .len = 1},
    {// Index 529
     .strings = (const char *[]){"en"},
     .len = 1},
    {// Index 530
     .strings = (const char *[]){"på"},
     .len = 1},
    {// Index 531
     .strings = (const char *[]){"är"},
     .len = 1},
    {// Index 532
     .strings = (const char *[]){"av"},
     .len = 1},
    {// Index 533
     .strings = (const char *[]){"för"},
     .len = 1},
    {// Index 534
     .strings = (const char *[]){"med"},
     .len = 1},
    {// Index 535
     .strings = (const char *[]){"till"},
     .len = 1},
    {// Index 536
     .strings = (const char *[]){"den"},
     .len = 1},
    {// Index 537
     .strings = (const char *[]){"har"},
     .len = 1},
    {// Index 538
     .strings = (const char *[]){"de"},
     .len = 1},
    {// Index 539
     .strings = (const char *[]){"inte"},
     .len = 1},
    {// Index 540
     .strings = (const char *[]){"om"},
     .len = 1},
    {// Index 541
     .strings = (const char *[]){"ett"},
     .len = 1},
    {// Index 542
     .strings = (const char *[]){"han"},
     .len = 1},
    {// Index 543
     .strings = (const char *[]){"var"},
     .len = 1},
    {// Index 544
     .strings = (const char *[]){"jag"},
     .len = 1},
    {// Index 545
     .strings = (const char *[]){"sig"},
     .len = 1},
    {// Index 546
     .strings = (const char *[]){"från"},
     .len = 1},
    {// Index 547
     .strings = (const char *[]){"vi"},
     .len = 1},
    {// Index 548
     .strings = (const char *[]){"så"},
     .len = 1},
    {// Index 549
     .strings = (const char *[]){"kan"},
     .len = 1},
    {// Index 550
     .strings = (const char *[]){"när"},
     .len = 1},
    {// Index 551
     .strings = (const char *[]){"år"},
     .len = 1},
    {// Index 552
     .strings = (const char *[]){"säger"},
     .len = 1},
    {// Index 553
     .strings = (const char *[]){"hon"},
     .len = 1},
    {// Index 554
     .strings = (const char *[]){"också"},
     .len = 1},
    {// Index 555
     .strings = (const char *[]){"efter"},
     .len = 1},
    {// Index 556
     .strings = (const char *[]){"eller"},
     .len = 1},
    {// Index 557
     .strings = (const char *[]){"nu"},
     .len = 1},
    {// Index 558
     .strings = (const char *[]){"sin"},
     .len = 1},
    {// Index 559
     .strings = (const char *[]){"där"},
     .len = 1},
    {// Index 560
     .strings = (const char *[]){"vid"},
     .len = 1},
    {// Index 561
     .strings = (const char *[]){"mot"},
     .len = 1},
    {// Index 562
     .strings = (const char *[]){"ska"},
     .len = 1},
    {// Index 563
     .strings = (const char *[]){"skulle"},
     .len = 1},
    {// Index 564
     .strings = (const char *[]){"kommer"},
     .len = 1},
    {// Index 565
     .strings = (const char *[]){"ut"},
     .len = 1},
    {// Index 566
     .strings = (const char *[]){"får"},
     .len = 1},
    {// Index 567
     .strings = (const char *[]){"finns"},
     .len = 1},
    {// Index 568
     .strings = (const char *[]){"vara"},
     .len = 1},
    {// Index 569
     .strings = (const char *[]){"tror"},
     .len = 1},
    {// Index 570
     .strings = (const char *[]){"hade"},
     .len = 1},
    {// Index 571
     .strings = (const char *[]){"alla"},
     .len = 1},
    {// Index 572
     .strings = (const char *[]){"andra"},
     .len = 1},
    {// Index 573
     .strings = (const char *[]){"mycket"},
     .len = 1},
    {// Index 574
     .strings = (const char *[]){"än"},
     .len = 1},
    {// Index 575
     .strings = (const char *[]){"här"},
     .len = 1},
    {// Index 576
     .strings = (const char *[]){"då"},
     .len = 1},
    {// Index 577
     .strings = (const char *[]){"sedan"},
     .len = 1},
    {// Index 578
     .strings = (const char *[]){"över"},
     .len = 1},
    {// Index 579
     .strings = (const char *[]){"bara"},
     .len = 1},
    {// Index 580
     .strings = (const char *[]){"blir"},
     .len = 1},
    {// Index 581
     .strings = (const char *[]){"upp"},
     .len = 1},
    {// Index 582
     .strings = (const char *[]){"även"},
     .len = 1},
    {// Index 583
     .strings = (const char *[]){"vad"},
     .len = 1},
    {// Index 584
     .strings = (const char *[]){"få"},
     .len = 1},
    {// Index 585
     .strings = (const char *[]){"två"},
     .len = 1},
    {// Index 586
     .strings = (const char *[]){"vill"},
     .len = 1},
    {// Index 587
     .strings = (const char *[]){"ha"},
     .len = 1},
    {// Index 588
     .strings = (const char *[]){"många"},
     .len = 1},
    {// Index 589
     .strings = (const char *[]){"hur"},
     .len = 1},
    {// Index 590
     .strings = (const char *[]){"mer"},
     .len = 1},
    {// Index 591
     .strings = (const char *[]){"går"},
     .len = 1},
    {// Index 592
     .strings = (const char *[]){"sverige"},
     .len = 1},
    {// Index 593
     .strings = (const char *[]){"kronor"},
     .len = 1},
    {// Index 594
     .strings = (const char *[]){"detta"},
     .len = 1},
    {// Index 595
     .strings = (const char *[]){"nya"},
     .len = 1},
    {// Index 596
     .strings = (const char *[]){"procent"},
     .len = 1},
    {// Index 597
     .strings = (const char *[]){"hans"},
     .len = 1},
    {// Index 598
     .strings = (const char *[]){"utan"},
     .len = 1},
    {// Index 599
     .strings = (const char *[]){"sina"},
     .len = 1},
    {// Index 600
     .strings = (const char *[]){"något"},
     .len = 1},
    {// Index 601
     .strings = (const char *[]){"svenska"},
     .len = 1},
    {// Index 602
     .strings = (const char *[]){"allt"},
     .len = 1},
    {// Index 603
     .strings = (const char *[]){"första"},
     .len = 1},
    {// Index 604
     .strings = (const char *[]){"fick"},
     .len = 1},
    {// Index 605
     .strings = (const char *[]){"måste"},
     .len = 1},
    {// Index 606
     .strings = (const char *[]){"mellan"},
     .len = 1},
    {// Index 607
     .strings = (const char *[]){"blev"},
     .len = 1},
    {// Index 608
     .strings = (const char *[]){"bli"},
     .len = 1},
    {// Index 609
     .strings = (const char *[]){"dag"},
     .len = 1},
    {// Index 610
     .strings = (const char *[]){"någon"},
     .len = 1},
    {// Index 611
     .strings = (const char *[]){"några"},
     .len = 1},
    {// Index 612
     .strings = (const char *[]){"sitt"},
     .len = 1},
    {// Index 613
     .strings = (const char *[]){"stor"},
     .len = 1},
    {// Index 614
     .strings = (const char *[]){"varit"},
     .len = 1},
    {// Index 615
     .strings = (const char *[]){"dem"},
     .len = 1},
    {// Index 616
     .strings = (const char *[]){"bland"},
     .len = 1},
    {// Index 617
     .strings = (const char *[]){"bra"},
     .len = 1},
    {// Index 618
     .strings = (const char *[]){"tre"},
     .len = 1},
    {// Index 619
     .strings = (const char *[]){"ta"},
     .len = 1},
    {// Index 620
     .strings = (const char *[]){"genom"},
     .len = 1},
    {// Index 621
     .strings = (const char *[]){"del"},
     .len = 1},
    {// Index 622
     .strings = (const char *[]){"hela"},
     .len = 1},
    {// Index 623
     .strings = (const char *[]){"annat"},
     .len = 1},
    {// Index 624
     .strings = (const char *[]){"fram"},
     .len = 1},
    {// Index 625
     .strings = (const char *[]){"gör"},
     .len = 1},
    {// Index 626
     .strings = (const char *[]){"ingen"},
     .len = 1},
    {// Index 627
     .strings = (const char *[]){"stockholm"},
     .len = 1},
    {// Index 628
     .strings = (const char *[]){"göra"},
     .len = 1},
    {// Index 629
     .strings = (const char *[]){"enligt"},
     .len = 1},
    {// Index 630
     .strings = (const char *[]){"mig"},
     .len = 1},
    {// Index 631
     .strings = (const char *[]){"redan"},
     .len = 1},
    {// Index 632
     .strings = (const char *[]){"inom"},
     .len = 1},
    {// Index 633
     .strings = (const char *[]){"kom"},
     .len = 1},
    {// Index 634
     .strings = (const char *[]){"du"},
     .len = 1},
    {// Index 635
     .strings = (const char *[]){"helt"},
     .len = 1},
    {// Index 636
     .strings = (const char *[]){"ju"},
     .len = 1},
    {// Index 637
     .strings = (const char *[]){"samma"},
     .len = 1},
    {// Index 638
     .strings = (const char *[]){"kanske"},
     .len = 1},
    {// Index 639
     .strings = (const char *[]){"själv"},
     .len = 1},
    {// Index 640
     .strings = (const char *[]){"oss"},
     .len = 1},
    {// Index 641
     .strings = (const char *[]){"tidigare"},
     .len = 1},
    {// Index 642
     .strings = (const char *[]){"se"},
     .len = 1},
    {// Index 643
     .strings = (const char *[]){"verkar"},
     .len = 1},
    {// Index 644
     .strings = (const char *[]){"bättre"},
     .len = 1},
    {// Index 645
     .strings = (const char *[]){"sämre"},
     .len = 1},
    {// Index 646
     .strings = (const char *[]){"sida"},
     .len = 1},
    {// Index 647
     .strings = (const char *[]){"vem"},
     .len = 1},
    {// Index 648
     .strings = (const char *[]){"varför"},
     .len = 1},
    {// Index 649
     .strings = (const char *[]){"ni"},
     .len = 1},
    {// Index 650
     .strings = (const char *[]){"vilken"},
     .len = 1},
    {// Index 651
     .strings = (const char *[]){"tid"},
     .len = 1},
    {// Index 652
     .strings = (const char *[]){"lite"},
     .len = 1},
    {// Index 653
     .strings = (const char *[]){"gå"},
     .len = 1},
};

static const size_t all_cycle_lists_len = ARRAY_SIZE(all_cycle_lists);

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
  // Define punctuation keycodes (HID Usages) that DON'T require special shift
  // handling here
  zmk_key_t punctuation_keys[] = {
      HID_USAGE_KEY_KEYBOARD_PERIOD_AND_GREATER_THAN, // .
      HID_USAGE_KEY_KEYBOARD_COMMA_AND_LESS_THAN,     // ,
      HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON, // ; (Shift for : handled by
                                                  // OS/keymap)
      // Keys requiring explicit Shift or special handling are checked
      // separately below:
      // !, ?, '
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
       HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE); // Key '\' (used for '' on SE
                                                   // layout)
  // Add more special cases here if needed

  // Core logic: If an instance was active and the key is punctuation (from
  // array) or a special case
  if (any_instance_was_active &&
      (is_punctuation || is_exclamation || is_question_mark || is_apostrophe)) {
    LOG_DBG("Punctuation/Special key (%d) pressed after active cycle string. "
            "Replacing space.",
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
    } else { // is_punctuation must be true here
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
