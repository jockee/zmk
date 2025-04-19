#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path

# --- Configuration ---
KEYMAP_FILE = Path(__file__).parent.parent / "config" / "glove80.keymap"
CHORD_MAP_FILE = Path(__file__).parent / "chordable_map.json"
OUTPUT_MACROS_FILE = Path(__file__).parent.parent / "config" / "generated_macros.dtsi"
OUTPUT_COMBOS_FILE = Path(__file__).parent.parent / "config" / "generated_combos.dtsi"
DEFAULT_COMBO_TIMEOUT = 50 # ms, timeout for combos # <<< Adjusted timeout as per your diff

# --- ZMK Keycode Mapping ---
# Maps lowercase characters/symbols to ZMK keycodes used in &kp bindings
# Ensure this covers all characters used in your words/ngrams AND present on your base layer
ZMK_KEYCODE_MAP = {
    'a': 'A', 'b': 'B', 'c': 'C', 'd': 'D', 'e': 'E', 'f': 'F', 'g': 'G',
    'h': 'H', 'i': 'I', 'j': 'J', 'k': 'K', 'l': 'L', 'm': 'M', 'n': 'N',
    'o': 'O', 'p': 'P', 'q': 'Q', 'r': 'R', 's': 'S', 't': 'T', 'u': 'U',
    'v': 'V', 'w': 'W', 'x': 'X', 'y': 'Y', 'z': 'Z',
    # Remove the Swedish character mappings
    # 'å': 'LBKT',  # Using Left Bracket based on keymap inspection (was RBKT, but LBKT is unused)
    # 'ä': 'SQT',   # Map to single quote key
    # 'ö': 'SEMI',  # Map to semicolon key
    '1': 'N1', '2': 'N2', '3': 'N3', '4': 'N4', '5': 'N5',
    '6': 'N6', '7': 'N7', '8': 'N8', '9': 'N9', '0': 'N0',
    ';': 'SEMI', '=': 'EQUAL', ',': 'COMMA', '.': 'DOT', '/': 'FSLH',
    '[': 'LBKT', ']': 'RBKT', "'": 'SQT', '-': 'MINUS', '`': 'GRAVE',
    '\\': 'BSLH', # Or NON_US_BSLH depending on your base layer definition
    # Add any other symbols present on your base layer if used in chords
}

