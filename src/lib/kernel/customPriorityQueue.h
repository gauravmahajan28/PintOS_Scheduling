#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
*	Custom priority queue structure
*/
typedef struct CustomPriorityQueue
  {
  	struct thread *threads[1000];
	int currentQueueSize;
	int maxQueueSize;

  }CustomPriorityQueue;

int getParentLocation(int childLocation);

int getLeftChildLocation(int parentLocation);

int getRightChildLocation(int parentLocation);

void customPriorityQueueInit(CustomPriorityQueue *customPriorityQueue, int size);

void swapCustomPriorityQueueThreads(CustomPriorityQueue *customPriorityQueue, int currentLocation, int parentLocation);

void insertIntoCustomPriorityQueue(CustomPriorityQueue *customPriorityQueue, struct thread *t);

struct thread *deleteFromCustomPriorityQueue(CustomPriorityQueue *customPriorityQueue);

struct thread *getMinimumFromCustomPriorityQueue(CustomPriorityQueue *customPriorityQueue);

int isCustomPriorityQueueEmpty(CustomPriorityQueue *customPriorityQueue);
