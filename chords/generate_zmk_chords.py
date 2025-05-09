#!/usr/bin/env python3
import json
import os
import re
import string
from pathlib import Path
from collections import defaultdict

# === Configuration ===
INPUT_CHORDS_FILE = "chords/jocke_chords.json" # Use your chord file
OUTPUT_CHORDS_KEYMAP_FILE = Path("config/generated_chords.keymap") # New output file
KEYMAP_FILE = Path("config/glove80.keymap")

# === ZMK Key Mapping (Add DUP) ===
ZMK_KEYCODE_MAP = {
    'a': 'A', 'b': 'B', 'c': 'C', 'd': 'D', 'e': 'E', 'f': 'F', 'g': 'G',
    'h': 'H', 'i': 'I', 'j': 'J', 'k': 'K', 'l': 'L', 'm': 'M', 'n': 'N',
    'o': 'O', 'p': 'P', 'q': 'Q', 'r': 'R', 's': 'S', 't': 'T', 'u': 'U',
    'v': 'V', 'w': 'W', 'x': 'X', 'y': 'Y', 'z': 'Z',
    '1': 'N1', '2': 'N2', '3': 'N3', '4': 'N4', '5': 'N5',
    '6': 'N6', '7': 'N7', '8': 'N8', '9': 'N9', '0': 'N0',
    ' ': 'SPACE', '\b': 'BSPC', '\n': 'RET', '\t': 'TAB', # Use BSPC for backspace
    '!': 'EXCL', '@': 'AT', '#': 'HASH', '$': 'DLLR', '%': 'PRCNT', # Use ZMK names
    '^': 'CARET', '&': 'AMPS', '*': 'ASTRK', '(': 'LPAR', ')': 'RPAR', # Use ZMK names
    '-': 'MINUS', '_': 'UNDER', '=': 'EQUAL', '+': 'PLUS', '[': 'LBKT',
    ']': 'RBKT', '{': 'LBRC', '}': 'RBRC', '\\': 'BSLH', '|': 'PIPE',
    ';': 'SEMI', ':': 'COLON', "'": 'SQT', '"': 'DQT', ',': 'COMMA',
    '.': 'DOT', '/': 'FSLH', '<': 'LT', '>': 'GT', '?': 'QMARK', # Use ZMK names
    '`': 'GRAVE', '~': 'TILDE',
    'Dup': 'DUP', # Add Dup mapping - Ensure 'DUP' exists in your keymap defines or use a placeholder like K_F13
    # Add mappings for any other special keys used in your combos (e.g., Bksp, Ret)
    'Bksp': 'BSPC',
    'Ret': 'RET',
    'Spc': 'SPACE',
    'Tab': 'TAB',
    # Add any other mappings needed from your jocke_chords.json 'combo' keys
}

# Keys that represent modifiers or layers, not standard key presses
NON_OUTPUT_KEYS = {'LSHFT', 'RSHFT', 'LCTRL', 'RCTRL', 'LALT', 'RALT', 'LGUI', 'RGUI', 'LSFT', 'RSFT'}

