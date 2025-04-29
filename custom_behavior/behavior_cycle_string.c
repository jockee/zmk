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
     .strings = (const char *[]){"you", "your", "yours"},
     .len = 3},
    {// Index 12
     .strings = (const char *[]){"it", "its"},
     .len = 2},
    {// Index 13
     .strings = (const char *[]){"have", "has", "had", "having"},
     .len = 4},
    {// Index 14
     .strings = (const char *[]){"that"},
     .len = 1},
    {// Index 15
     .strings = (const char *[]){"for"},
     .len = 1},
    {// Index 16
     .strings = (const char *[]){"do", "does", "doing", "did"},
     .len = 4},
    {// Index 17
     .strings = (const char *[]){"he", "his", "him"},
     .len = 3},
    {// Index 18
     .strings = (const char *[]){"with"},
     .len = 1},
    {// Index 19
     .strings = (const char *[]){"on"},
     .len = 1},
    {// Index 20
     .strings = (const char *[]){"this"},
     .len = 1},
    {// Index 21
     .strings = (const char *[]){"we", "our", "us", "ours"},
     .len = 4},
    {// Index 22
     .strings = (const char *[]){"not"},
     .len = 1},
    {// Index 23
     .strings = (const char *[]){"but"},
     .len = 1},
    {// Index 24
     .strings = (const char *[]){"they", "their", "them", "theirs"},
     .len = 4},
    {// Index 25
     .strings = (const char *[]){"say", "says", "saying", "said"},
     .len = 4},
    {// Index 26
     .strings = (const char *[]){"at"},
     .len = 1},
    {// Index 27
     .strings = (const char *[]){"what"},
     .len = 1},
    {// Index 28
     .strings = (const char *[]){"from"},
     .len = 1},
    {// Index 29
     .strings = (const char *[]){"go", "goes", "going", "went"},
     .len = 4},
    {// Index 30
     .strings = (const char *[]){"or"},
     .len = 1},
    {// Index 31
     .strings = (const char *[]){"by"},
     .len = 1},
    {// Index 32
     .strings = (const char *[]){"get", "gets", "getting", "got"},
     .len = 4},
    {// Index 33
     .strings = (const char *[]){"she", "her", "hers"},
     .len = 3},
    {// Index 34
     .strings = (const char *[]){"can", "could"},
     .len = 2},
    {// Index 35
     .strings = (const char *[]){"as"},
     .len = 1},
    {// Index 36
     .strings = (const char *[]){"know", "knows", "knowing", "knew"},
     .len = 4},
    {// Index 37
     .strings = (const char *[]){"if"},
     .len = 1},
    {// Index 38
     .strings = (const char *[]){"me", "my", "myself"},
     .len = 3},
    {// Index 39
     .strings = (const char *[]){"your", "yours"},
     .len = 2},
    {// Index 40
     .strings = (const char *[]){"all"},
     .len = 1},
    {// Index 41
     .strings = (const char *[]){"who"},
     .len = 1},
    {// Index 42
     .strings = (const char *[]){"about"},
     .len = 1},
    {// Index 43
     .strings = (const char *[]){"will"},
     .len = 1},
    {// Index 44
     .strings = (const char *[]){"so"},
     .len = 1},
    {// Index 45
     .strings = (const char *[]){"would"},
     .len = 1},
    {// Index 46
     .strings = (const char *[]){"make", "makes", "making", "made"},
     .len = 4},
    {// Index 47
     .strings = (const char *[]){"just"},
     .len = 1},
    {// Index 48
     .strings = (const char *[]){"up"},
     .len = 1},
    {// Index 49
     .strings = (const char *[]){"think", "thinks", "thinking", "thought"},
     .len = 4},
    {// Index 50
     .strings = (const char *[]){"time", "times"},
     .len = 2},
    {// Index 51
     .strings = (const char *[]){"there"},
     .len = 1},
    {// Index 52
     .strings = (const char *[]){"see", "sees", "seeing", "saw"},
     .len = 4},
    {// Index 53
     .strings = (const char *[]){"out"},
     .len = 1},
    {// Index 54
     .strings = (const char *[]){"one"},
     .len = 1},
    {// Index 55
     .strings = (const char *[]){"come", "comes", "coming", "came"},
     .len = 4},
    {// Index 56
     .strings = (const char *[]){"people"},
     .len = 1},
    {// Index 57
     .strings = (const char *[]){"take", "takes", "taking", "took"},
     .len = 4},
    {// Index 58
     .strings = (const char *[]){"year", "years"},
     .len = 2},
    {// Index 59
     .strings = (const char *[]){"him", "his"},
     .len = 2},
    {// Index 60
     .strings = (const char *[]){"them", "their"},
     .len = 2},
    {// Index 61
     .strings = (const char *[]){"some"},
     .len = 1},
    {// Index 62
     .strings = (const char *[]){"want", "wants", "wanting", "wanted"},
     .len = 4},
    {// Index 63
     .strings = (const char *[]){"how"},
     .len = 1},
    {// Index 64
     .strings = (const char *[]){"when"},
     .len = 1},
    {// Index 65
     .strings = (const char *[]){"which"},
     .len = 1},
    {// Index 66
     .strings = (const char *[]){"now"},
     .len = 1},
    {// Index 67
     .strings = (const char *[]){"like", "likes", "liking", "liked"},
     .len = 4},
    {// Index 68
     .strings = (const char *[]){"other", "others"},
     .len = 2},
    {// Index 69
     .strings = (const char *[]){"our", "ours"},
     .len = 2},
    {// Index 70
     .strings = (const char *[]){"into"},
     .len = 1},
    {// Index 71
     .strings = (const char *[]){"here"},
     .len = 1},
    {// Index 72
     .strings = (const char *[]){"then"},
     .len = 1},
    {// Index 73
     .strings = (const char *[]){"than"},
     .len = 1},
    {// Index 74
     .strings = (const char *[]){"look", "looks", "looking", "looked"},
     .len = 4},
    {// Index 75
     .strings = (const char *[]){"way", "ways"},
     .len = 2},
    {// Index 76
     .strings = (const char *[]){"more"},
     .len = 1},
    {// Index 77
     .strings = (const char *[]){"these"},
     .len = 1},
    {// Index 78
     .strings = (const char *[]){"no"},
     .len = 1},
    {// Index 79
     .strings = (const char *[]){"thing", "things"},
     .len = 2},
    {// Index 80
     .strings = (const char *[]){"well", "better", "best"},
     .len = 3},
    {// Index 81
     .strings = (const char *[]){"because"},
     .len = 1},
    {// Index 82
     .strings = (const char *[]){"also"},
     .len = 1},
    {// Index 83
     .strings = (const char *[]){"two"},
     .len = 1},
    {// Index 84
     .strings = (const char *[]){"use", "uses", "using", "used"},
     .len = 4},
    {// Index 85
     .strings = (const char *[]){"tell", "tells", "telling", "told"},
     .len = 4},
    {// Index 86
     .strings = (const char *[]){"good", "better", "best"},
     .len = 3},
    {// Index 87
     .strings = (const char *[]){"first"},
     .len = 1},
    {// Index 88
     .strings = (const char *[]){"man", "men"},
     .len = 2},
    {// Index 89
     .strings = (const char *[]){"day", "days"},
     .len = 2},
    {// Index 90
     .strings = (const char *[]){"find", "finds", "finding", "found"},
     .len = 4},
    {// Index 91
     .strings = (const char *[]){"give", "gives", "giving", "gave"},
     .len = 4},
    {// Index 92
     .strings = (const char *[]){"new", "newer", "newest"},
     .len = 3},
    {// Index 93
     .strings = (const char *[]){"us"},
     .len = 1},
    {// Index 94
     .strings = (const char *[]){"any"},
     .len = 1},
    {// Index 95
     .strings = (const char *[]){"those"},
     .len = 1},
    {// Index 96
     .strings = (const char *[]){"very"},
     .len = 1},
    {// Index 97
     .strings = (const char *[]){"need", "needs", "needing", "needed"},
     .len = 4},
    {// Index 98
     .strings = (const char *[]){"back", "backs", "backing", "backed"},
     .len = 4},
    {// Index 99
     .strings = (const char *[]){"should"},
     .len = 1},
    {// Index 100
     .strings = (const char *[]){"even"},
     .len = 1},
    {// Index 101
     .strings = (const char *[]){"only"},
     .len = 1},
    {// Index 102
     .strings = (const char *[]){"many"},
     .len = 1},
    {// Index 103
     .strings = (const char *[]){"really"},
     .len = 1},
    {// Index 104
     .strings = (const char *[]){"work", "works", "working", "worked"},
     .len = 4},
    {// Index 105
     .strings = (const char *[]){"life", "lives"},
     .len = 2},
    {// Index 106
     .strings = (const char *[]){"why"},
     .len = 1},
    {// Index 107
     .strings = (const char *[]){"right", "rights"},
     .len = 2},
    {// Index 108
     .strings = (const char *[]){"down"},
     .len = 1},
    {// Index 109
     .strings = (const char *[]){"try", "tries", "trying", "tried"},
     .len = 4},
    {// Index 110
     .strings = (const char *[]){"let", "lets", "letting", "let"},
     .len = 4},
    {// Index 111
     .strings = (const char *[]){"something"},
     .len = 1},
    {// Index 112
     .strings = (const char *[]){"too"},
     .len = 1},
    {// Index 113
     .strings = (const char *[]){"call", "calls", "calling", "called"},
     .len = 4},
    {// Index 114
     .strings = (const char *[]){"woman", "women"},
     .len = 2},
    {// Index 115
     .strings = (const char *[]){"may"},
     .len = 1},
    {// Index 116
     .strings = (const char *[]){"still"},
     .len = 1},
    {// Index 117
     .strings = (const char *[]){"through"},
     .len = 1},
    {// Index 118
     .strings = (const char *[]){"mean", "means", "meaning", "meant"},
     .len = 4},
    {// Index 119
     .strings = (const char *[]){"after"},
     .len = 1},
    {// Index 120
     .strings = (const char *[]){"never"},
     .len = 1},
    {// Index 121
     .strings = (const char *[]){"world", "worlds"},
     .len = 2},
    {// Index 122
     .strings = (const char *[]){"feel", "feels", "feeling", "felt"},
     .len = 4},
    {// Index 123
     .strings = (const char *[]){"great", "greater", "greatest"},
     .len = 3},
    {// Index 124
     .strings = (const char *[]){"last"},
     .len = 1},
    {// Index 125
     .strings = (const char *[]){"child", "children"},
     .len = 2},
    {// Index 126
     .strings = (const char *[]){"oh"},
     .len = 1},
    {// Index 127
     .strings = (const char *[]){"ah"},
     .len = 1},
    {// Index 128
     .strings = (const char *[]){"over"},
     .len = 1},
    {// Index 129
     .strings = (const char *[]){"ask", "asks", "asking", "asked"},
     .len = 4},
    {// Index 130
     .strings = (const char *[]){"state", "states", "stated", "stating"},
     .len = 4},
    {// Index 131
     .strings = (const char *[]){"much"},
     .len = 1},
    {// Index 132
     .strings = (const char *[]){"talk", "talks", "talking", "talked"},
     .len = 4},
    {// Index 133
     .strings = (const char *[]){"keep", "keeps", "keeping", "kept"},
     .len = 4},
    {// Index 134
     .strings = (const char *[]){"leave", "leaves", "leaving", "left"},
     .len = 4},
    {// Index 135
     .strings = (const char *[]){"put", "puts", "putting", "put"},
     .len = 4},
    {// Index 136
     .strings = (const char *[]){"help", "helps", "helping", "helped"},
     .len = 4},
    {// Index 137
     .strings = (const char *[]){"big", "bigger", "biggest"},
     .len = 3},
    {// Index 138
     .strings = (const char *[]){"where"},
     .len = 1},
    {// Index 139
     .strings = (const char *[]){"same"},
     .len = 1},
    {// Index 140
     .strings = (const char *[]){"own", "owns", "owning", "owned"},
     .len = 4},
    {// Index 141
     .strings = (const char *[]){"while"},
     .len = 1},
    {// Index 142
     .strings = (const char *[]){"start", "starts", "starting", "started"},
     .len = 4},
    {// Index 143
     .strings = (const char *[]){"three"},
     .len = 1},
    {// Index 144
     .strings = (const char *[]){"high", "higher", "highest"},
     .len = 3},
    {// Index 145
     .strings = (const char *[]){"every"},
     .len = 1},
    {// Index 146
     .strings = (const char *[]){"another"},
     .len = 1},
    {// Index 147
     .strings = (const char *[]){"become", "becomes", "becoming", "became"},
     .len = 4},
    {// Index 148
     .strings = (const char *[]){"most"},
     .len = 1},
    {// Index 149
     .strings = (const char *[]){"between"},
     .len = 1},
    {// Index 150
     .strings = (const char *[]){"happen", "happens", "happening", "happened"},
     .len = 4},
    {// Index 151
     .strings = (const char *[]){"family", "families"},
     .len = 2},
    {// Index 152
     .strings = (const char *[]){"old", "older", "oldest"},
     .len = 3},
    {// Index 153
     .strings = (const char *[]){"yes"},
     .len = 1},
    {// Index 154
     .strings = (const char *[]){"house", "houses"},
     .len = 2},
    {// Index 155
     .strings = (const char *[]){"show", "shows", "showing", "showed"},
     .len = 4},
    {// Index 156
     .strings = (const char *[]){"again"},
     .len = 1},
    {// Index 157
     .strings = (const char *[]){"student", "students"},
     .len = 2},
    {// Index 158
     .strings = (const char *[]){"seem", "seems", "seeming", "seemed"},
     .len = 4},
    {// Index 159
     .strings = (const char *[]){"might"},
     .len = 1},
    {// Index 160
     .strings = (const char *[]){"part", "parts"},
     .len = 2},
    {// Index 161
     .strings = (const char *[]){"hear", "hears", "hearing", "heard"},
     .len = 4},
    {// Index 162
     .strings = (const char *[]){"its"},
     .len = 1},
    {// Index 163
     .strings = (const char *[]){"place", "places", "placing", "placed"},
     .len = 4},
    {// Index 164
     .strings = (const char *[]){"problem", "problems"},
     .len = 2},
    {// Index 165
     .strings =
         (const char *[]){"believe", "believes", "believing", "believed"},
     .len = 4},
    {// Index 166
     .strings = (const char *[]){"country", "countries"},
     .len = 2},
    {// Index 167
     .strings = (const char *[]){"always"},
     .len = 1},
    {// Index 168
     .strings = (const char *[]){"week", "weeks"},
     .len = 2},
    {// Index 169
     .strings = (const char *[]){"point", "points", "pointing", "pointed"},
     .len = 4},
    {// Index 170
     .strings = (const char *[]){"hand", "hands", "handing", "handed"},
     .len = 4},
    {// Index 171
     .strings = (const char *[]){"off"},
     .len = 1},
    {// Index 172
     .strings = (const char *[]){"play", "plays", "playing", "played"},
     .len = 4},
    {// Index 173
     .strings = (const char *[]){"turn", "turns", "turning", "turned"},
     .len = 4},
    {// Index 174
     .strings = (const char *[]){"few", "fewer", "fewest"},
     .len = 3},
    {// Index 175
     .strings = (const char *[]){"group", "groups", "grouping", "grouped"},
     .len = 4},
    {// Index 176
     .strings = (const char *[]){"such"},
     .len = 1},
    {// Index 177
     .strings = (const char *[]){"fact", "facts"},
     .len = 2},
    {// Index 178
     .strings = (const char *[]){"against"},
     .len = 1},
    {// Index 179
     .strings = (const char *[]){"case", "cases"},
     .len = 2},
    {// Index 180
     .strings = (const char *[]){"around"},
     .len = 1},
    {// Index 181
     .strings = (const char *[]){"company", "companies"},
     .len = 2},
    {// Index 182
     .strings = (const char *[]){"system", "systems"},
     .len = 2},
    {// Index 183
     .strings = (const char *[]){"each"},
     .len = 1},
    {// Index 184
     .strings = (const char *[]){"program", "programs"},
     .len = 2},
    {// Index 185
     .strings = (const char *[]){"question", "questions"},
     .len = 2},
    {// Index 186
     .strings = (const char *[]){"during"},
     .len = 1},
    {// Index 187
     .strings = (const char *[]){"run", "runs", "running", "ran"},
     .len = 4},
    {// Index 188
     .strings = (const char *[]){"small", "smaller", "smallest"},
     .len = 3},
    {// Index 189
     .strings = (const char *[]){"number", "numbers"},
     .len = 2},
    {// Index 190
     .strings = (const char *[]){"move", "moves", "moving", "moved"},
     .len = 4},
    {// Index 191
     .strings = (const char *[]){"night", "nights"},
     .len = 2},
    {// Index 192
     .strings = (const char *[]){"live", "lives", "living", "lived"},
     .len = 4},
    {// Index 193
     .strings = (const char *[]){"hold", "holds", "holding", "held"},
     .len = 4},
    {// Index 194
     .strings = (const char *[]){"today"},
     .len = 1},
    {// Index 195
     .strings = (const char *[]){"bring", "brings", "bringing", "brought"},
     .len = 4},
    {// Index 196
     .strings = (const char *[]){"next"},
     .len = 1},
    {// Index 197
     .strings = (const char *[]){"without"},
     .len = 1},
    {// Index 198
     .strings = (const char *[]){"before"},
     .len = 1},
    {// Index 199
     .strings = (const char *[]){"large", "larger", "largest"},
     .len = 3},
    {// Index 200
     .strings = (const char *[]){"must"},
     .len = 1},
    {// Index 201
     .strings = (const char *[]){"home", "homes"},
     .len = 2},
    {// Index 202
     .strings = (const char *[]){"under"},
     .len = 1},
    {// Index 203
     .strings = (const char *[]){"water"},
     .len = 1},
    {// Index 204
     .strings = (const char *[]){"room", "rooms"},
     .len = 2},
    {// Index 205
     .strings = (const char *[]){"write", "writes", "writing", "wrote"},
     .len = 4},
    {// Index 206
     .strings = (const char *[]){"area", "areas"},
     .len = 2},
    {// Index 207
     .strings = (const char *[]){"national"},
     .len = 1},
    {// Index 208
     .strings = (const char *[]){"money"},
     .len = 1},
    {// Index 209
     .strings = (const char *[]){"story", "stories"},
     .len = 2},
    {// Index 210
     .strings = (const char *[]){"young", "younger", "youngest"},
     .len = 3},
    {// Index 211
     .strings = (const char *[]){"month", "months"},
     .len = 2},
    {// Index 212
     .strings = (const char *[]){"different"},
     .len = 1},
    {// Index 213
     .strings = (const char *[]){"lot", "lots"},
     .len = 2},
    {// Index 214
     .strings = (const char *[]){"study", "studies", "studying", "studied"},
     .len = 4},
    {// Index 215
     .strings = (const char *[]){"book", "books"},
     .len = 2},
    {// Index 216
     .strings = (const char *[]){"job", "jobs"},
     .len = 2},
    {// Index 217
     .strings = (const char *[]){"word", "words"},
     .len = 2},
    {// Index 218
     .strings = (const char *[]){"though"},
     .len = 1},
    {// Index 219
     .strings = (const char *[]){"business", "businesses"},
     .len = 2},
    {// Index 220
     .strings = (const char *[]){"issue", "issues"},
     .len = 2},
    {// Index 221
     .strings = (const char *[]){"side", "sides"},
     .len = 2},
    {// Index 222
     .strings = (const char *[]){"kind", "kinds"},
     .len = 2},
    {// Index 223
     .strings = (const char *[]){"four"},
     .len = 1},
    {// Index 224
     .strings = (const char *[]){"head", "heads", "heading", "headed"},
     .len = 4},
    {// Index 225
     .strings = (const char *[]){"far", "farther", "farthest"},
     .len = 3},
    {// Index 226
     .strings = (const char *[]){"black", "blacks", "blacked", "blacking"},
     .len = 4},
    {// Index 227
     .strings = (const char *[]){"long", "longer", "longest"},
     .len = 3},
    {// Index 228
     .strings = (const char *[]){"both"},
     .len = 1},
    {// Index 229
     .strings = (const char *[]){"little", "less", "least"},
     .len = 3},
    {// Index 230
     .strings = (const char *[]){"since"},
     .len = 1},
    {// Index 231
     .strings =
         (const char *[]){"provide", "provides", "providing", "provided"},
     .len = 4},
    {// Index 232
     .strings = (const char *[]){"service", "services"},
     .len = 2},
    {// Index 233
     .strings = (const char *[]){"friend", "friends", "friending", "friended"},
     .len = 4},
    {// Index 234
     .strings = (const char *[]){"important"},
     .len = 1},
    {// Index 235
     .strings = (const char *[]){"father", "fathers"},
     .len = 2},
    {// Index 236
     .strings = (const char *[]){"sit", "sits", "sitting", "sat"},
     .len = 4},
    {// Index 237
     .strings = (const char *[]){"away"},
     .len = 1},
    {// Index 238
     .strings = (const char *[]){"until"},
     .len = 1},
    {// Index 239
     .strings = (const char *[]){"power", "powers", "powering", "powered"},
     .len = 4},
    {// Index 240
     .strings = (const char *[]){"hour", "hours"},
     .len = 2},
    {// Index 241
     .strings = (const char *[]){"game", "games"},
     .len = 2},
    {// Index 242
     .strings = (const char *[]){"often"},
     .len = 1},
    {// Index 243
     .strings = (const char *[]){"yet"},
     .len = 1},
    {// Index 244
     .strings = (const char *[]){"line", "lines", "lining", "lined"},
     .len = 4},
    {// Index 245
     .strings = (const char *[]){"end", "ends", "ending", "ended"},
     .len = 4},
    {// Index 246
     .strings = (const char *[]){"among"},
     .len = 1},
    {// Index 247
     .strings = (const char *[]){"ever"},
     .len = 1},
    {// Index 248
     .strings = (const char *[]){"stand", "stands", "standing", "stood"},
     .len = 4},
    {// Index 249
     .strings = (const char *[]){"bad", "worse", "worst"},
     .len = 3},
    {// Index 250
     .strings = (const char *[]){"lose", "loses", "losing", "lost"},
     .len = 4},
    {// Index 251
     .strings = (const char *[]){"however"},
     .len = 1},
    {// Index 252
     .strings = (const char *[]){"member", "members"},
     .len = 2},
    {// Index 253
     .strings = (const char *[]){"pay", "pays", "paying", "paid"},
     .len = 4},
    {// Index 254
     .strings = (const char *[]){"law", "laws"},
     .len = 2},
    {// Index 255
     .strings = (const char *[]){"meet", "meets", "meeting", "met"},
     .len = 4},
    {// Index 256
     .strings = (const char *[]){"car", "cars"},
     .len = 2},
    {// Index 257
     .strings = (const char *[]){"city", "cities"},
     .len = 2},
    {// Index 258
     .strings = (const char *[]){"almost"},
     .len = 1},
    {// Index 259
     .strings =
         (const char *[]){"include", "includes", "including", "included"},
     .len = 4},
    {// Index 260
     .strings =
         (const char *[]){"continue", "continues", "continuing", "continued"},
     .len = 4},
    {// Index 261
     .strings = (const char *[]){"set", "sets", "setting", "set"},
     .len = 4},
    {// Index 262
     .strings = (const char *[]){"later"},
     .len = 1},
    {// Index 263
     .strings = (const char *[]){"community", "communities"},
     .len = 2},
    {// Index 264
     .strings = (const char *[]){"name", "names", "naming", "named"},
     .len = 4},
    {// Index 265
     .strings = (const char *[]){"five"},
     .len = 1},
    {// Index 266
     .strings = (const char *[]){"once"},
     .len = 1},
    {// Index 267
     .strings = (const char *[]){"white", "whiter", "whitest"},
     .len = 3},
    {// Index 268
     .strings = (const char *[]){"least"},
     .len = 1},
    {// Index 269
     .strings = (const char *[]){"learn", "learns", "learning", "learned"},
     .len = 4},
    {// Index 270
     .strings = (const char *[]){"real"},
     .len = 1},
    {// Index 271
     .strings = (const char *[]){"change", "changes", "changing", "changed"},
     .len = 4},
    {// Index 272
     .strings = (const char *[]){"team", "teams"},
     .len = 2},
    {// Index 273
     .strings = (const char *[]){"minute", "minutes"},
     .len = 2},
    {// Index 274
     .strings = (const char *[]){"several"},
     .len = 1},
    {// Index 275
     .strings = (const char *[]){"idea", "ideas"},
     .len = 2},
    {// Index 276
     .strings = (const char *[]){"kid", "kids"},
     .len = 2},
    {// Index 277
     .strings = (const char *[]){"body", "bodies"},
     .len = 2},
    {// Index 278
     .strings = (const char *[]){"information"},
     .len = 1},
    {// Index 279
     .strings = (const char *[]){"nothing"},
     .len = 1},
    {// Index 280
     .strings = (const char *[]){"ago"},
     .len = 1},
    {// Index 281
     .strings = (const char *[]){"lead", "leads", "leading", "led"},
     .len = 4},
    {// Index 282
     .strings = (const char *[]){"social"},
     .len = 1},
    {// Index 283
     .strings = (const char *[]){"understand", "understands", "understanding",
                                 "understood"},
     .len = 4},
    {// Index 284
     .strings = (const char *[]){"whether"},
     .len = 1},
    {// Index 285
     .strings = (const char *[]){"watch", "watches", "watching", "watched"},
     .len = 4},
    {// Index 286
     .strings = (const char *[]){"together"},
     .len = 1},
    {// Index 287
     .strings = (const char *[]){"follow", "follows", "following", "followed"},
     .len = 4},
    {// Index 288
     .strings = (const char *[]){"parent", "parents"},
     .len = 2},
    {// Index 289
     .strings = (const char *[]){"stop", "stops", "stopping", "stopped"},
     .len = 4},
    {// Index 290
     .strings = (const char *[]){"face", "faces", "facing", "faced"},
     .len = 4},
    {// Index 291
     .strings = (const char *[]){"create", "creates", "creating", "created"},
     .len = 4},
    {// Index 292
     .strings = (const char *[]){"public"},
     .len = 1},
    {// Index 293
     .strings = (const char *[]){"already"},
     .len = 1},
    {// Index 294
     .strings = (const char *[]){"speak", "speaks", "speaking", "spoke"},
     .len = 4},
    {// Index 295
     .strings = (const char *[]){"read", "reads", "reading", "read"},
     .len = 4},
    {// Index 296
     .strings = (const char *[]){"level", "levels", "leveling", "leveled"},
     .len = 4},
    {// Index 297
     .strings = (const char *[]){"allow", "allows", "allowing", "allowed"},
     .len = 4},
    {// Index 298
     .strings = (const char *[]){"add", "adds", "adding", "added"},
     .len = 4},
    {// Index 299
     .strings = (const char *[]){"office", "offices"},
     .len = 2},
    {// Index 300
     .strings = (const char *[]){"spend", "spends", "spending", "spent"},
     .len = 4},
    {// Index 301
     .strings = (const char *[]){"door", "doors"},
     .len = 2},
    {// Index 302
     .strings = (const char *[]){"health"},
     .len = 1},
    {// Index 303
     .strings = (const char *[]){"person", "people"},
     .len = 2},
    {// Index 304
     .strings = (const char *[]){"sure", "surer", "surest"},
     .len = 3},
    {// Index 305
     .strings = (const char *[]){"history", "histories"},
     .len = 2},
    {// Index 306
     .strings = (const char *[]){"party", "parties"},
     .len = 2},
    {// Index 307
     .strings = (const char *[]){"within"},
     .len = 1},
    {// Index 308
     .strings = (const char *[]){"grow", "grows", "growing", "grew"},
     .len = 4},
    {// Index 309
     .strings = (const char *[]){"result", "results", "resulting", "resulted"},
     .len = 4},
    {// Index 310
     .strings = (const char *[]){"open", "opens", "opening", "opened"},
     .len = 4},
    {// Index 311
     .strings = (const char *[]){"morning", "mornings"},
     .len = 2},
    {// Index 312
     .strings = (const char *[]){"walk", "walks", "walking", "walked"},
     .len = 4},
    {// Index 313
     .strings = (const char *[]){"reason", "reasons", "reasoning", "reasoned"},
     .len = 4},
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
  struct behavior_cycle_string_state *state = dev->data;

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
