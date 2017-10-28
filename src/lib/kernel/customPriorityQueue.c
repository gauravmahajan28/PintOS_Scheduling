#include "../stdio.h"
#include "customPriorityQueue.h"
#include "../../threads/thread.h"
#include "../debug.h"

/**
*	Return the minimum waiting thread from priority queue
*/
struct thread *getMinimumFromCustomPriorityQueue(CustomPriorityQueue *customPriorityQueue)
{
	if(customPriorityQueue->currentQueueSize == 0)
		return NULL;

	else
		return (customPriorityQueue->threads[0]);


}

/**
*	Delete minimum waiting thread from priority queue
*/
struct thread *deleteFromCustomPriorityQueue(CustomPriorityQueue *customPriorityQueue)
{
	int i;
	if(customPriorityQueue->currentQueueSize == 0)
		return NULL;



	struct thread *minimumThread = customPriorityQueue->threads[0];

	swapCustomPriorityQueueThreads(customPriorityQueue, 0 , (customPriorityQueue->currentQueueSize - 1));

	(customPriorityQueue->currentQueueSize)--;

	int currentParentLocation = 0;
	int maxParentLocation = (customPriorityQueue->currentQueueSize - 2) / 2;
	int currentQueueSize = customPriorityQueue->currentQueueSize;

	while(currentParentLocation <= maxParentLocation)
	{

		struct thread *currentParentThread = customPriorityQueue->threads[currentParentLocation];

		int minimumTimeLocation = currentParentLocation;

		int leftChildLocation = getLeftChildLocation(currentParentLocation);
		int rightChildLocation = getRightChildLocation(currentParentLocation);

		if(leftChildLocation < currentQueueSize)
		{
			struct thread *leftChildThread = customPriorityQueue->threads[getLeftChildLocation(currentParentLocation)];

			if((currentParentThread->time) > (leftChildThread->time))
				minimumTimeLocation = leftChildLocation;
		}

		if(rightChildLocation < currentQueueSize)
		{


			struct thread *rightChildThread = customPriorityQueue->threads[getRightChildLocation(currentParentLocation)];
			if((customPriorityQueue->threads[minimumTimeLocation]->time) > rightChildThread->time)
				minimumTimeLocation = rightChildLocation;

		}
		if(minimumTimeLocation != currentParentLocation)
		{
			swapCustomPriorityQueueThreads(customPriorityQueue, currentParentLocation , minimumTimeLocation);
			currentParentLocation = minimumTimeLocation;
		}
		else
			break;

	}

}

/**
* 	insert thread into priority queue, comparison is done with respect to time
*/
void insertIntoCustomPriorityQueue(CustomPriorityQueue *customPriorityQueue, struct thread *t)
{

	// handle error
	if(customPriorityQueue->currentQueueSize == customPriorityQueue->maxQueueSize)
		return;


	int currentLocation = customPriorityQueue->currentQueueSize;

	customPriorityQueue->threads[currentLocation] = t;

	while(currentLocation > 0)
	{
		struct thread *currentThread = customPriorityQueue->threads[currentLocation];
		struct thread *parentThread = customPriorityQueue->threads[getParentLocation(currentLocation)];
		if(currentThread->time < parentThread->time)
		{
			swapCustomPriorityQueueThreads(customPriorityQueue, currentLocation, getParentLocation(currentLocation));
			currentLocation = getParentLocation(currentLocation);
		}
		else
			break;
	}

	//increase by one
	(customPriorityQueue->currentQueueSize)++;

}

/**
* 	get right child location
*/
int getRightChildLocation(int parentLocation)
{
	return (2*parentLocation + 2);
}

/**
* 	init sizes
*/
void customPriorityQueueInit(CustomPriorityQueue *customPriorityQueue, int size)
{
	customPriorityQueue->currentQueueSize = 0;
	customPriorityQueue->maxQueueSize = size;

}

/**
*	swap thread pointers between two locations
*/
void swapCustomPriorityQueueThreads(CustomPriorityQueue *customPriorityQueue, int currentLocation, int parentLocation)
{
	struct thread *tempThread = customPriorityQueue->threads[currentLocation];
	customPriorityQueue->threads[currentLocation] = customPriorityQueue->threads[parentLocation];
	customPriorityQueue->threads[parentLocation] = tempThread;

}

/**
*	get parent location
*/
int getParentLocation(int childLocation)
{
	return (childLocation-1)/2;
}

/**
*	get left child location
*/
int getLeftChildLocation(int parentLocation)
{
	return (2 * parentLocation + 1);
}

/**
*	method to check whether priority queue is empty
*/
int isCustomPriorityQueueEmpty(CustomPriorityQueue *customPriorityQueue)
{
	if(customPriorityQueue->currentQueueSize == 0)
		return 1; // true
	return 0;// false

}