def create_macro_bindings(text):
    """Create ZMK macro bindings for the given text, handling shifts."""
    if not text:
        return "", "" # Return empty strings for base and shifted

    base_bindings = []
    shifted_bindings = []
    needs_shifted_macro = False
    first_char_processed = False

    # Handle leading backspace
    if text.startswith('\b'):
        base_bindings.append("&kp BSPC")
        shifted_bindings.append("&kp BSPC")
        text = text[1:]

    for char in text:
        is_first_char = not first_char_processed
        first_char_processed = True

        if char.islower() and char in ZMK_KEYCODE_MAP:
            zmk_code = ZMK_KEYCODE_MAP[char]
            base_bindings.append(f"&kp {zmk_code}")
            shifted_bindings.append(f"&kp LS({zmk_code})")
            if is_first_char:
                needs_shifted_macro = True # Need shift version if first char is lowercase
        elif char.isupper() and char.lower() in ZMK_KEYCODE_MAP:
            zmk_code = ZMK_KEYCODE_MAP[char.lower()]
            base_bindings.append(f"&kp LS({zmk_code})")
            shifted_bindings.append(f"&kp LS({zmk_code})") # Shifted uppercase is still shifted uppercase
        elif char in ZMK_KEYCODE_MAP:
            zmk_code = ZMK_KEYCODE_MAP[char]
            base_bindings.append(f"&kp {zmk_code}")
            # How to handle shifted symbols? Depends on layout. Assume base for now.
            # You might need a more complex mapping for shifted symbols.
            shifted_bindings.append(f"&kp {zmk_code}")
        else:
            print(f"Warning: Unknown character '{char}' in text '{text}' for macro generation.")

    # Only return shifted bindings if the first character required a shift
    return " ".join(base_bindings), " ".join(shifted_bindings) if needs_shifted_macro else None

_used_zmk_names = {}
def generate_zmk_name(base_name, prefix="m"):
    """Generate a unique and valid ZMK identifier."""
    # Sanitize base_name
    name = ''.join(c for c in base_name.lower() if c.isalnum() or c == '_')
    name = re.sub(r'_+', '_', name).strip('_') # Replace multiple underscores and strip ends

    if not name: # Handle cases where sanitization results in empty string
        name = f"{prefix}_unnamed"
    elif not name[0].isalpha():
        name = f"{prefix}_{name}"
    else:
        name = f"{prefix}_{name}"

    # Ensure uniqueness
    original_name = name
    count = 1
    while name in _used_zmk_names:
        name = f"{original_name}_{count}"
        count += 1
    _used_zmk_names[name] = True

    # ZMK identifiers have length limits (e.g., 31 for labels)
    return name[:31]

