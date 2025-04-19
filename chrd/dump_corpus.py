import sqlite3

# Connect to the SQLite database
conn = sqlite3.connect("key_log.sqlite")
cur = conn.cursor()

# Fetch all key codes in order
cur.execute("SELECT key_code FROM key_log ORDER BY time_utc")
keystrokes = [row[0] for row in cur.fetchall()]

# Define word boundaries
WORD_BOUNDARIES = {
    "<space>", "<enter>", "<tab>", ".", ",", "!", "?", ";", ":",
    "(", ")", "[", "]", "{", "}", "\"", "'"
}

words = []
current_word = ""

for key in keystrokes:
    if key in WORD_BOUNDARIES:
        if current_word:
            words.append(current_word)
            current_word = ""
    elif key == "<backspace>":
        current_word = current_word[:-1]
    elif len(key) == 1 and key.isprintable():
        current_word += key.lower()

if current_word:
    words.append(current_word)

# Write to file
with open("my_corpus.txt", "w", encoding="utf-8") as f:
    f.write(" ".join(words))

print("Dumped corpus to my_corpus.txt")

