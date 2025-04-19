import sqlite3
import json
from collections import Counter

# === SETUP ===
conn = sqlite3.connect("key_log.sqlite")
cur = conn.cursor()
cur.execute("SELECT key_code FROM key_log ORDER BY time_utc")
keystrokes = [row[0] for row in cur.fetchall()]

# === FINGER MAP (Glove80 w/ Colemak DH, Swedish layout) ===
finger_map = {
    'leftPinky':    {'q', 'a', 'z'},
    'leftRing':     {'w', 'r', 'x'},
    'leftMiddle':   {'f', 's', 'c'},
    'leftIndex':    {'p', 't', 'd', 'b', 'g', 'v'},
    'rightIndex':   {'j', 'm', 'k', 'l', 'n', 'h'},
    'rightMiddle':  {'u', 'e'},
    'rightRing':    {'y', 'i'},
    'rightPinky':   {'\u00f6', 'o', '\u00e5', '\u00e4'},
}

# === HELPERS ===
key_to_finger = {k: f for f, keys in finger_map.items() for k in keys}

def is_character(key):
    return len(key) == 1 and key.isprintable()

def is_chordable(word):
    used_fingers = set()
    for letter in word:
        finger = key_to_finger.get(letter)
        if not finger or finger in used_fingers:
            return False
        used_fingers.add(finger)
    return True

def get_chord(word):
    return ''.join(sorted(set(word)))

def is_chordable_by_compound(word, known_chordable):
    for i in range(2, len(word) - 1):
        left = word[:i]
        right = word[i:]
        if left in known_chordable and right in known_chordable:
            return (left, right)
    return None

def calculate_pairwise_ngram_score(ngram, word_counter, candidate_chordable_ngrams):
    """Calculates score based on forming words with other candidate ngrams."""
    score = 0
    ngram_len = len(ngram)
    for word, freq in word_counter.items():
        word_len = len(word)
        if word_len > ngram_len:
            # Check if ngram is prefix and suffix is another candidate ngram
            if word.startswith(ngram):
                suffix = word[ngram_len:]
                if suffix in candidate_chordable_ngrams:
                    score += freq # Weight by word frequency

            # Check if ngram is suffix and prefix is another candidate ngram
            if word.endswith(ngram):
                prefix = word[:-ngram_len]
                if prefix in candidate_chordable_ngrams:
                    score += freq # Weight by word frequency
    return score

def finger_hands(ngram):
    return {key_to_finger.get(ch, '')[:5] for ch in ngram if ch in key_to_finger}

def cross_hand_bonus(ngram):
    hands = finger_hands(ngram)
    return 1.2 if len(hands) > 1 else 1.0

# === PARSE WORDS ===
words = []
current_word = ""
WORD_BOUNDARIES = {
    "<space>", "<enter>", "<tab>", ".", ",", "!", "?", ";", ":",
    "(", ")", "[", "]", "{", "}", "\"", "'"
}

for key in keystrokes:
    if key in WORD_BOUNDARIES:
        if current_word:
            words.append(current_word)
            current_word = ""
    elif key == "<backspace>":
        current_word = current_word[:-1]
    elif is_character(key):
        current_word += key.lower()
if current_word:
    words.append(current_word)

# === ANALYZE ===
counter = Counter(words)
chord_to_word = {}
known_chordable = set(w for w in counter if is_chordable(w))

# === Analyze N-grams ===
ngram_counter = Counter()
for word in words:
    for i in range(len(word) - 1):
        bigram = word[i:i+2]
        if is_chordable(bigram):
            ngram_counter[bigram] += 1
    for i in range(len(word) - 2):
        trigram = word[i:i+3]
        if is_chordable(trigram):
            ngram_counter[trigram] += 1

# === Score N-grams ===
candidate_chordable_ngrams = set(ngram_counter.keys()) # Set of all found chordable ngrams
ngram_quality = []
for ngram, freq in ngram_counter.items():
    pairwise_score = calculate_pairwise_ngram_score(ngram, counter, candidate_chordable_ngrams)
    total_score = pairwise_score * cross_hand_bonus(ngram) # Combine pairwise score with cross-hand bonus
    ngram_quality.append((ngram, freq, pairwise_score, total_score))

# === Output Top Results ===
TOP_N_WORDS = 200
TOP_N_NGRAMS = 100

print("\nAssigning Chords to Top Words")
print(f"{'Word':<15} {'Assigned Chord':<15} {'Count':<6}")
print("-" * 40)

assigned_chords = {} # Stores {frozenset(chord_letters): word}
word_to_chord_map = {} # Stores {word: "chord_string"}
words_processed = 0

for word, count in counter.most_common():
    if words_processed >= TOP_N_WORDS:
        break

    if not is_chordable(word):
        continue

    natural_chord_letters = frozenset(word)
    natural_chord_str = "".join(sorted(natural_chord_letters))

    # Try assigning the natural chord
    if natural_chord_letters not in assigned_chords:
        assigned_chords[natural_chord_letters] = word
        word_to_chord_map[word] = natural_chord_str
        print(f"{word:<15} {natural_chord_str:<15} {count:<6}")
        words_processed += 1
    else:
        # Collision: Try subset chords (removing one letter)
        found_subset = False
        for letter_to_remove in natural_chord_letters:
            subset_chord_letters = natural_chord_letters - {letter_to_remove}
            if len(subset_chord_letters) < 2: # Ensure chord has at least 2 keys
                continue

            subset_chord_str = "".join(sorted(subset_chord_letters))
            # Check if subset is physically chordable and not already assigned
            if is_chordable(subset_chord_str) and subset_chord_letters not in assigned_chords:
                assigned_chords[subset_chord_letters] = word
                word_to_chord_map[word] = subset_chord_str
                print(f"{word:<15} {subset_chord_str:<15} {count:<6} (subset)")
                words_processed += 1
                found_subset = True
                break # Found a suitable subset chord

        # if not found_subset:
        #     print(f"{word:<15} {'-'*15} {count:<6} (collision, no subset)")


print("\nTop Chordable Ngrams")
print(f"{'Ngram':<10} {'Freq':<6} {'Pair Score':<10} {'Total Score':<12}") # Changed header
print("-" * 45) # Adjusted separator length
chordable_ngrams = []
# Use correct indices: ngram=x[0], freq=x[1], pairwise_score=x[2], total_score=x[3]
for ngram, freq, pairwise_score, total_score in sorted(ngram_quality, key=lambda x: -x[3])[:TOP_N_NGRAMS]:
    print(f"{ngram:<10} {freq:<6} {pairwise_score:<10} {int(total_score):<12}") # Use pairwise_score
    chordable_ngrams.append(ngram)

# === Export JSON mapping ===
with open("chordable_map.json", "w") as f:
    # Save the word-to-chord mapping and the ngrams list
    json.dump({"words": word_to_chord_map, "ngrams": chordable_ngrams}, f, indent=2)
print("\nExported chordable_map.json")

