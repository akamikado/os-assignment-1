#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PERMS 0666
#define CONSTANT 100000000

struct timeval start, stop;

struct MessageBuffer {
  long mtype;
  int key;
};

int main(int argc, char *argv[]) {
  srand(time(0));
  // Create Shared Memory Key
  key_t shmKey = rand() % CONSTANT;

  // Get matrix dimensions
  int rowColSize, stringSize;
  char testcaseFileName[15];
  sprintf(testcaseFileName, "testcase%s.bin", argv[1]);
  FILE *testcaseFile = fopen(testcaseFileName, "r");
  fscanf(testcaseFile, "%d", &rowColSize);
  fscanf(testcaseFile, "%d", &stringSize);

  // printf("The matrix size is %d and the max word size is %d\n", rowColSize,
  //        stringSize);

  // Create shared memory segment
  int shmId;
  char(*shmPtr)[rowColSize][stringSize];

  shmId = shmget(shmKey, sizeof(char[rowColSize][rowColSize][stringSize]),
                 PERMS | IPC_CREAT);
  if (shmId == -1) {
    perror("Error in shmget");
    exit(1);
  }

  shmPtr = shmat(shmId, NULL, 0);
  if (shmPtr == (void *)-1) {
    perror("Error in shmat");
    exit(1);
  }

  for (int i = 0; i < rowColSize; i++) {
    for (int j = 0; j < rowColSize; j++) {
      fscanf(testcaseFile, " %s", shmPtr[i][j]);
      // DEBUG
      // printf("%s\n", shmPtr[i][j]);
    }
  }

  // DEBUG
  // for (int i = 0; i < rowColSize; i++) {
  //   for (int j = 0; j < rowColSize; j++) {
  //     fscanf(testcaseFile, " %s", shmPtr[i][j]);
  //     printf("%s\n", shmPtr[i][j]);
  //   }
  // }

  fclose(testcaseFile);

  // Store answer array and new keys
  int ansArrayLength;
  char answerFileName[15];
  sprintf(answerFileName, "answer%s.bin", argv[1]);
  FILE *answerFile = fopen(answerFileName, "r");
  fscanf(answerFile, "%d", &ansArrayLength);
  int ansArray[ansArrayLength], keyArray[ansArrayLength];

  for (int i = 0; i < ansArrayLength; i++) {
    fscanf(answerFile, "%d", &ansArray[i]);
  }

  for (int i = 0; i < ansArrayLength; i++) {
    fscanf(answerFile, "%d", &keyArray[i]);
  }

  fclose(answerFile);

  // Delete the answer file before running the student's solution.
  int deletionProcessId = fork();
  if (deletionProcessId == -1) {
    perror("Error while forking");
    exit(1);
  }

  if (deletionProcessId == 0) {
    if (execlp("rm", "rm", answerFileName, NULL) == -1) {
      perror("Error in execlp");
      exit(1);
    }
  }

  // Create message queue key
  key_t msgKey = rand() % CONSTANT;

  // Store the input values for the solution program
  char inputFileName[15];
  sprintf(inputFileName, "input%s.txt", argv[1]);
  FILE *inputFile = fopen(inputFileName, "w");
  fprintf(inputFile, "%d\n%d\n%d\n%d", rowColSize, stringSize, shmKey, msgKey);
  fflush(inputFile);

  printf("Testcase %s\n", argv[1]);

  // Create message queue
  int msgId;

  if ((msgId = msgget(msgKey, PERMS | IPC_CREAT)) == -1) {
    perror("Error in msgget");
    exit(1);
  }

  gettimeofday(&start, NULL);

  // Run the solution program
  int childId = fork();

  if (childId == -1) {
    perror("Error while forking");
    exit(1);
  }

  if (childId == 0) {
    if (execlp("./solution", "solution", argv[1], NULL) == -1) {
      perror("Error in execlp");
      exit(1);
    }
  }

  // Main Logic
  int index = 0, error = 0;
  struct MessageBuffer sentMessage, recievedMessage;
  while (index < ansArrayLength) {
    sentMessage.mtype = 2;
    if (msgrcv(msgId, &recievedMessage,
               sizeof(recievedMessage) - sizeof(recievedMessage.mtype), 1,
               0) == -1) {
      perror("Error in msgrcv");
      exit(1);
    }
    if (recievedMessage.key == ansArray[index]) {
      sentMessage.key = keyArray[index];
      index++;
    } else {
      sentMessage.key = -1;
      error = 1;
    }
    if (msgsnd(msgId, &sentMessage,
               sizeof(sentMessage) - sizeof(sentMessage.mtype), 0) == -1) {
      perror("Error in msgsnd");
      exit(1);
    }
    if (error) {
      break;
    }
  }

  wait(NULL);

  gettimeofday(&stop, NULL);
  double result =
      ((stop.tv_sec - start.tv_sec)) + ((stop.tv_usec - start.tv_usec) / 1e6);

  for (int i = 0; i < ansArrayLength; i++) {
    if (i == index && error) {
      printf("Failed at diagonal %d\n", i + 1);
      break;
    } else {
      printf("Correct at diagonal %d\n", i + 1);
    }
  }

  // DEBUG
  printf("Time taken by your solution to execute: %lf seconds\n", result);
  printf(
      "Please note that this number may fluctuate with server load, and won't "
      "be used for the final evaluation\n");

  // Terminate message queue
  if (msgctl(msgId, IPC_RMID, NULL) == -1) {
    perror("Error in msgctl");
    exit(1);
  }

  // Terminate shared memory
  if (shmdt(shmPtr) == -1) {
    perror("Error in shmdt");
    exit(1);
  }

  if (shmctl(shmId, IPC_RMID, 0) == -1) {
    perror("Error in shmctl");
    return 1;
  }

  return 0;
}
