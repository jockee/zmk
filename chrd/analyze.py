import sqlite3
from collections import Counter

# Connect to your SQLite file
conn = sqlite3.connect("key_log.sqlite")
cur = conn.cursor()

# Fetch all keys in order
cur.execute("SELECT key_code FROM key_log ORDER BY time_utc")
keystrokes = [row[0] for row in cur.fetchall()]

# Helper to filter keys
def is_character(key):
    return len(key) == 1 and key.isprintable()

words = []
current_word = ""
WORD_BOUNDARIES = {
    "<space>", "<enter>", "<tab>",
    ".", ",", "!", "?", ";", ":", 
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
        current_word += key
    # ignore everything else like <ctrl> or <cmd>

# Don't forget the last word if still being typed
if current_word:
    words.append(current_word)

# Count frequencies
counter = Counter(words)

# Show most common words
for word, count in counter.most_common(20):
    print(f"{word}: {count}")

