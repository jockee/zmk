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
# OUTPUT_MACROS_FILE = Path("config/generated_macros.dtsi") # Old
# OUTPUT_COMBOS_FILE = Path("config/generated_combos.dtsi") # Old
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
    key_name_to_pos_num = {
        # Letters (map lowercase from user input to uppercase ZMK code)
        'Q': 14, 'W': 13, 'F': 12, 'P': 11, 'B': 10, # Left Hand Top Row (R2 C2-C6)
        'J': 17, 'L': 18, 'U': 19, 'Y': 20, 'SEMI': 21, # Right Hand Top Row (R2 C2-C6) - Note SEMI for ;
        'A': 26, 'R': 25, 'S': 24, 'T': 23, 'G': 22, # Left Hand Home Row (R3 C2-C6)
        'M': 29, 'N': 30, 'E': 31, 'I': 32, 'O': 33, # Right Hand Home Row (R3 C2-C6)
        'Z': 38, 'X': 37, 'C': 36, 'D': 35, 'V': 34, # Left Hand Bottom Row (R4 C2-C6)
        'K': 41, 'H': 42, 'COMMA': 43, 'DOT': 44, 'FSLH': 45, # Right Hand Bottom Row (R4 C2-C6) - Note COMMA, DOT, FSLH

        # Numbers (Check your Num layer or Base layer if they are there)
        'N1': 4, 'N2': 3, 'N3': 2, 'N4': 1, 'N5': 0,  # Example: Top row left
        'N6': 5, 'N7': 6, 'N8': 7, 'N9': 8, 'N0': 9,  # Example: Top row right

        # Symbols (Add positions for symbols used in combos if not covered by letters/numbers)
        'SQT': 40, # Single Quote (') POS_RH_C1R4
        'EQUAL': 5, # Equals (=) POS_LH_C2R1
        'MINUS': 9, # Minus (-) POS_RH_C6R1
        'LBKT': 16, # Left Bracket ([) POS_RH_C1R2
        # Add other symbols like GRAVE, TILDE, HASH, DLLR etc. based on your layer_Sym bindings and their POS_ defines

        # Navigation/Special Keys (Update positions based on your layer_Base)
        'BSPC': 61,  # POS_RH_C4R5 (Common thumb) - VERIFY
        'RET': 55,   # POS_RH_T3 (Common thumb) - VERIFY
        'SPACE': 56, # POS_RH_T2 (Common thumb) - VERIFY
        'TAB': 15,   # POS_LH_C1R2 (Common pinky/ring) - VERIFY
        'ESC': 27,   # POS_LH_C1R3 (Common pinky/ring) - VERIFY
        # Add others like DEL, HOME, END, UP, DOWN, LEFT, RIGHT if used in combos
    }

    # Manually define Shift and Dup positions (Update these numbers!)
    # CRITICAL: Find the position number for your main Shift key (e.g., Left Shift)
    # Check your #define POS_... lines or count in layer_Base
    shift_pos_num = 46 # POS_LH_C6R5 (Default Glove80 Left Shift) - VERIFY THIS!
    # CRITICAL: Find the position number where you bound '&key_repeat_behavior'
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

    # --- Prepare content for the new single output file ---
    output_content = f"""
/*
 * Generated ZMK Chords & Macros from {INPUT_CHORDS_FILE}
 * Automatically included by glove80.keymap
 * DO NOT EDIT MANUALLY
 */

// Helper macro for stringification (from cradio)
#define str(s) #s

// Define a macro for word chords that types keys and adds a space
#define WORD_MACRO(name, keys) \\
    {name}: {name} {{ \\
        compatible = "zmk,behavior-macro"; \\
        #binding-cells = <0>; \\
        wait-ms = <1>; \\
        tap-ms = <1>; \\
        bindings = <keys>, <&kp SPACE>; \\
    }};

// Define a LAYER_CHORD macro for generating combos on specific layers (from cradio)
// Assumes layer defines like LAYER_Base exist in the main keymap
#define LAYER_CHORD(name, keypress, keypos, lays, timeout) \\
  combo_{name}: combo_{name} {{ \\
    timeout-ms = <timeout>; \\
    bindings = <keypress>; \\
    key-positions = <keypos>; \\
    layers = <lays>; \\
  }};

// Define a CHORD macro as shorthand for LAYER_CHORD on the Base layer
#define CHORD(name, keypress, keypos, timeout) \\
  LAYER_CHORD(name, keypress, keypos, LAYER_Base, timeout)

/ {{
    macros {{
""" # Start macros node

    macros_string_list = [] # Store macro definition strings
    combos_data = [] # Store tuples: (zmk_name, key_positions_str, macro_binding, comment)

    # Process chords
    if "chords" in jocke_data and isinstance(jocke_data["chords"], list):
        for item in jocke_data["chords"]:
            if not isinstance(item, dict):
                continue

            combo_keys = item.get("combo", [])
            output_val = item.get("output")
            behavior = item.get("behavior") # Check for behavior definition
            exact = item.get("exact", False)
            shift_override = item.get("shift") # Sartak's explicit shift output

            if not combo_keys or (output_val is None and behavior is None):
                continue

            # --- Determine Base Name for ZMK Identifier ---
            base_name_src = ""
            if behavior:
                 base_name_src = behavior.replace('-', '_') # Use behavior name
            elif isinstance(output_val, list) and output_val:
                base_name_src = output_val[0]
            elif isinstance(output_val, str):
                base_name_src = output_val
            else:
                 base_name_src = f"chord_{len(_used_zmk_names)}" # Fallback name

            base_zmk_name = generate_zmk_name(base_name_src, prefix="m") # Macro name
            combo_zmk_name = base_zmk_name.replace("m_", "c_", 1) # Combo name

            # --- Handle Output/Behavior ---
            macro_binding = None
            shifted_macro_binding = None
            macro_to_generate = None
            shifted_macro_to_generate = None

            if behavior:
                 # Directly use behavior if defined (no macro needed)
                 # Convert behavior name to ZMK binding if possible, e.g., &kp, &mo, etc.
                 # This part needs specific mapping based on desired behaviors
                 print(f"Warning: Behavior '{behavior}' found, direct binding not yet implemented in script. Skipping combo generation for {base_name_src}.")
                 # Example: if behavior == 'delete-word': macro_binding = '&kp LA(BSPC)'
                 continue # Skip combo generation for now if behavior is complex

            elif output_val is not None:
                # Use output to generate macro(s)
                output_text = output_val[0] if isinstance(output_val, list) else output_val
                output_text_clean = output_text # Keep leading backspace for create_macro_bindings
                add_space = not exact

                base_bindings_str, shifted_bindings_str = create_macro_bindings(output_text_clean)

                if base_bindings_str:
                    macro_binding = f"&{base_zmk_name}"
                    macro_to_generate = (base_zmk_name, base_bindings_str, add_space, output_text) # Pass original text for comment

                if shifted_bindings_str:
                    # Generate shifted macro name
                    shifted_zmk_name = base_zmk_name.replace("m_", "m_S_", 1)
                    shifted_macro_binding = f"&{shifted_zmk_name}"
                    shifted_macro_to_generate = (shifted_zmk_name, shifted_bindings_str, add_space, output_text) # Pass original text


            # --- Generate Macro Definitions ---
            if macro_to_generate:
                m_name, m_bindings, m_add_space, m_orig_text = macro_to_generate
                # final_bindings = m_bindings + (" &kp SPACE" if m_add_space else "") # WORD_MACRO adds space
                # Format as WORD_MACRO call for the output file
                macros_string_list.append(f"""
        // Output: '{m_orig_text}'{' + SPACE' if m_add_space else ''}
        WORD_MACRO({m_name}, {m_bindings})""") # Pass only key bindings, macro adds space
                # macros_content += f"""
                # {m_name}: {m_name} {{ // Output: '{m_orig_text}'{' + SPACE' if m_add_space else ''}
                #     compatible = "zmk,behavior-macro";
                #     #binding-cells = <0>;
                #     wait-ms = <1>; // Adjust timing if needed
                #     tap-ms = <1>;
                #     bindings = <{final_bindings}>;
                # }};""" # Old format commented out

            if shifted_macro_to_generate:
                sm_name, sm_bindings, sm_add_space, sm_orig_text = shifted_macro_to_generate
                # final_s_bindings = sm_bindings + (" &kp SPACE" if sm_add_space else "") # WORD_MACRO adds space
                # Format as WORD_MACRO call for the output file
                macros_string_list.append(f"""
        // Shifted Output: '{sm_orig_text}'{' + SPACE' if sm_add_space else ''}
        WORD_MACRO({sm_name}, {sm_bindings})""") # Pass only key bindings, macro adds space
                # macros_content += f"""
                # {sm_name}: {sm_name} {{ // Shifted Output: '{sm_orig_text}'{' + SPACE' if sm_add_space else ''}
                #     compatible = "zmk,behavior-macro";
                #     #binding-cells = <0>;
                #     wait-ms = <1>;
                #     tap-ms = <1>;
                #     bindings = <{final_s_bindings}>;
                # }};""" # Old format commented out

            # --- Prepare Combo Data ---
            key_positions = []
            valid_combo = True
            for key in combo_keys:
                # Map key name (from JSON) to ZMK keycode name used in keymap
                zmk_keycode = ZMK_KEYCODE_MAP.get(key)
                if not zmk_keycode:
                    print(f"Warning: No ZMK keycode mapping for '{key}' in combo for '{base_name_src}'. Skipping combo.")
                    valid_combo = False
                    break
                # Map ZMK keycode name to position number
                pos_num = key_name_to_pos_num.get(zmk_keycode)
                if pos_num is None:
                    print(f"Warning: Could not find position for ZMK keycode '{zmk_keycode}' (from '{key}') in combo for '{base_name_src}'. Skipping combo.")
                    valid_combo = False
                    break
                key_positions.append(str(pos_num))

            if valid_combo and macro_binding:
                # Generate comment for combo
                combo_comment = f"// Combo for word: {base_name_src} (Chord: {''.join(sorted(k.lower() for k in combo_keys if k.lower() in ZMK_KEYCODE_MAP))})"
                combos_data.append((combo_zmk_name, " ".join(sorted(key_positions, key=int)), macro_binding, combo_comment))

                # Add shifted combo if applicable
                if shifted_macro_binding and shift_pos_num is not None:
                    shifted_combo_name = combo_zmk_name.replace("c_", "c_S_", 1)
                    shifted_key_positions = sorted(key_positions + [str(shift_pos_num)], key=int)
                    shifted_combo_comment = f"// Shifted combo for word: {base_name_src}"
                    combos_data.append((shifted_combo_name, " ".join(shifted_key_positions), shifted_macro_binding, shifted_combo_comment))


    # --- Generate Combo Definitions ---
    combos_string_list = [] # Initialize list to store combo strings
    for c_name, c_pos_str, c_binding, c_comment in combos_data:
         # Basic timeout, could be adjusted based on len(c_pos_str.split())
         key_count = len(c_pos_str.split())
         timeout = 40 + (key_count - 1) * 10 if key_count > 1 else 40 # Base timeout 40ms
         timeout = min(timeout, 80) # Cap timeout at 80ms for now
         # Adjust name for the CHORD macro (remove c_ or c_S_ prefix)
         chord_arg_name = c_name.replace("c_S_", "S_", 1).replace("c_", "", 1)
         combos_string_list.append(f"""
        {c_comment}
        CHORD({chord_arg_name}, {c_binding}, {c_pos_str}, {timeout})""") # Append to list

    # --- Finalize Output Content ---
    # Add generated macros to the output string
    output_content += "\n".join(macros_string_list)
    output_content += """
    }; // end of macros

    combos {
        compatible = "zmk,combos";
""" # Close macros, open combos
    # Add generated combos to the output string
    output_content += "\n".join(combos_string_list)
    output_content += """
    }; // end of combos
}; // end of the single root node /
"""

    # Save the output file
    try:
        # Save the single output file
        os.makedirs(os.path.dirname(OUTPUT_CHORDS_KEYMAP_FILE), exist_ok=True)
        with open(OUTPUT_CHORDS_KEYMAP_FILE, 'w') as f:
            f.write(output_content) # Write the combined content
        print(f"Successfully created '{OUTPUT_CHORDS_KEYMAP_FILE}'")
    except IOError as e:
        print(f"Error writing output files: {e}")
        exit(1)

if __name__ == "__main__":
    main()
