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

// go through string to see if all digits
int isnum(char * amount){
    int counter = 0;
    int len = strlen(amount);
    int i;
    for(i = 0 ; i < len ; i++){
        if(isdigit(amount[i]) == 0){
            if(amount[i] == '.'){
                if(counter++ > 1){
                    return 0;
                }
            }else{
                return 0;
            }
        }
    }
    return 1;
}

action_command find_action(char * command){
    int i;
    for(i = 0; i < strlen(command); i++){
        command[i] = tolower(command[i]);
    }

    if(strcmp(command,"open") == 0){
        return acc_open;
    }else if(strcmp(command,"start") == 0){
        return acc_start;
    }else if(strcmp(command,"deposit") == 0){
        return acc_deposit;
    }else if(strcmp(command,"withdraw") == 0){
        return acc_withdraw;
    }else if(strcmp(command,"balance") == 0){
        return acc_balance;
    }else if(strcmp(command,"finish") == 0){
        return acc_finish;
    }else if(strcmp(command,"exit") == 0){
        return acc_exit;
    }else{
      return acc_notexit;
    }

    return acc_notexit;
}


void start_account(account ***arr_account){
    *arr_account = (account ** ) malloc(sizeof(account *)*20); // malloc bs
    int i;
    for(i = 0 ; i < 20 ; i++){
        (*arr_account)[i] = (account *) malloc(sizeof(account));
        ((*arr_account)[i])->service_flag = -1;
        pthread_mutex_init(&(((*arr_account)[i])->lock), NULL);
    }
    return;
}

// check if account exists
int search_account(account ** arr_account, char * name){
    int i;
    for(i = 0 ; i < 20 ; i++){
        if(strcmp( (arr_account[i])->name ,name) == 0){
          return i;
        }
    }
    return -1;
}

// need to add a mutex lock for open()
void open_acc(int client_socket_fd, int *account_num, account ***arr_account, char * name, int * session, pthread_mutex_t * openLock){
    // lock the openLock
    pthread_mutex_lock(openLock);
    // init buffer
    char buff_2[256];
    memset(buff_2, 0, sizeof(buff_2));
    // return name of account
    if(strlen(name) <= 0){
      if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Must specify name of account\n") ) < 0 ){
          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
          pthread_mutex_unlock(openLock);
          exit(1);
      }
      pthread_mutex_unlock(openLock);
      return;
    }
    // account is still open, cant make new account
    if(*session >= 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Account already open idiot, can't open again: %s\n",name) ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            pthread_mutex_unlock(openLock);
            exit(1);
        }
        pthread_mutex_unlock(openLock);
        return;
    }
    // account limit reached
    if(*account_num >= 20){
      if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Account capacity reached. Can't make more: %s\n",name) ) < 0 ){
          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
          pthread_mutex_unlock(openLock);
          exit(1);
      }
      pthread_mutex_unlock(openLock);
      return;
    }
    // account alredy exists
    if(search_account(*arr_account,name) >= 0 ){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Account name taken lol: %s\n",name) ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            pthread_mutex_unlock(openLock);
            exit(1);
        }
        pthread_mutex_unlock(openLock);
        return;
    }
    // create account
    account * client_account = (*arr_account)[*account_num];

    strcpy(client_account->name, name);   // init account name (CASE sensitive)
    client_account->balance = 0.0;       // 0 balance initialize
    client_account->service_flag = 0;    // not in session

    if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Account made!: %s\n",name) ) < 0 ){
        printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
        pthread_mutex_unlock(openLock);
        exit(1);
    }

    // update the number of existing accounts
    *account_num = *account_num + 1;
    // release lock for other clients to use
    pthread_mutex_unlock(openLock);
    return;

}

void start_acc(int client_socket_fd,  account ***arr_account, char * name, int *session){
    char buff_2[256];
    memset(buff_2, 0, sizeof(buff_2));
    // check for input for name
    if(strlen(name) <= 0){
      if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Must specify name of account\n") ) < 0 ){
          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
          exit(1);
      }
      return;
    }
    // prevent making more than none session
    if(*session >= 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Session in progress, please close session: %s\n",name) ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    // account does not exist
    if( (*session = search_account(*arr_account,name)) < 0 ){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Account does not exist lol: %s\n",name) ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    // identify account
    account * client_account = (*arr_account)[*session];

    // if account, exists lock account session
    // if account is locked already, current client will need to wait
    if( pthread_mutex_trylock(&(client_account->lock)) != 0 ){   //pthread_mutex_lock(&(client_account->lock));
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Account already active yo %s\n",name) ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        *session = -1;    // session was not started
          return;
    }

    // in session
    client_account->service_flag = 1;


    if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Account sessino started: %s\n",name) ) < 0 ){
        printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
        exit(1);
    }

    return;

}

void deposit_acc(int client_socket_fd, account ***arr_account, char * amount, int *session){
    char buff_2[256];
    memset(buff_2, 0, sizeof(buff_2));

    // check for name
    if(strlen(amount) <= 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Specify amount to deposit.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }
    // if not float
    if(!isnum(amount)){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Can't deposit a negative number wtf.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    // check if session exists
    if(*session < 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Session does not exist.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    float depositval = atof(amount);

    account * client_account = (*arr_account)[*session];
    client_account->balance += depositval;

    if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Deposit successful\n") ) < 0 ){
        printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
        exit(1);
    }

    return;

}

void withdraw_acc(int client_socket_fd, account ***arr_account, char * amount, int *session){
    char buff_2[256];
    memset(buff_2, 0, sizeof(buff_2));

    // check if amount is given
    if(strlen(amount) <= 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Specify amount to withdraw.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }
    // if not float
    if(!isnum(amount)){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Can't withdraw a negative number wtf.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    // check if session exists
    if(*session < 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Session does not exist.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    float withdrawval = atof(amount);

    account * client_account = (*arr_account)[*session];
    // check that withdraw <= balance
    if(withdrawval > client_account->balance){
      if( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Can't withdraw more than whats in your account you broke bastard.\n") ) < 0 ){
          printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
          exit(1);
      }
      return;
    }

    client_account->balance -= withdrawval;

    if( write(client_socket_fd, buff_2, sprintf(buff_2, "Your account has been withdrawed.\n") ) < 0 ){
        printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
        exit(1);
    }

    return;

}

void balance_acc(int client_socket_fd,  account ***arr_account, int *session){
    char buff_2[256];
    memset(buff_2, 0, sizeof(buff_2));

    // check if session exists
    if(*session < 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: Session does not exist.\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    // get balance
    float balance = (*arr_account)[*session]->balance;

    if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Balance: %f\n",balance) ) < 0 ){
        printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
        exit(1);
    }


    return;


}

void finish_acc(int client_socket_fd, account ***arr_account,  int *session){
    char buff_2[256];
    memset(buff_2, 0, sizeof(buff_2));
    // check if session exists
    if(*session < 0){
        if ( write(client_socket_fd, buff_2, sprintf(buff_2, "ERROR: SESSION DOES NOT EXIST\n") ) < 0 ){
            printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
            exit(1);
        }
        return;
    }

    account * client_account = (*arr_account)[*session];
    char * name = client_account->name;

    if ( write(client_socket_fd, buff_2, sprintf(buff_2, "Account session finished: %s\n",name) ) < 0 ){
        printf("ERROR: WRITE FAILED: %s\n", strerror(errno));
        exit(1);
    }

    // change service flag
    client_account->service_flag = 0;
    // change session flag
    *session = -1;

    pthread_mutex_unlock(&(client_account->lock));

    return;

}