# === Main Processing ===
def main():
    # Load the input JSON
    try:
        with open(INPUT_CHORDS_FILE, 'r') as f:
            jocke_data = json.load(f)
    except FileNotFoundError:
        print(f"Error: Input file '{INPUT_CHORDS_FILE}' not found.")
        exit(1)
    except json.JSONDecodeError:
        print(f"Error: Could not decode JSON from '{INPUT_CHORDS_FILE}'.")
        exit(1)

    # --- Hardcoded Key Positions (Based on User Input & Glove80 Defaults) ---
    # Mapping from ZMK Keycode Name (e.g., 'A', 'B', 'N1') to Position Number
    # UPDATED based on provided colemak_dh_lookup
    key_name_to_pos_num = {
        # Left Hand
        'Q': 23, 'W': 24, 'F': 25, 'P': 26, 'B': 27,
        'A': 35, 'R': 36, 'S': 37, 'T': 38, 'G': 39,
        'Z': 47, 'X': 48, 'C': 49, 'D': 50, 'V': 51,
        # Right Hand
        'J': 28, 'L': 29, 'U': 30, 'Y': 31, 'SEMI': 32, # ';' maps to SEMI
        'M': 40, 'N': 41, 'E': 42, 'I': 43, 'O': 44,
        'K': 45, 'H': 59, 'COMMA': 60, 'DOT': 61, 'FSLH': 62, # ',' maps to COMMA, '.' to DOT, '/' to FSLH
        # Add any missing keys from your ZMK_KEYCODE_MAP if they are used in combos
        # e.g., 'N1': ?, 'N2': ?, ..., 'SPACE': ?, 'RET': ?, 'TAB': ?, 'ESC': ?, 'BSPC': ?
    }

    # Manually define Shift and Dup positions (Update these numbers!)
    # CRITICAL: Find the position number for your main Shift key (e.g., Left Shift)
    # Check your #define POS_... lines or count in layer_Base
    # Verify this position based on your Colemak layout/preferences!
    shift_pos_num = 46 # POS_LH_C6R5 (Default Glove80 Left Shift) - VERIFY THIS!
    # CRITICAL: Find the position number where you bound '&key_repeat_behavior'
    # Verify this position based on your Colemak layout/preferences!
    dup_pos_num = 63   # POS_RH_C6R5 (Default Glove80 K_PP / Right Thumb) - VERIFY THIS!

    # Add DUP to the map using its manual position
    if dup_pos_num is not None:
        key_name_to_pos_num['DUP'] = dup_pos_num
        print(f"Using DUP position: {dup_pos_num}")
    else:
        print("Warning: dup_pos_num is not set. 'Dup' key combos will fail.")

    if shift_pos_num is None:
         print("Error: shift_pos_num is not set. Shifted combos cannot be generated.")
         # exit(1) # Optional: exit if shift is critical
    else:
         print(f"Using Shift position: {shift_pos_num}")
         # Add Shift itself to the map if needed for some reason (usually not)
         # key_name_to_pos_num['LSHFT'] = shift_pos_num

    print(f"Using hardcoded map with {len(key_name_to_pos_num)} keys.")
    # --- End of Hardcoded Positions ---

    # --- Prepare the output content ---
    # Create the header with helper macros
    output_content = f"""
/*
 * Generated ZMK Chords & Macros from {INPUT_CHORDS_FILE}
 * Automatically included by glove80.keymap
 * DO NOT EDIT MANUALLY
 */

#define str(s) #s

#define WORD_MACRO(name, keys) \\
    name: name {{ \\
        compatible = "zmk,behavior-macro"; \\
        #binding-cells = <0>; \\
        wait-ms = <1>; \\
        tap-ms = <1>; \\
        bindings = <keys>, <&kp SPACE>; \\
    }};

#define LAYER_CHORD(name, keypress, keypos, lays) \\
  combo_##name: combo_##name {{ \\
    timeout-ms = <60>; \\
    bindings = <keypress>; \\
    key-positions = <keypos>; \\
    layers = <lays>; \\
  }};

#define CHORD(name, keypress, keypos) \\
  LAYER_CHORD(name, keypress, keypos, LAYER_Base)

/ {{
    macros {{
"""
    macros_definitions = []
    combos_definitions = []

    # First pass: Generate macro definitions
    if "chords" in jocke_data and isinstance(jocke_data["chords"], list):
        # Reset used names for this pass if needed, or manage globally
        _used_zmk_names.clear() # Clear names before generating macros
        for item in jocke_data["chords"]:
            if not isinstance(item, dict): continue
            output_val = item.get("output")
            behavior = item.get("behavior")
            if output_val is None or behavior: continue # Only process output-based macros here

            # Determine Base Name
            base_name_src = output_val[0] if isinstance(output_val, list) else output_val
            base_zmk_name = generate_zmk_name(base_name_src, prefix="m")
            exact = item.get("exact", False)
            add_space = not exact

            # Create Macro Bindings
            output_text = output_val[0] if isinstance(output_val, list) else output_val
            base_bindings_str, shifted_bindings_str = create_macro_bindings(output_text)

            if base_bindings_str:
                macros_definitions.append(f"""
        // Output: '{output_text}'{' + SPACE' if add_space else ''}
        WORD_MACRO({base_zmk_name}, {base_bindings_str})""")

            if shifted_bindings_str:
                shifted_zmk_name = base_zmk_name.replace("m_", "m_S_", 1)
                macros_definitions.append(f"""
        // Shifted Output: '{output_text}'{' + SPACE' if add_space else ''}
        WORD_MACRO({shifted_zmk_name}, {shifted_bindings_str})""")

    # Append macro definitions to output
    output_content += "\n".join(macros_definitions)
    output_content += """
    }; // end of macros

    combos {
        compatible = "zmk,combos";
"""

    # Second pass: Generate combo definitions
    if "chords" in jocke_data and isinstance(jocke_data["chords"], list):
        # Reset used names again to ensure consistency if generate_zmk_name relies on it
        _used_zmk_names.clear() # Clear names before generating combos/referencing macros
        for item in jocke_data["chords"]:
            if not isinstance(item, dict): continue
            combo_keys = item.get("combo", [])
            output_val = item.get("output")
            behavior = item.get("behavior")
            if not combo_keys or (output_val is None and behavior is None): continue

            # Determine Base Name and Macro Binding
            base_name_src = ""
            macro_binding = None
            shifted_macro_binding = None

            if behavior:
                print(f"Warning: Behavior '{behavior}' found, direct binding not yet implemented. Skipping combo for {behavior}.")
                continue
            elif output_val is not None:
                base_name_src = output_val[0] if isinstance(output_val, list) else output_val
                # Regenerate name to match macro pass - IMPORTANT: ensure generate_zmk_name is deterministic
                # or that the state (_used_zmk_names) is reset correctly between passes.
                base_zmk_name = generate_zmk_name(base_name_src, prefix="m")
                macro_binding = f"&{base_zmk_name}"
                # Check if shifted version exists (based on first char of output)
                output_text = output_val[0] if isinstance(output_val, list) else output_val
                _, shifted_bindings_str = create_macro_bindings(output_text)
                if shifted_bindings_str:
                    # Need the shifted name again
                    shifted_zmk_name = base_zmk_name.replace("m_", "m_S_", 1)
                    shifted_macro_binding = f"&{shifted_zmk_name}"

            if not macro_binding: continue # Skip if no valid binding (e.g., behavior only)

            # Prepare Key Positions
            key_positions = []
            valid_combo = True
            for key in combo_keys:
                zmk_keycode = ZMK_KEYCODE_MAP.get(key)
                if not zmk_keycode:
                    print(f"Warning: No ZMK keycode mapping for '{key}' in combo for '{base_name_src}'. Skipping combo.")
                    valid_combo = False; break
                pos_num = key_name_to_pos_num.get(zmk_keycode)
                if pos_num is None:
                    print(f"Warning: Could not find position for ZMK keycode '{zmk_keycode}' (from '{key}') in combo for '{base_name_src}'. Skipping combo.")
                    valid_combo = False; break
                key_positions.append(str(pos_num))

            if valid_combo:
                # Derive combo name from base ZMK name used for the macro
                combo_zmk_name = base_zmk_name.replace("m_", "c_", 1)
                chord_arg_name = combo_zmk_name.replace("c_", "", 1) # Name for the CHORD macro argument
                combo_comment = f"// Combo for word: {base_name_src} (Chord: {''.join(sorted(k.lower() for k in combo_keys if k.lower() in ZMK_KEYCODE_MAP))})"
                combos_definitions.append(f"""
        {combo_comment}
        CHORD({chord_arg_name}, {macro_binding}, {" ".join(sorted(key_positions, key=int))})""") # Remove < >

                # Add shifted combo if applicable
                if shifted_macro_binding and shift_pos_num is not None:
                    # The CHORD macro itself needs a unique name for the shifted version
                    shifted_chord_arg_name = f"S_{chord_arg_name}"
                    shifted_key_positions = sorted(key_positions + [str(shift_pos_num)], key=int)
                    shifted_combo_comment = f"// Shifted combo for word: {base_name_src}"
                    combos_definitions.append(f"""
        {shifted_combo_comment}
        CHORD({shifted_chord_arg_name}, {shifted_macro_binding}, {" ".join(shifted_key_positions)})""") # Remove < >


    # Append combo definitions to output
    output_content += "\n".join(combos_definitions)
    output_content += """
    }; // end of combos
}; // end of /
"""

    # Save the output file
    try:
        os.makedirs(os.path.dirname(OUTPUT_CHORDS_KEYMAP_FILE), exist_ok=True)
        with open(OUTPUT_CHORDS_KEYMAP_FILE, 'w') as f:
            f.write(output_content)
        print(f"Successfully created '{OUTPUT_CHORDS_KEYMAP_FILE}'")
    except IOError as e:
        print(f"Error writing output files: {e}")
        exit(1)

if __name__ == "__main__":
    main()
