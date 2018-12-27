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

 #include "bank.h"

 account **arr_account;
 int account_num;
 pthread_mutex_t openLock;


 //this is to free data
 void * clientreader(void * arg){
      printf("Server accepted connection .\n");
      int client_socket_fd = *(int *) arg;
      // state buffers
      char command[10];
      char name[100];
      char buff_1[256];
      char buff_2[256];
      // clear
      memset(buff_1, 0, sizeof(buff_1));
      memset(buff_2, 0, sizeof(buff_2));
      memset(command, 0, sizeof(command));
      memset(name, 0, sizeof(name));
      action_command ac;
      // session state < 0 if not in session,
      // session state >=0 if in session (specifying location of account)
      int session = -1;
      // tell em what to do haha
      char c_options[100] = "COMMANDS:\n\topen\n\tstart\n\tdeposit\n\twithdraw\n\tbalance\n\tfinish\n\texit";
      if ( write(client_socket_fd, buff_2, sprintf(buff_2, "%s\n",c_options) ) < 0 ){
          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
          exit(1);
        }
      memset(buff_2, 0, sizeof(buff_2));
      // read and do the thing
      while( read(client_socket_fd, buff_1, 255) > 0){
          sscanf(buff_1, "%s %s",command, name);

          ac = find_action(command);

          switch(ac){
              case acc_open:
                  open_acc(client_socket_fd,&account_num,&arr_account,name,&session,&openLock);
                  break;
              case acc_start:
                  start_acc(client_socket_fd,&arr_account,name,&session);
                  break;
              case acc_deposit:
                  deposit_acc(client_socket_fd,&arr_account,name,&session);
                  break;
              case acc_withdraw:
                  withdraw_acc(client_socket_fd,&arr_account,name,&session);
                  break;
              case acc_balance:
                  balance_acc(client_socket_fd,&arr_account,&session);
                  break;
              case acc_finish:
                  finish_acc(client_socket_fd,&arr_account,&session);
                  break;
              case acc_exit:     // exit session
                  if(session < 0){
                      if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Session has finished\n") ) < 0 ){
                          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
                          exit(1);
                        }
                      printf("Server has closed a client connection\n");
                      close(client_socket_fd);
                      free(arg);
                      return 0;
                    }
                    arr_account[session]->service_flag = 0;
                    pthread_mutex_unlock(&(arr_account[session]->lock));
                    session = -1;
                      if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Session has finished\n") ) < 0 ){
                          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
                          exit(1);
                        }
                    printf("Server has ended connection with client\n");
                    close(client_socket_fd);
                    free(arg);
                  return 0;
              default:
                  if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Invalid argument dummy, use the listed commands: %s\n",command) ) < 0 ){
                      printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
                      exit(1);
                  }
                  break;

          }
          if ( write(client_socket_fd, buff_2, sprintf(buff_2, "%s\n",c_options) ) < 0 ){
              printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
              exit(1);
            }
          // clear buffers
          memset(buff_1, 0, sizeof(buff_1));
          memset(buff_2, 0, sizeof(buff_2));
          memset(command, 0, sizeof(command));
          memset(name, 0, sizeof(name));
      }

      free(arg);
      return 0;
 }

 void *print20(void * args){

      while(1){
          printf("BANK STATS: \n");
          if(account_num == 0){
              printf("\tNo Account have been opened yet\n");
          }else{
              int i;
              for(i = 0 ; i < 20 ; i++){
                  account * ca = arr_account[i];
                  if(ca->service_flag >= 0){
                      if(ca->service_flag == 0){
                          printf("Account name: %s, Balance: %f, Open?: %s\n",ca->name,ca->balance,"NO");
                      }else{
                          printf("Account name: %s, Balance: %f, Open?: %s\n",ca->name,ca->balance,"YES");
                      }
                  }
              }
          }
          sleep(20);
      }
 }

 int main() {
    // server socket file descriptor
    int server_socket_fd;
    // client socket file descriptor (that server receives)
    int client_socket_fd;
    // request structs we need for socket communication
    struct addrinfo request;
    request.ai_flags = AI_PASSIVE;
    request.ai_family = AF_INET;
    request.ai_socktype = SOCK_STREAM;
    request.ai_protocol = 0;
    request.ai_addrlen = 0;
    request.ai_canonname = NULL;
    request.ai_next = NULL;
    // will point to results
    struct addrinfo *result;

    getaddrinfo(0, "7007", &request, &result );

    // create socket
    if ( (server_socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol) ) < 0 ){
        printf("ERROR: SERVER COULD NOT BE CREATED: %s\n", strerror(errno));
        exit(1);
    }

    // create connection
    if( bind(server_socket_fd, result->ai_addr, result->ai_addrlen) < 0){
        printf("ERROR: SERVER COULD NOT BE CREATED: %s\n", strerror(errno));
        exit(1);
    }
    int optval = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR , &optval, sizeof(int));

    // to keep track of accounts
    account_num = 0;
    start_account(&arr_account);
    pthread_mutex_init(&openLock, NULL);

    // print stats every 20 secs

    pthread_t get_account_info;
    pthread_create(&get_account_info,NULL,&print20,NULL);

    listen(server_socket_fd,5); // allow for 5 connections to be q'd
    void * sock_arg;   // socket argument for every client thread created
    pthread_t client;
    while(1){

        if( (client_socket_fd = accept(server_socket_fd, NULL, NULL)) < 0){
            printf("ERROR: FAILED TO ACCEPT: %s\n", strerror(errno));
            exit(1);
        }

        sock_arg = malloc(sizeof(int));
        memcpy(sock_arg, &client_socket_fd, sizeof(int));

        // create thread for client - service
        if (pthread_create(&client, NULL, &clientreader, sock_arg ) != 0){
            printf("ERROR: Can't create user server thread: %s\n", strerror(errno));
            exit(1);
        }

        if (pthread_detach(client) != 0){
            printf("ERROR: Could not detach client thread: %s\n", strerror(errno));
            exit(1);
        }



    }
 }
