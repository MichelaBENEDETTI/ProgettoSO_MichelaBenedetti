#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "Types.h"
#include "errExit.h"

#define convergence_threshold 1e-10

double euclidean_distance(CoordinatePoint p1, CoordinatePoint p2) {
  double dx = p1.x_coordinate - p2.x_coordinate;
  double dy = p1.y_coordinate - p2.y_coordinate;
  return sqrt(dx * dx + dy * dy);
}

int is_duplicate(Centroid c, Centroid centroids[], int num_centroids) {
    for (int i = 0; i < num_centroids; i++) {
        if (c.point.x_coordinate == centroids[i].point.x_coordinate && c.point.y_coordinate == centroids[i].point.y_coordinate) {
            return 1; // Duplicate found
        }
    }
    return 0; // No duplicate
}

void update_centroid(Centroid *centroid, double sum_x, double sum_y, int num_points) {
  centroid->point.x_coordinate = sum_x / num_points;
  centroid->point.y_coordinate = sum_y / num_points;
}

double calculateVariance(CoordinatePoint points[], Centroid centroids[], int cluster[], int n) {
  double sumDistances = 0.0;
  for (int i = 0; i < n; ++i) {
    sumDistances += pow(euclidean_distance(points[i], centroids[cluster[i]].point), 2);
  }
  return sumDistances / n;
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Usage: %s <IPC key> <K> <P>\n", argv[0]);
    return 1;
  }

  key_t ipc_key = atoi(argv[1]);  
  int K = atoi(argv[2]);          
  int nPoints = atoi(argv[3]);    

  int cluster[nPoints];
  int shmid = shmget(ipc_key, 0, SHM_RDONLY);
  if (shmid == -1) {
    errExit("shmget");
    return 1;
  }

  CoordinatePoint *points = (CoordinatePoint *)shmat(shmid, NULL, 0);  
  if (points == (CoordinatePoint *)-1) {
    errExit("shmat");
    return 1;
  }

  int msgid = msgget(ipc_key, S_IWUSR);
  if (msgid == -1) {
    errExit("worker: Error getting message queue ID");
  }

  Centroid prev_centroids[K];

  while (1) {
    srand(time(NULL));
    Centroid centroids[K];

    for (int i = 0; i < K; i++) {
      Centroid c;
      do { 
          int random_index = rand() % nPoints;
          c.point = points[random_index];
      } while(is_duplicate(c, centroids, i));
      centroids[i] = c;
      prev_centroids[i] = c;
    }

    while (1) {
      for (int i = 0; i < nPoints; i++) {  
        double min_distance = INFINITY;
        int closest_cluster = 0;
        for (int j = 0; j < K; j++) {  
          double d = euclidean_distance(points[i], centroids[j].point);
          if (d < min_distance) {
            min_distance = d;
            closest_cluster = j;
          }
        }
        cluster[i] = closest_cluster;  
      }

      for (int i = 0; i < K; i++) {  
        double sum_point_x = 0;
        double sum_point_y = 0;
        int num_points = 0;
        for (int j = 0; j < nPoints; j++) {  
          if (cluster[j] == i) {  
            sum_point_x += points[j].x_coordinate;
            sum_point_y += points[j].y_coordinate;
            num_points++;
          }
        }
        if (num_points > 1) {
          update_centroid(&centroids[i], sum_point_x, sum_point_y, num_points);
        } else {  
            Centroid c;     
            do {
              int random_index = rand() % nPoints;
              c.point = points[random_index];
            } while(is_duplicate(c, centroids, K));
            centroids[i] = c;
        }
      }  

      double centroid_diff = 0.0;
      for (int i = 0; i < K; i++) {
        double d = euclidean_distance(centroids[i].point, prev_centroids[i].point);
        centroid_diff += d;
      }

      if (centroid_diff < convergence_threshold) {
        break;
      }

      for (int i = 0; i < K; i++) {
        prev_centroids[i] = centroids[i];
      }
    }  

    double variance = calculateVariance(points, centroids, cluster, nPoints);

    Message allert;
    allert.message_type = 1;
    allert.content.variance_value = variance;
    for (int i = 0; i < K; i++) {
      allert.content.centroids[i].point.x_coordinate = centroids[i].point.x_coordinate;
      allert.content.centroids[i].point.y_coordinate = centroids[i].point.y_coordinate;
    }

    /*
    printf("Message: x:%f, y:%f, v:%f\n", allert.content.centroids[0].point.x_coordinate,
           allert.content.centroids[0].point.y_coordinate, allert.content.variance_value);
    */

    if (msgsnd(msgid, &allert, sizeof(allert) - sizeof(long), 0) == -1) {
      errExit("msgsnd");
    }
  }  

  shmdt(points);

  return 0;
}  

