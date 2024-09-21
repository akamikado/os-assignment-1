import random
import string


def decode(encoded_string, key):
    key = key % 26
    decoded = ""

    for c in encoded_string:
        num = ord(c) + key
        if num > 122:
            num -= 26
        decoded += chr(num)

    return decoded


def encode(decoded_string, key):
    key = key % 26
    key = (26 - key % 26) % 26
    return decode(decoded_string, key)


def get_standard_permutation(word):
    diff = ord(word[0]) - ord("a")
    return encode(word, diff)


# These are the parameters, edit depending on the test case you want
min_word_length = 6
max_word_length = 25
n = 20
word_count = 5000
testcase_number = 1  # Can later be put in a loop to generate many cases

# Set containing unique, decoded matrix words
matrix_set = set()
# List containing all the words in words.txt
words = []
# Set containing the rotations of each word in the matrix that start with 'a'
standard_permutation_set = set()

# First generate the decoded words of the matrix
while len(matrix_set) < n * n:
    length = random.randint(min_word_length, max_word_length)
    word = "".join(random.choices(string.ascii_lowercase, k=length))

    # Check that no permutation of this word exists in the matrix
    standard_permutation = get_standard_permutation(word)
    if standard_permutation in standard_permutation_set:
        continue

    matrix_set.add(word)
    standard_permutation_set.add(standard_permutation)

# Set upper bound for how many times a word may occur in the file
max_occurrences = word_count // (n * n)
matrix_list = list(matrix_set)
matrix_answers = {}

# Fix the number of occurrences of each word in the matrix, and add to words list
for word in matrix_list:
    occurrences = random.randint(0, max_occurrences)
    matrix_answers[word] = occurrences
    words.extend([word] * occurrences)

# Fill remaining spots in words list with random words
while len(words) < word_count:
    if random.random() > 0.9:
        # Generate a random word
        length = random.randint(min_word_length, max_word_length)

        word_is_viable = False
        while not word_is_viable:
            word = "".join(random.choices(string.ascii_lowercase, k=length))
            if word not in matrix_set:
                word_is_viable = True

    else:
        # Use a rotated version of a word in the decoded matrix
        word = decode(random.choice(matrix_list), random.randint(1, 25))

    words.append(word)

# Shuffle the words list
random.shuffle(words)

# Output the file to be given to users
with open(f"words{testcase_number}.txt", "w") as output_file:
    output_file.write(" ".join(word for word in words))

# Save the matrix as a matrix
matrix = []
for i in range(n):
    row = []
    for j in range(n):
        row.append(matrix_list[j + i * n])
    matrix.append(row)

# print("The decoded matrix is:")
# for i in range(n):
#     print(" ".join(word for word in matrix[i]))
#
# print("\nThe number of occurrences are:")
# for i in range(n):
#     print(" ".join(str(matrix_answers[word]) for word in matrix[i]))

# Store the final answers for each diagonal
diagonal_answers = []
for i in range(2 * n - 1):
    col = min(i, n - 1)
    row = max(0, i - n + 1)

    diagona_sum = 0

    while col >= 0 and row < n:
        diagona_sum += matrix_answers[matrix[row][col]]
        col -= 1
        row += 1

    diagonal_answers.append(diagona_sum)

# Generate keys and encode the matrix
encoded_matrix = [[None for _ in range(n)] for _ in range(n)]
encoded_matrix[0][0] = matrix[0][0]
keys = []

for i in range(1, 2 * n - 1):
    key = random.randint(0, 100000)
    keys.append(key)

    col = min(i, n - 1)
    row = max(0, i - n + 1)

    while col >= 0 and row < n:
        encoded_matrix[row][col] = encode(matrix[row][col], key)
        col -= 1
        row += 1

# print("\nThe encoded matrix is:")
# for i in range(n):
#     print(" ".join(word for word in encoded_matrix[i]))

# Print in a manner directly copyable to C code for testing
# print("\nHere is the C copyable form:\n{", end="")
# for i in range(n):
#     print("{" + ", ".join('"' + word + '"' for word in encoded_matrix[i]) + "},")
# print("};")

# print("\nThe list of keys is:")
# print(" ".join(str(key) for key in keys))

# print("\nThe list of answers for the diagonals is:")
# print(" ".join(str(answer) for answer in diagonal_answers))

# Generate the testcase and answer file for the helper
with open(f"testcase{testcase_number}.bin", "w") as testcase_file:
    testcase_file.write(f"{n} {max_word_length + 1}\n")  # +1 for '\0'
    for i in range(n):
        testcase_file.write(" ".join(word for word in encoded_matrix[i]) + "\n")

with open(f"answer{testcase_number}.bin", "w") as answer_file:
    answer_file.write(f"{len(diagonal_answers)}\n")
    answer_file.write(" ".join(str(answer) for answer in diagonal_answers) + "\n")
    answer_file.write(" ".join(str(key) for key in keys) + " 0")