# --- Helper Functions ---
def parse_keymap_for_positions(keymap_path: Path) -> dict[str, int]:
    """Parses the base layer of a ZMK keymap file to map keys to positions."""
    print(f"Parsing keymap: {keymap_path}")
    if not keymap_path.exists():
        print(f"Error: Keymap file not found at {keymap_path}", file=sys.stderr)
        sys.exit(1)

    try:
        keymap_content = keymap_path.read_text(encoding="utf-8")
    except IOError as e:
        print(f"Error reading keymap file {keymap_path}: {e}", file=sys.stderr)
        sys.exit(1)

    # --- Create inverse map from ZMK_KEYCODE_MAP ---
    # Maps ZMK Keycode (e.g., 'A', 'SEMI') back to character (e.g., 'a', ';')
    keycode_to_char = {v: k for k, v in ZMK_KEYCODE_MAP.items()}

    # --- Extract base layer bindings ---
    # Regex to find the bindings array within the layer_Base definition
    base_layer_match = re.search(
        r"layer_Base\s*\{.*?bindings\s*=\s*<\s*(.*?)\s*>\s*;",
        keymap_content,
        re.DOTALL | re.IGNORECASE
    )
    if not base_layer_match:
        print(f"Error: Could not find layer_Base bindings in {keymap_path}", file=sys.stderr)
        sys.exit(1)

    bindings_str = base_layer_match.group(1)
    # Remove C-style comments /* ... */
    bindings_str = re.sub(r"/\*.*?\*/", "", bindings_str, flags=re.DOTALL)
    # Remove C++-style comments // ...
    bindings_str = re.sub(r"//.*", "", bindings_str)
    # Split into individual bindings, handles multiple spaces/newlines
    # bindings = bindings_str.split() # OLD METHOD - Incorrect for multi-part bindings
    bindings_str = bindings_str.strip() # Remove leading/trailing whitespace

    key_positions = {}
    position_index = 0
    # Assuming Glove80 has 80 keys (positions 0-79) based on keymap #defines
    max_expected_positions = 80

    # --- New Parsing Logic using regex ---
    # Find all starting points of bindings (& followed by word chars)
    binding_starts = list(re.finditer(r"&\w+", bindings_str))

    print(f"Found {len(binding_starts)} potential binding starts.")

    # Check if the number of found bindings matches expectations
    if len(binding_starts) != max_expected_positions:
         print(f"Warning: Found {len(binding_starts)} binding starts, but expected {max_expected_positions}. Keymap structure might be unusual or parsing may be inaccurate.", file=sys.stderr)
         # If the count is drastically wrong, it might be better to abort
         # if abs(len(binding_starts) - max_expected_positions) > 5: # Example threshold
         #     print("Error: Significant mismatch in expected binding count. Aborting.", file=sys.stderr)
         #     sys.exit(1)

    for i, match in enumerate(binding_starts):
        if position_index >= max_expected_positions:
            print(f"Warning: Processed {max_expected_positions} bindings. Stopping parse.", file=sys.stderr)
            break

        # Determine the full binding string for this position
        start_pos = match.start()
        end_pos = binding_starts[i+1].start() if i + 1 < len(binding_starts) else len(bindings_str)
        full_binding = bindings_str[start_pos:end_pos].strip()

        # Check if this full binding is a &kp binding
        kp_match = re.match(r"&kp\s+([A-Z0-9_]+)", full_binding)
        if kp_match:
            keycode = kp_match.group(1)
            char = keycode_to_char.get(keycode)
            if char:
                # Check for duplicates - might indicate an issue in the keymap or logic
                if char in key_positions:
                    # Allow duplicate mappings if they point to the same position (unlikely but possible)
                    if key_positions[char] != position_index:
                        print(f"Warning: Character '{char}' is mapped to multiple positions ({key_positions[char]} and {position_index}). Using the first encountered: {key_positions[char]}.", file=sys.stderr)
                else:
                    key_positions[char] = position_index
            # else: # Optional: Warn if a keycode doesn't map back to a known char
            #     print(f"Debug: Keycode '{keycode}' at position {position_index} not found in ZMK_KEYCODE_MAP inverse.", file=sys.stderr)

        # Increment position index for every binding start found
        position_index += 1

    # This warning might be less relevant now if we iterate based on found bindings
    # if position_index < max_expected_positions:
    #      print(f"Warning: Processed only {position_index} bindings, expected {max_expected_positions}.", file=sys.stderr)

    # Debug output
    print(f"Successfully mapped {len(key_positions)} unique keys to positions from keymap.")
    print("Key position mapping:")
    for char, pos in sorted(key_positions.items()):
        print(f"  '{char}' -> position {pos}")

    return key_positions

# Initialize the dictionary to track used names (outside the function)
_used_zmk_names = {}

def generate_zmk_name(base_name: str, prefix: str) -> str:
    """Creates a ZMK-compatible node name (macro or combo)."""
    global _used_zmk_names # Use a global or class member to track used names

    # Filter out Swedish characters completely
    cleaned_name = base_name.replace('å', '').replace('ä', '').replace('ö', '')
    # Replace remaining invalid chars with underscore
    cleaned_name = re.sub(r'[^a-zA-Z0-9_]', '_', cleaned_name)
    # Remove leading/trailing underscores
    cleaned_name = cleaned_name.strip('_')
    if not cleaned_name: # Handle empty string case after cleaning
        cleaned_name = "empty"

    # Construct the base ZMK name with prefix
    zmk_base_name = f"{prefix}_{cleaned_name}"

    # Ensure starts with letter
    if not zmk_base_name[0].isalpha():
         # This case should be rare now with the prefix, but handle defensively
         zmk_base_name = f'{prefix}_' + zmk_base_name[len(prefix)+1:]

    # ZMK identifiers have length limits, truncate if necessary (e.g., 31 chars typical max)
    zmk_base_name = zmk_base_name[:25]  # Leave room for uniqueness suffix

    # Note: Checking against reserved names might be less critical now with prefixes,
    # but could be added back here if needed, operating on zmk_base_name.

    # Add a unique suffix if this name has been used before
    # Use zmk_base_name for uniqueness tracking
    final_name = zmk_base_name
    if zmk_base_name in _used_zmk_names:
        counter = _used_zmk_names[zmk_base_name]
        _used_zmk_names[zmk_base_name] += 1
        final_name = f"{zmk_base_name}_{counter}" # Append suffix
    else:
        _used_zmk_names[zmk_base_name] = 1

    return final_name

