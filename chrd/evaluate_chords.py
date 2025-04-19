import sys
import json
import re
from collections import Counter

if len(sys.argv) < 3 or len(sys.argv) > 5:
    print("Usage: python evaluate_chords.py <chordable_map.json> <corpus.txt> [ignore_list.txt] [min_frequency]")
    print("  min_frequency: minimum number of times a word must appear to be counted (default: 2)")
    sys.exit(1)

# Penalty map
penalties = {
    "chordable_word": 1,
    "chordable_ngram": 2,
    "not_chordable": 3
}

print("Scoring rules:")
for k, v in penalties.items():
    print(f"- {k.replace('_', ' ').capitalize()}: {v} point(s)")

# Load parameters
chord_file = sys.argv[1]
corpus_file = sys.argv[2]
ignore_file = None
min_frequency = 2

# Parse optional arguments
if len(sys.argv) >= 4:
    # Check if the 4th argument is a number (min_frequency) or a file (ignore_list)
    if sys.argv[3].isdigit():
        min_frequency = int(sys.argv[3])
    else:
        ignore_file = sys.argv[3]

if len(sys.argv) == 5:
    min_frequency = int(sys.argv[4])

with open(chord_file, "r") as f:
    chord_data = json.load(f)
    # Load the dictionary mapping words to their assigned chords
    word_to_chord_map = chord_data["words"]
    chordable_ngrams = set(chord_data["ngrams"])

# Read corpus and split into words
with open(corpus_file, "r", encoding="utf-8") as f:
    corpus_text = f.read().lower()
    words = re.findall(r"\b[a-z\u00e4\u00f6\u00e5]+\b", corpus_text)

# Count word frequencies in the corpus
word_frequencies = Counter(words)

print(f"Minimum word frequency: {min_frequency}")

# Filter out words that appear less than min_frequency times
words_to_process = [word for word in words if word_frequencies[word] >= min_frequency]

print(f"Total unique words in corpus: {len(set(words))}")
print(f"Words appearing at least {min_frequency} times: {len(set(words_to_process))}")
print(f"Words appearing less than {min_frequency} times: {len(set(words)) - len(set(words_to_process))}")

# Load ignored words if an ignore file is provided
ignored_words = set()
if ignore_file:
    try:
        with open(ignore_file, "r", encoding="utf-8") as f:
            # Read words, strip whitespace, convert to lower, ignore empty lines
            ignored_words = {line.strip().lower() for line in f if line.strip()}
        print(f"Loaded {len(ignored_words)} words to ignore from {ignore_file}")
    except FileNotFoundError:
        print(f"Warning: Ignore file '{ignore_file}' not found. No words will be ignored.")


def can_be_built_from_ngrams(word, ngrams):
    n = len(word)
    for i in range(1, n):
        first = word[:i]
        second = word[i:]
        if first in ngrams and second in ngrams:
            return True
    return False

score = 0
count = 0
category_counts = Counter()
not_chordable_words = [] # Store words that aren't chordable by word or ngram

for word in words_to_process:
    # Skip words that are in the ignore list
    if word in ignored_words:
        continue
        
    count += 1
    # Check if the word exists as a key in the word-to-chord map
    if word in word_to_chord_map:
        category = "chordable_word"
    elif can_be_built_from_ngrams(word, chordable_ngrams):
        category = "chordable_ngram"
    else:
        category = "not_chordable"
        not_chordable_words.append(word) # Track non-chordable words

    category_counts[category] += 1
    score += penalties[category]

score_per_word = score / count if count else 0

print("\nEvaluation result:")
print(f"Total words processed: {count}")
print(f"Words ignored: {len([w for w in words_to_process if w in ignored_words])}")
print(f"Total score: {score}")
print(f"Average score per word: {score_per_word:.2f}")

print("\nBreakdown by category:")
for cat in ["chordable_word", "chordable_ngram", "not_chordable"]:
    print(f"{cat.replace('_', ' ').capitalize()}: {category_counts[cat]}")

# Calculate flow coverage
chordable_count = category_counts["chordable_word"] + category_counts["chordable_ngram"]
flow_coverage_percentage = (chordable_count / count * 100) if count else 0
print(f"\nFlow Coverage (Word or Ngram): {flow_coverage_percentage:.2f}%")

# Analyze and display the most frequent non-chordable words
if not_chordable_words:
    print(f"\nAll Non-Chordable Words with Frequency >= {min_frequency}:")
    print(f"{'Word':<20} {'Frequency':<10}")
    print("-" * 30)
    non_chordable_counts = Counter(not_chordable_words)
    # Sort by frequency (highest first), then alphabetically
    for word, freq in sorted(non_chordable_counts.items(), key=lambda x: (-x[1], x[0])):
        print(f"{word:<20} {freq:<10}")
