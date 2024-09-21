#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
typedef struct trie_node trie_node, trie;
struct trie_node {
  trie *children[52];
  int eow;
  int count;
};

typedef struct msg_buf msg_buf;
struct msg_buf {
  long mtype;
  int key;
};

typedef struct thread_data data_t;
struct thread_data {
  trie *root;
  char *word;
  int key;
  int occurrences;
  pthread_mutex_t lock;
};

trie *create_trie() {
  trie *root = (trie *)malloc(sizeof(trie));
  for (int i = 0; i < 52; i++) {
    root->children[i] = NULL;
  }
  root->eow = 0;
  root->count = 0;
  return root;
}

void insert_trie(trie *root, char *word, int length) {
  trie *current = root;
  for (int i = 0; i < length; i++) {
    if (word[i] >= 'A' && word[i] <= 'Z') {
      if (current->children[word[i] - 'A' + 26] == NULL) {
        current->children[word[i] - 'A' + 26] = create_trie();
      }
      current = current->children[word[i] - 'A' + 26];
    } else if (word[i] >= 'a' && word[i] <= 'z') {
      if (current->children[word[i] - 'a'] == NULL) {
        current->children[word[i] - 'a'] = create_trie();
      }
      current = current->children[word[i] - 'a'];
    }
  }
  current->eow = 1;
  current->count++;
}

int number_of_occurrences(trie *root, char *word, int length) {
  trie *current = root;
  for (int i = 0; i < length; i++) {
    if (word[i] >= 'A' && word[i] <= 'Z') {
      if (current->children[word[i] - 'A' + 26] == NULL) {
        return 0;
      }
      current = current->children[word[i] - 'A' + 26];
    } else if (word[i] >= 'a' && word[i] <= 'z') {
      if (current->children[word[i] - 'a'] == NULL) {
        return 0;
      }
      current = current->children[word[i] - 'a'];
    }
  }
  int *result = (int *)malloc(2 * sizeof(int));
  return current->count;
}

void delete_trie(trie *root) {
  if (root == NULL) {
    return;
  }

  for (int i = 0; i < 52; i++) {
    if (root->children[i] != NULL) {
      delete_trie(root->children[i]);
    }
  }

  free(root);
}

void decipher_caesar_cipher(char *word, int length, int key) {
  for (int i = 0; i < length; i++) {
    if (word[i] >= 'A' && word[i] <= 'Z') {
      word[i] = (word[i] - 'A' + 26 + key) % 26 + 'A';
    } else if (word[i] >= 'a' && word[i] <= 'z') {
      word[i] = (word[i] - 'a' + 26 + key) % 26 + 'a';
    }
  }
}

void read_input_file(char *input_file, int *matrix_size, int *string_length,
                     int *shm_key, int *msg_queue_key) {
  FILE *ptr = fopen(input_file, "r");
  if (ptr == NULL) {
    exit(1);
  }
  if (fscanf(ptr, "%d", matrix_size) == EOF) {
    exit(1);
  }
  if (fscanf(ptr, "%d", string_length) == EOF) {
    exit(1);
  }
  if (fscanf(ptr, "%d", shm_key) == EOF) {
    exit(1);
  }
  if (fscanf(ptr, "%d", msg_queue_key) == EOF) {
    exit(1);
  }

  fclose(ptr);

  return;
}

trie *read_word_file(char *word_file) {
  FILE *ptr = fopen(word_file, "r");
  if (ptr == NULL) {
    exit(1);
  }
  fseek(ptr, 0, SEEK_END);
  long file_size = ftell(ptr);
  fseek(ptr, 0, SEEK_SET);
  char *words = (char *)malloc((file_size + 1) * sizeof(char));
  fread(words, 1, file_size, ptr);
  words[file_size] = '\0';
  fclose(ptr);

  int i = 0, j = 0;
  trie *root = create_trie();

  while (1) {
    if (words[i] == ' ' || words[i] == '\0') {
      char *word = (char *)malloc((i - j + 1) * sizeof(char));
      strncpy(word, words + j, i - j);
      word[i - j] = '\0';
      insert_trie(root, word, i - j);
      free(word);
      j = i + 1;
    }
    if (words[i] == '\0') {
      break;
    }
    i++;
  }

  free(words);

  return root;
}