def create_macro_bindings(text: str, add_space: bool) -> str:
    """Generates the ZMK macro bindings string for given text."""
    bindings = []
    for char in text:
        keycode = ZMK_KEYCODE_MAP.get(char.lower())
        if keycode:
            # Basic check for uppercase - assumes simple shift behavior
            # More complex shifting (e.g., layer shifts) isn't handled here.
            if 'a' <= char <= 'z' and char.isupper():
                 bindings.extend(["&macro_press &kp LSHIFT", "&macro_tap &kp " + keycode, "&macro_release &kp LSHIFT"])
            elif char == ' ': # Explicit space handling if needed, though we add it after
                 bindings.append("&macro_tap &kp SPACE")
            else:
                bindings.append("&macro_tap &kp " + keycode)
        else:
            print(f"Warning: No ZMK keycode found for character '{char}' in text '{text}'. Skipping in macro.", file=sys.stderr)

    if add_space:
        bindings.append("&macro_tap &kp SPACE")

    return "\n                ".join(bindings)

# --- Main Execution ---
print("Starting ZMK config generation...")

# 1. Load Chord Map
print(f"Loading chord map: {CHORD_MAP_FILE}")
if not CHORD_MAP_FILE.exists():
    print(f"Error: Chord map file not found at {CHORD_MAP_FILE}", file=sys.stderr)
    sys.exit(1)
try:
    with open(CHORD_MAP_FILE, "r", encoding="utf-8") as f:
        chord_data = json.load(f)
except json.JSONDecodeError as e:
    print(f"Error: Could not parse JSON file {CHORD_MAP_FILE}: {e}", file=sys.stderr)
    sys.exit(1)

word_to_chord = chord_data.get("words", {})
ngrams = chord_data.get("ngrams", [])
print(f"Loaded {len(word_to_chord)} word chords and {len(ngrams)} ngram chords.")

# 2. Parse Keymap for Positions
key_to_pos = parse_keymap_for_positions(KEYMAP_FILE)
if not key_to_pos:
     print("Error: Could not map keys to positions. Aborting.", file=sys.stderr)
     sys.exit(1)

# 3. Generate Macros
print("Generating macros...")
macros_dtsi_content = """
/*
 * Generated ZMK Macros
 * Automatically included by glove80.keymap
 * DO NOT EDIT MANUALLY - Run generate_zmk_config.py instead
 */

/ {
    macros {
"""
combo_to_macro_map = {} # Store {combo_name: macro_name} - This will become unused but kept for now to minimize changes if other parts rely on it implicitly.
combo_generation_data = [] # Store data needed for combo generation
macros_generated_count = 0 # Keep track of macros generated

# Word Macros (with space)
for word, chord_str in word_to_chord.items():
    if not word or not chord_str:
        print(f"Warning: Skipping invalid word/chord entry: word='{word}', chord='{chord_str}'", file=sys.stderr)
        continue

    # Skip words containing Swedish characters in the word or the chord definition
    if 'å' in word or 'ä' in word or 'ö' in word or 'å' in chord_str or 'ä' in chord_str or 'ö' in chord_str:
        print(f"Info: Skipping word '{word}' as it or its chord '{chord_str}' contains Swedish characters.", file=sys.stderr)
        continue

    macro_name = generate_zmk_name(word, "m")
    combo_name = generate_zmk_name(word, "c")
    macro_binding_str = create_macro_bindings(word, add_space=True)
    if macro_binding_str:
        macros_dtsi_content += f"""
        {macro_name}: {macro_name} {{
            compatible = "zmk,behavior-macro";
            label = "MAC_{macro_name.upper()}"; // Use uppercase label for convention
            #binding-cells = <0>;
            bindings = <{macro_binding_str}>;
        }};
"""
        # Store data needed for combo generation later
        combo_generation_data.append({
            "type": "word",
            "original": word,
            "chord": chord_str,
            "combo_name": combo_name, # The unique name generated here
            "macro_name": macro_name  # The unique name generated here
        })
        macros_generated_count += 1
    else:
         print(f"Warning: Could not generate macro bindings for word '{word}'. Skipping combo.", file=sys.stderr)


