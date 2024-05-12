#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Types.h"
#include "errExit.h"

#define nMAX_IMPROVEMENT 1000

CoordinatePoint *points;
int shmid;
int queueid;

// Signal handler for SIGINT
void sigterm_handler(int signum) {
  // Detach from shared memory and message queue
  shmdt(points);  // Detach from shared memory
  shmctl(shmid, IPC_RMID, NULL);  // Delete shared memory segment
  msgctl(queueid, IPC_RMID, NULL);  // Delete message queue
  printf("Master terminating...\n");
  fflush(stdout);
  exit(0);
}

int main(int argc, char *argv[]) {
  // Set signal handler for SIGINT
  signal(SIGINT, sigterm_handler);

  // Check command-line arguments
  if (argc != 5) {
    printf("Usage: %s <K> <N> <key> <dataset>\n", argv[0]);
    return 1;
  }

  // Parse command-line arguments
  int K = atoi(argv[1]);  // Number of clusters
  int N = atoi(argv[2]);  // Number of worker processes
  key_t key = atoi(argv[3]);  // Key for shared memory and message queue
  char *dataset = argv[4];  // Dataset file

  // Open dataset file
  FILE *f = fopen(dataset, "r");
  if (!f) {
    errExit("Error opening dataset file");
    return 1;
  }

  // Count number of lines in dataset file
  int lines = 0;
  char cent;
  while ((cent = fgetc(f)) != EOF) {
    if (cent =='\n') {
      lines++;
    }
  }
  fseek(f, 0, SEEK_SET);

  // Ensure there are enough lines in the dataset
  if (lines <= K || lines <= 0) {
    errExit("Invalid number of clusters or points\n");
  }

  // Create shared memory segment
  shmid = shmget(key, sizeof(CoordinatePoint) * lines, IPC_CREAT | S_IRUSR | S_IWUSR);
  if (shmid == -1) {
    errExit("Error creating shared memory segment");
    return 1;
  }

  // Attach to shared memory segment
  points = (CoordinatePoint *)shmat(shmid, NULL, 0);
  if (points == (CoordinatePoint *)-1) {
    errExit("Error attaching to shared memory segment");
    return 1;
  }

  // Read dataset and populate shared memory
  int i = 0;
  char line[100];
  while (fgets(line, 100, f) != NULL) {
    char *token = strtok(line, ",");
    points[i].x_coordinate = atof(token);
    token = strtok(NULL, ",");
    points[i].y_coordinate = atof(token);
    i++;
  }
  fclose(f);

  // Create message queue
  queueid = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);
  if (queueid == -1) {
    errExit("Error creating message queue");
    return 1;
  }

  // Fork worker processes
  pid_t pids[N];
  for (int i = 0; i < N; i++) {
    pids[i] = fork();
    if (pids[i] == -1) {
      errExit("Error creating child process");
    } else if (pids[i] == 0) {
      // Child process
      char Kstr[10], keystr[10], linesstr[100];
      sprintf(Kstr, "%d", K);
      sprintf(keystr, "%d", key);
      sprintf(linesstr, "%d", lines);
      if (execl("worker", "worker", keystr, Kstr, linesstr, (char *)NULL) == -1) {
        errExit("execl failed");
      }
    }
  }

  // Counter for number of messages that did not improve the clustering
  int nadd = 0;
  double var_min = 1e100;

  // Keep receiving messages from workers
  while (1) {
    // Read a single message
    Message allert;
    Centroid b_centroids[K];
    if (msgrcv(queueid, &allert, sizeof(allert) - sizeof(long), 0, 0) == -1) {  
      errExit("Error receiving message");
    }

    // Update best clustering (lowest variance)
    if (allert.content.variance_value < var_min) {
      var_min = allert.content.variance_value;
      nadd = 0;
      for (int i = 0; i < K; i++) {
        b_centroids[i].point.x_coordinate = allert.content.centroids[i].point.x_coordinate;
        b_centroids[i].point.y_coordinate = allert.content.centroids[i].point.y_coordinate;
      }
    } else {
      nadd++;
    }

    // Debug print statements
    /*
    printf("Received message from worker %f\n", allert.content.variance_value);
    */

    // Increment nadd if the variance did not improve
    if (nadd == nMAX_IMPROVEMENT) {
      // Dump centroids to file
      FILE *f = fopen("centroids.csv", "w");
      if (f == NULL) {
        errExit("Error opening file");
      }
      for (int i = 0; i < K; i++) {
        fprintf(f, "%.2lf,%.2lf\n", b_centroids[i].point.x_coordinate, b_centroids[i].point.y_coordinate);
        printf("Final centroid: %2lf, %2lf\n", b_centroids[i].point.x_coordinate, b_centroids[i].point.y_coordinate);
      }
      fclose(f);

      // Send SIGINT to all workers
      for (int i = 0; i < N; i++) {
        kill(pids[i], SIGINT);
      }
      break;
    }
  }

  // Gather exit status of all worker processes
  for (int i = 0; i < N; i++) {
    wait(NULL);
  }

  // Detach from shared memory segment
  shmdt(points);

  // Delete shared memory
  shmctl(shmid, IPC_RMID, NULL);

  // Deallocate message queue
  msgctl(queueid, IPC_RMID, NULL);

  printf("Master terminating...\n");

  return 0;
}

