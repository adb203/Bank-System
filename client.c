#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>

#include <pthread.h>
#include	<sys/time.h>
#include	<signal.h>

#include "bank.h"

int nsock_fd; // network socket file descriptor

static void sigint_handler(int signo)
{
  char command[5] = "exit";
	printf( "Closing session. . .\n");

  if ( (write(nsock_fd, command, strlen(command) ) ) < 0){
         printf("Session has expired.\n");
  }
  return;
}

//get input from server and output to server
void * read_server_user(void * args){
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    int netsock_fd = *(int *)args;
    int status;

    while( (status = read(netsock_fd, buffer, sizeof(buffer)) ) > 0 ){
        printf("%s", buffer);
        memset(buffer, 0, sizeof(buffer));
    }
    printf("-Thanks for using BANK-\n- The Bank has now closed -\n");
    close(netsock_fd);
    free(args);
    return 0;
}

int main(int argc, char *argv[]){

    char buffer[256];
    // sockete that client connects to
    int netsock_fd;
    // dynamic memory for thread
    void *socket_arg1 = malloc(sizeof(netsock_fd));
    void *socket_arg2 = malloc(sizeof(netsock_fd));
    //we need this to communicate w server
    struct addrinfo request;
    request.ai_flags = 0;
    request.ai_family = AF_INET;
    request.ai_socktype = SOCK_STREAM;
    request.ai_protocol = 0;
    request.ai_addrlen = 0;
    request.ai_canonname = NULL;
    request.ai_next = NULL;
    //point to results
    struct addrinfo *result;

    if(argc < 2){
        printf("ERROR: Pick a server noob\n");
        exit(1);
    }

    // check if server host is valid
    if (gethostbyname(argv[1]) == NULL){
        printf("ERROR: Host unavailable lol\n");
        exit(1);
    }

    // retrieve structures
    getaddrinfo(argv[1], "7007", &request, &result );


    // start socket
    netsock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    memcpy(socket_arg1, &netsock_fd, sizeof(int));
    memcpy(socket_arg2, &netsock_fd, sizeof(int));
    // connect to socket
    int status = connect(netsock_fd, result->ai_addr, result->ai_addrlen);
    while( status < 0 ){
        close(netsock_fd);
        netsock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if(errno != ECONNREFUSED){
              printf("ERROR: %s\n", strerror(errno));
              exit(1);
        }
        printf("Server not found. Reconnecting...\n");
        sleep(3);
        status = connect(netsock_fd, result->ai_addr, result->ai_addrlen);
    }

    nsock_fd = netsock_fd;
    struct sigaction	action;
    action.sa_flags = 0;
	  action.sa_handler = sigint_handler;	/* short form */
	  sigemptyset( &action.sa_mask );		/* no additional signals blocked */
	  sigaction( SIGINT, &action, 0 );

    printf("HI WELCOME TO CHILES\n");



// Make thread to communicate between server and client
    pthread_t server_user;

    if (pthread_create(&server_user, NULL, &read_server_user, socket_arg2 ) != 0){
         printf("Can't establish connection: %s\n", strerror(errno));
         exit(1);
    }

    pthread_detach(server_user);

    memset(buffer, 0, sizeof(buffer));

    while( read(0, buffer, sizeof(buffer)) > 0) {
        if ( (status = write(netsock_fd, buffer, strlen(buffer) ) ) < 0){
               printf("Session has ended:/\n");
               break;
        }
        //clear buffer
        memset(buffer, 0, sizeof(buffer));
        sleep(2);
    }

    return 0;

}