# Ngram Macros (no space)
for ngram in ngrams:
    if not ngram:
        print("Warning: Skipping empty ngram string.", file=sys.stderr)
        continue

    # Skip ngrams containing Swedish characters (ngrams are their own chords)
    if 'å' in ngram or 'ä' in ngram or 'ö' in ngram:
        print(f"Info: Skipping ngram '{ngram}' as it contains Swedish characters.", file=sys.stderr)
        continue

    macro_name = generate_zmk_name(ngram, "m")
    combo_name = generate_zmk_name(ngram, "c")
    macro_binding_str = create_macro_bindings(ngram, add_space=False)
    if macro_binding_str:
        macros_dtsi_content += f"""
        {macro_name}: {macro_name} {{
            compatible = "zmk,behavior-macro";
            label = "MAC_{macro_name.upper()}";
            #binding-cells = <0>;
            bindings = <{macro_binding_str}>;
        }};
"""
        # Store data needed for combo generation later
        combo_generation_data.append({
            "type": "ngram",
            "original": ngram,
            "chord": ngram, # For ngrams, the chord IS the ngram
            "combo_name": combo_name,
            "macro_name": macro_name
        })
        macros_generated_count += 1
    else:
        print(f"Warning: Could not generate macro bindings for ngram '{ngram}'. Skipping combo.", file=sys.stderr)

macros_dtsi_content += """
    }; // end of macros
}; // end of /
"""
print(f"Generated {macros_generated_count} macros.")

# 4. Generate Combos
print("Generating combos...")
combos_dtsi_content = """
/*
 * Generated ZMK Combos
 * Automatically included by glove80.keymap
 * DO NOT EDIT MANUALLY - Run generate_zmk_config.py instead
 */

/ {
    combos {
        compatible = "zmk,combos";
"""
combos_generated_count = 0

# --- Generate Combos from stored data ---
for item in combo_generation_data:
    combo_name = item["combo_name"]
    macro_name = item["macro_name"]
    chord_str = item["chord"]
    original_text = item["original"] # For logging/debug purposes

    positions = []
    valid_combo = True
    unique_chars_in_chord = set(chord_str)

    for char in unique_chars_in_chord:
        pos = key_to_pos.get(char.lower())
        if pos is not None:
            positions.append(str(pos))
        else:
            print(f"Warning: Key '{char}' in chord string '{chord_str}' for '{original_text}' not found in keymap base layer. Skipping combo.", file=sys.stderr)
            valid_combo = False
            break

    if valid_combo and len(positions) >= 2: # Need at least 2 keys for a combo
        position_str = " ".join(sorted(positions, key=int)) # Sort positions numerically
        # Add a comment indicating the original word/ngram
        comment = f"// Combo for {item['type']}: {original_text} (Chord: {chord_str})"
        combos_dtsi_content += f"""
        {comment}
        {combo_name}: {combo_name} {{
            key-positions = <{position_str}>;
            timeout-ms = <{DEFAULT_COMBO_TIMEOUT}>; /* Explicit timeout */
            bindings = <&{macro_name}>;
        }};
"""
        combos_generated_count += 1
    elif len(positions) < 2:
        # Only print info for multi-character words/ngrams or if zero keys were found
        if len(original_text) > 1 or len(positions) == 0:
            print(f"Info: Skipping combo for '{original_text}' as chord '{chord_str}' results in less than 2 mapped key positions ({positions}).", file=sys.stderr)

# Add default timeout definition at the top of the file if not already there
# (Assuming DEFAULT_COMBO_TIMEOUT = 50 is defined near the top)

combos_dtsi_content += """
    }; // end of combos
}; // end of /
"""
print(f"Generated {combos_generated_count} combos.")

# 5. Write Output Files
try:
    print(f"Writing macros to: {OUTPUT_MACROS_FILE}")
    OUTPUT_MACROS_FILE.write_text(macros_dtsi_content, encoding="utf-8")
    print(f"Writing combos to: {OUTPUT_COMBOS_FILE}")
    OUTPUT_COMBOS_FILE.write_text(combos_dtsi_content, encoding="utf-8")
except IOError as e:
     print(f"Error writing output file: {e}", file=sys.stderr)
     sys.exit(1)

print("\nGeneration complete.")
print(f"Generated files written directly to the '{KEYMAP_FILE.parent.name}/' directory.")
print(f"Ensure '{KEYMAP_FILE.name}' includes the generated files like this (usually placed within the top-level / {{ ... }}):")
print("""
/ {
    // ... other includes like behaviors ...

    #include "generated_macros.dtsi"
    #include "generated_combos.dtsi"

    keymap {
        // ... your keymap layers ...
    };

    // ... other nodes like leds, sensors ...
};
""")
