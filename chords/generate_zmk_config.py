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
DEFAULT_COMBO_TIMEOUT = 50 # ms, timeout for combos

# --- ZMK Keycode Mapping ---
# Maps lowercase characters/symbols to ZMK keycodes used in &kp bindings
# Ensure this covers all characters used in your words/ngrams AND present on your base layer
ZMK_KEYCODE_MAP = {
    'a': 'A', 'b': 'B', 'c': 'C', 'd': 'D', 'e': 'E', 'f': 'F', 'g': 'G',
    'h': 'H', 'i': 'I', 'j': 'J', 'k': 'K', 'l': 'L', 'm': 'M', 'n': 'N',
    'o': 'O', 'p': 'P', 'q': 'Q', 'r': 'R', 's': 'S', 't': 'T', 'u': 'U',
    'v': 'V', 'w': 'W', 'x': 'X', 'y': 'Y', 'z': 'Z',
    # Map Swedish characters to keys that exist in the base layer for combo position mapping.
    # The macro will still type the correct character.
    'å': 'LBKT',  # Using Left Bracket based on keymap inspection (was RBKT, but LBKT is unused)
    'ä': 'SQT',   # Map to single quote key
    'ö': 'SEMI',  # Map to semicolon key
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

    content = keymap_path.read_text()
    key_positions = {}

    # Find the layer_Base block (adjust if your base layer name differs)
    layer_match = re.search(r"layer_Base\s*{[^}]*bindings\s*=\s*<([^>]*)>", content, re.DOTALL)
    if not layer_match:
        # Try finding the default_layer if layer_Base isn't found
        layer_match = re.search(r"default_layer\s*{[^}]*bindings\s*=\s*<([^>]*)>", content, re.DOTALL)
        if not layer_match:
            print(f"Error: Could not find 'layer_Base' or 'default_layer' bindings in {keymap_path}", file=sys.stderr)
            sys.exit(1)
        print("Info: Found 'default_layer' instead of 'layer_Base'.")


    bindings_str = layer_match.group(1)
    # Extract individual bindings like '&kp A', '&kp LSHIFT', '&mo 1' etc.
    bindings = re.findall(r"(&[a-zA-Z0-9_]+\s*(?:[A-Z0-9_]+)?)", bindings_str)

    print(f"Found {len(bindings)} bindings on base layer.")

    # Create a reverse mapping from ZMK keycode to character
    zmk_to_char = {v: k for k, v in ZMK_KEYCODE_MAP.items()}

    for i, binding in enumerate(bindings):
        # Match basic key presses like '&kp A', '&kp N1', '&kp COMMA'
        kp_match = re.match(r"&kp\s+([A-Z0-9_]+)", binding)
        if kp_match:
            keycode = kp_match.group(1)
            # Find the character this keycode maps to (lowercase) using the reverse map
            found_char = zmk_to_char.get(keycode)
            if found_char:
                if found_char in key_positions:
                    # If a key appears multiple times (e.g., Shift), only map the first one found.
                    # This assumes chords use the primary position of a key.
                    pass
                else:
                    key_positions[found_char] = i
                    # print(f"Mapped '{found_char}' ({keycode}) to position {i}")

    print(f"Successfully mapped {len(key_positions)} unique keys to positions.")
    if not key_positions:
        print("Warning: No key positions were mapped. Check ZMK_KEYCODE_MAP and keymap format.", file=sys.stderr)
    return key_positions

def generate_zmk_name(base_name: str, prefix: str) -> str:
    """Creates a ZMK-compatible node name (macro or combo)."""
    # Handle specific non-ASCII chars explicitly for better names
    name = base_name.replace('å', 'aring').replace('ä', 'adia').replace('ö', 'odia')
    # Replace remaining invalid chars with underscore
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    # Remove leading/trailing underscores
    name = name.strip('_')
    if not name: # Handle empty string case after cleaning
        name = f"{prefix}_empty"
    # Ensure starts with letter
    if not name[0].isalpha():
        name = f'{prefix}_' + name
    # ZMK identifiers have length limits, truncate if necessary (e.g., 31 chars typical max)
    return name[:31]

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
combo_to_macro_map = {} # Store {combo_name: macro_name}
macros_generated_count = 0

# Word Macros (with space)
for word, chord_str in word_to_chord.items():
    if not word or not chord_str:
        print(f"Warning: Skipping invalid word/chord entry: word='{word}', chord='{chord_str}'", file=sys.stderr)
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
        combo_to_macro_map[combo_name] = macro_name
        macros_generated_count += 1
    else:
         print(f"Warning: Could not generate macro bindings for word '{word}'. Skipping combo.", file=sys.stderr)


# Ngram Macros (no space)
for ngram in ngrams:
    if not ngram:
        print("Warning: Skipping empty ngram string.", file=sys.stderr)
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
        combo_to_macro_map[combo_name] = macro_name
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
combos_dtsi_content = f"""
/*
 * Generated ZMK Combos
 * Automatically included by glove80.keymap
 * DO NOT EDIT MANUALLY - Run generate_zmk_config.py instead
 */

/ {{
    combos {{
        compatible = "zmk,combos";
        // Global combo settings can be adjusted here if needed
        // timeout-ms = <{DEFAULT_COMBO_TIMEOUT}>; // Default timeout for all combos unless overridden
        // require-prior-idle-ms = <50>; // Optional: time keys must be idle before combo activates
"""
combos_generated_count = 0

# Word Combos
for word, chord_str in word_to_chord.items():
    combo_name = generate_zmk_name(word, "c")
    if combo_name not in combo_to_macro_map: continue # Skip if macro failed

    positions = []
    valid_combo = True
    unique_chars_in_chord = set(chord_str) # Use set to handle duplicate chars in chord string if any

    for char in unique_chars_in_chord:
        pos = key_to_pos.get(char.lower())
        if pos is not None:
            positions.append(str(pos))
        else:
            print(f"Warning: Key '{char}' in chord string '{chord_str}' for word '{word}' not found in keymap base layer. Skipping combo.", file=sys.stderr)
            valid_combo = False
            break

    if valid_combo and len(positions) >= 2: # Need at least 2 keys for a combo
        macro_name = combo_to_macro_map[combo_name]
        position_str = " ".join(sorted(positions, key=int)) # Sort positions numerically
        combos_dtsi_content += f"""
        {combo_name}: {combo_name} {{
            timeout-ms = <{DEFAULT_COMBO_TIMEOUT}>;
            key-positions = <{position_str}>;
            bindings = <&{macro_name}>;
            // Only trigger on base layer (layer 0)
            layers = <0>;
        }};
"""
        combos_generated_count += 1
    elif len(positions) < 2:
         # Don't warn for single-letter words if they exist in the map and have < 2 keys
         if len(word) > 1 or len(positions) == 0: # Warn if multi-letter word has < 2 keys OR 0 keys found
             print(f"Info: Skipping combo for '{word}' as chord '{chord_str}' results in less than 2 mapped key positions ({positions}).", file=sys.stderr)


# Ngram Combos
for ngram in ngrams:
    combo_name = generate_zmk_name(ngram, "c")
    if combo_name not in combo_to_macro_map: continue # Skip if macro failed

    positions = []
    valid_combo = True
    # For ngrams, the chord keys are the letters of the ngram itself
    unique_chars_in_ngram = set(ngram)

    for char in unique_chars_in_ngram:
        pos = key_to_pos.get(char.lower())
        if pos is not None:
            positions.append(str(pos))
        else:
            print(f"Warning: Key '{char}' in ngram '{ngram}' not found in keymap base layer. Skipping combo.", file=sys.stderr)
            valid_combo = False
            break

    if valid_combo and len(positions) >= 2:
        macro_name = combo_to_macro_map[combo_name]
        position_str = " ".join(sorted(positions, key=int))
        combos_dtsi_content += f"""
        {combo_name}: {combo_name} {{
            timeout-ms = <{DEFAULT_COMBO_TIMEOUT}>;
            key-positions = <{position_str}>;
            bindings = <&{macro_name}>;
            layers = <0>;
        }};
"""
        combos_generated_count += 1
    elif len(positions) < 2:
         print(f"Info: Skipping combo for ngram '{ngram}' as it results in less than 2 mapped key positions ({positions}).", file=sys.stderr)


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