void *find_occurrences_of_word(void *arg) {
  data_t *data = (data_t *)arg;
  int length = strlen(data->word);
  decipher_caesar_cipher(data->word, length, data->key);
  int result = number_of_occurrences(data->root, data->word, length);

  pthread_mutex_lock(&data->lock);

  data->occurrences += result;

  pthread_mutex_unlock(&data->lock);

  return NULL;
}

void iterate_through_matrix(int matrix_size, int string_length,
                            char (*shm_ptr)[matrix_size][string_length],
                            int msgq_key, trie *root) {
  int msgq_id = msgget(msgq_key, 0444);
  if (msgq_id == -1) {
    exit(1);
  }

  struct msg_buf snd_buffer, rcv_buffer;
  snd_buffer.mtype = 1;
  rcv_buffer.key = 0;
  rcv_buffer.mtype = 2;

  pthread_mutex_t lock;
  if (pthread_mutex_init(&lock, NULL) != 0) {
    exit(1);
  }

  for (int k = 0; k < 2 * matrix_size - 1; k++) {
    int num_threads = k < matrix_size ? k + 1 : 2 * matrix_size - k - 1;
    pthread_t threads[num_threads];
    data_t thread_data[num_threads];

    for (int i = 0; i < num_threads; i++) {
      int row, col;

      if (k < matrix_size) {
        row = i;
        col = k - i;
      } else {
        row = k - (matrix_size - 1) + i;
        col = (matrix_size - 1) - i;
      }
      if (row >= matrix_size || col >= matrix_size) {
        continue;
      }

      thread_data[i].word = shm_ptr[row][col];
      thread_data[i].key = rcv_buffer.key;
      thread_data[i].root = root;
      thread_data[i].occurrences = 0;
      thread_data[i].lock = lock;

      if (pthread_create(&threads[i], NULL, find_occurrences_of_word,
                         (void *)&thread_data[i])) {
        perror("Error in pthread_create");
        exit(1);
      }
    }

    int sum = 0;
    for (int i = 0; i < num_threads; i++) {
      pthread_join(threads[i], NULL);
      sum += thread_data[i].occurrences;
    }

    snd_buffer.key = sum;

    if (msgsnd(msgq_id, &snd_buffer,
               sizeof(snd_buffer) - sizeof(snd_buffer.mtype), 0) == -1) {
      perror("Error in msgsnd");
      exit(1);
    }

    if (msgrcv(msgq_id, &rcv_buffer,
               sizeof(rcv_buffer) - sizeof(rcv_buffer.mtype), 2, 0) == -1) {
      perror("Error in msgrcv");
      exit(1);
    }

    if (rcv_buffer.key == -1) {
      exit(1);
    }

    rcv_buffer.key %= 26;
  }

  if (pthread_mutex_destroy(&lock) != 0) {
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    return 1;
  }

  char input_file[16];
  char word_file[16];

  sprintf(input_file, "input%d.txt", atoi(argv[1]));
  input_file[11] = '\0';
  sprintf(word_file, "words%d.txt", atoi(argv[1]));
  word_file[10] = '\0';

  int matrix_size;
  int string_length;
  int shm_key;
  int msg_queue_key;

  read_input_file(input_file, &matrix_size, &string_length, &shm_key,
                  &msg_queue_key);

  int shm_id = shmget(
      shm_key, sizeof(char[matrix_size][matrix_size][string_length]), 0444);

  if (shm_id == -1) {
    return 1;
  }

  char(*shm_ptr)[matrix_size][string_length];
  shm_ptr = shmat(shm_id, NULL, 0);
  if (shm_ptr == (void *)-1) {
    return 1;
  }

  trie *root = read_word_file(word_file);

  iterate_through_matrix(matrix_size, string_length, shm_ptr, msg_queue_key,
                         root);

  if (shmdt(shm_ptr) == -1) {
    return 1;
  }
  delete_trie(root);

  return 0;
}
