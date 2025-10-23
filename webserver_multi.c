#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "webserver.h"
#include <semaphore.h>
#include <signal.h>
#include<unistd.h>

#define MAX_REQUEST 100

int buffer[MAX_REQUEST];
int port, numThread;
pthread_mutex_t mutex;
sem_t full;
sem_t empty;
int bufLoc;
int in;
int out;

void *worker() {
	// Worker loop
	while (1) {
		sem_wait(&full);                // Wait for a spot in the buffer to fill
		pthread_mutex_lock(&mutex);     // Lock the mutex

		/* DO SOMETHING */
		int fd = buffer[out];        // Get the fd from the buffer
		out = (out + 1) % MAX_REQUEST; // Move the buffer index back 1

		pthread_mutex_unlock(&mutex);   // Unlock the mutex
		sem_post(&empty);               // Signal there is a new empty space in buffer

		process(fd);                    // Process the fd's request
	} // while

	return;
} // worker

void *listener() {
	int r;
	struct sockaddr_in sin;
	struct sockaddr_in peer;
	int peer_len = sizeof(peer);
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	r = bind(sock, (struct sockaddr *) &sin, sizeof(sin));
	if (r < 0) {
		perror("Error binding socket:");
		return;
	} // if

	r = listen(sock, 5);
	if (r < 0) {
		perror("Error listening socket:");
		return;
	} // if

	printf("HTTP server listening on port %d\n", port);
	while (1) {
		int s;
		s = accept(sock, NULL, NULL);
		if (s < 0) break;

		/* PLACE FD IN BUFFER */
		sem_wait(&empty);               // Wait for there to be an empty spot in the buffer
		pthread_mutex_lock(&mutex);     // Lock the mutex

		buffer[in] = s;                 // Place fd into buffer
		in = (in + 1) % MAX_REQUEST;    // Move the buffer index forward 1

		pthread_mutex_unlock(&mutex);   // Unlock the mutex
		sem_post(&full);                // Signal that there is a new full space in the buffer
	} // while

	close(sock);
} // listener

void threadMonitor(pthread_t workers[], pthread_t *listenerThread) {
	while (1) {
		// Check each of the worker threads
		for (int i = 0; i < numThread; i++) {
			if (pthread_kill(workers[i], 0) != 0) {
				printf("Error: Worker thread %i died! Restarting thread %i...\n", i+1, i+1);
				while (pthread_create(&workers[i], NULL, worker, NULL) != 0) {
					printf("Error restarting thread %i, trying again...\n", i+1);
				} // while
			} // if
		} // for

		// Check the listener thread
		if (pthread_kill(*listenerThread, 0) != 0) {
			printf("Error: Listener thread died! Restarting thread...\n");
			while (pthread_create(listenerThread, NULL, listener, NULL) != 0) {
				printf("Error restarting listener thread, trying again...\n");
			} // while
		} // if

		// Sleep
		sleep(1);
	} // while
} // threadMonitor

void thread_control() {

	// Set the semaphore values
	pthread_mutex_init(&mutex, NULL);    // init the mutex
	sem_init(&full, 0, 0);               // number of full spaces in buffer (0)
	sem_init(&empty, 0, MAX_REQUEST);    // number of empty spaces in buffer (MAX_REQUEST)

	// Set buffer pointer
	in = 0;
	out = 0;

	// Create 1 listener thread
	pthread_t listenerThread;
	if (pthread_create(&listenerThread, NULL, listener, NULL)) {
		printf("Error: Failed to create listener thread.");
		exit(1);
	} // if
	// printf("Listener thread created and ready.\n");

	// Create numThreads worker threads
	pthread_t workerThread[numThread];
	for (int i = 0; i < numThread; i++) {
		if (pthread_create(&workerThread[i], NULL, worker, NULL)) {
			printf("Error: Failed to create worker thread %i.\n", i);
			exit(1);
		} // if
		// printf("Worker thread %i created and ready to process requests.\n", i+1);
	} // for

	// Monitor the threads (BROKEN)
	threadMonitor(workerThread, &listenerThread);

	// TODO: Destroy the semaphores(?)
} // thread_control


int main(int argc, char *argv[]) {
	if(argc != 3 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000) {
		fprintf(stderr, "./webserver_multi PORT(2001 ~ 49999) #_of_threads\n");
		return 0;
	} // if

	int i;
	port = atoi(argv[1]);
	numThread = atoi(argv[2]);
	thread_control();
	return 0;
} // main
