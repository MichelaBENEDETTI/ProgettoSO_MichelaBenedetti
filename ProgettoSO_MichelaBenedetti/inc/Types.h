// Definizione della struttura per i punti
typedef struct {
  double x_coordinate;
  double y_coordinate;
} CoordinatePoint;

// Definizione della struttura per i centroidi
typedef struct {
  CoordinatePoint point;
} Centroid;

#define MAX_CENTROIDS 100

// Struttura dentro al messaggio
typedef struct {
  double variance_value;
  Centroid centroids[MAX_CENTROIDS];
} MessageStructure;

// Definizione della struttura per il messaggio
typedef struct {
  long message_type;
  MessageStructure content;
} Message;

