import json
import string

# Input and output file names
input_file = "sartak_chords.json"
output_file = "sartak_chordable_map.json"

# Load the input JSON
try:
    with open(input_file, 'r') as f:
        sartak_data = json.load(f)
except FileNotFoundError:
    print(f"Error: Input file '{input_file}' not found.")
    exit(1)
except json.JSONDecodeError:
    print(f"Error: Could not decode JSON from '{input_file}'.")
    exit(1)

word_to_chord_map = {}

# Iterate through the chord definitions in the "chords" list
if "chords" in sartak_data and isinstance(sartak_data["chords"], list):
    for item in sartak_data["chords"]:
        # Skip non-dictionary items (like comments/strings)
        if not isinstance(item, dict):
            continue

        # Get combo keys and output value, handling potential missing keys
        combo_keys = item.get("combo")
        output_val = item.get("output")

        # Skip entries missing essential data or with empty combos
        if not combo_keys or not output_val:
            continue

        # --- Process Output Word ---
        word = ""
        if isinstance(output_val, list):
            if output_val: # Ensure list is not empty
                word = output_val[0] # Take the first word if it's a list
        elif isinstance(output_val, str):
            word = output_val

        # Clean the word: remove leading \b, strip whitespace, lowercase
        word = word.lstrip('\b').strip().lower()

        # Skip if word becomes empty after cleaning
        if not word:
            continue

        # --- Process Combo Keys ---
        # Filter keys: keep only single lowercase letters
        # This excludes keys like "Dup", "Bksp", "." etc.
        filtered_combo = [
            k.lower() for k in combo_keys
            if isinstance(k, str) and len(k) == 1 and k.islower()
        ]

        # Skip if combo is empty after filtering (no valid keys found)
        if not filtered_combo:
             continue

        # Create the chord string: sorted, lowercase keys joined together
        chord_string = "".join(sorted(filtered_combo))

        # Add to map (overwrite if word already exists, last one wins)
        word_to_chord_map[word] = chord_string

else:
    print(f"Warning: Expected 'chords' key with a list value in '{input_file}'. No chords processed.")


# Prepare the final output structure, matching chordable_map.json format
output_data = {
    "words": word_to_chord_map,
    "ngrams": [] # Ngrams are not generated from this source file
}

# Save the output JSON
try:
    with open(output_file, 'w') as f:
        json.dump(output_data, f, indent=2, ensure_ascii=False) # ensure_ascii=False for potential unicode chars
    print(f"Successfully created '{output_file}' with {len(word_to_chord_map)} word chords.")
except IOError:
    print(f"Error: Could not write to output file '{output_file}'.")
    exit(1)
