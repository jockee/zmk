#ifndef CUSTOM_BEHAVIOR_CYCLE_STRING_LISTS_H
#define CUSTOM_BEHAVIOR_CYCLE_STRING_LISTS_H

#include <stddef.h> // For size_t
#include <zephyr/sys/util.h> // For ARRAY_SIZE

// Structure to hold a cycle list and its length
typedef struct {
  const char **strings;
  const size_t len;
} cycle_list_t;

// Define your actual string lists here.
// Example:
// static const char *list_0_strings[] = {"string1", "string2"};
// static const char *list_1_strings[] = {"cycleA", "cycleB", "cycleC"};
// ... add all your lists

// Define the main array containing pointers to all lists and their lengths.
// IMPORTANT: The order here MUST match the list_index parameter used in your keymap.
static const cycle_list_t all_cycle_lists[] = {
    // Example:
    // { .strings = list_0_strings, .len = ARRAY_SIZE(list_0_strings) },
    // { .strings = list_1_strings, .len = ARRAY_SIZE(list_1_strings) },
    // Add entries for all your defined lists above
};

// Calculate the total number of lists defined.
static const size_t all_cycle_lists_len = ARRAY_SIZE(all_cycle_lists);

#endif // CUSTOM_BEHAVIOR_CYCLE_STRING_LISTS_H
