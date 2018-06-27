#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"
#include "server.h"
#include "directory.h"
#include "thread_counter.h"
#include "csapp.h"

static void terminate();
static char *extract_port(int argc, char* argv[]);

THREAD_COUNTER *thread_counter;
int serverfd;
int *clientfd;

void sighup_handler(int sig) { terminate(); }

int main(int argc, char* argv[]) {
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    char *port = extract_port(argc, argv);
    if(port == NULL) {
        fprintf(stderr, "Invalid arguments. Run using: bin/bavarde -p <port>\n");
        exit(EXIT_FAILURE);
    }

    // Perform required initializations of the thread counter and directory.
    thread_counter = tcnt_init();
    dir_init();

    // Install the SIGHUP handler, so that receipt of SIGHUP will perform
    // a clean shutdown of the server.
    Signal(SIGHUP, sighup_handler);

    // Set up the server socket.
    //int serverfd; // The file descriptor for the server.
    serverfd = Open_listenfd(port);
    if(serverfd < 0) {
        terminate();
        exit(EXIT_FAILURE);
    }

    // Enter a loop to accept connections on this socket.
    // For each connection, a thread should be started to
    // run function bvd_client_service().
    struct sockaddr_in client_addr;
    //int *clientfd; // The file descriptor for the client.
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid;

    while(1) {
        // Accept the connection from the client to the server.
        clientfd = malloc(sizeof(int));
        *clientfd = Accept(serverfd, (SA *) &client_addr, &client_len);

        if(*clientfd < 0) {
            free(clientfd);
            terminate();
            exit(EXIT_FAILURE);
        }

        // Start a new thread and run bvd_client_service.
        Pthread_create(&tid, NULL, bvd_client_service, clientfd);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the Bavarde server will function.\n");

    terminate();
}

/*
 * Obtains the port number from the arguments.
 */
static char *extract_port(int argc, char* argv[]) {
    int flag = 0;

    if(strcmp(argv[1], "-p") != 0) {
        return NULL;
    }

    if(argc > 3) {
        return NULL;
    }

    while((flag = getopt(argc, argv, "+p:")) != -1) {
        switch(flag) {
            case 'p': /* Empty statement to allow assignment. */;
                int port = atoi(optarg);

                return port < 1024 ? NULL : optarg;
            default:
                break;
        }
    }

    return NULL;
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int sig) {
    if(serverfd != -1) {
        close(serverfd);
    }

    if(clientfd != NULL) {
        free(clientfd);
    }

    // Shut down the directory.
    // This will trigger the eventual termination of service threads.
    dir_shutdown();

    debug("Waiting for service threads to terminate...");
    tcnt_wait_for_zero(thread_counter);
    debug("All service threads terminated.");

    tcnt_fini(thread_counter);
    dir_fini();
    exit(EXIT_SUCCESS);
}
