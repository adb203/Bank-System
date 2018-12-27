#ifndef BANK_H
#define BANK_H

#include <pthread.h>
#include <ctype.h>

    typedef enum action_command_ {
        acc_open,
        acc_start,
        acc_deposit,
        acc_withdraw,
        acc_balance,
        acc_finish,
        acc_exit,
        acc_notexit,
    } action_command;

    typedef struct account_ {
        char name[100];
        float balance;
        int service_flag;
        // -1: Not created, 1: In service,  0: Not in service
        pthread_mutex_t lock;
    } account;

    int isnum(char * amount);

    action_command find_action(char * command);

    void start_account(account ***arr_account);

    // check if account existss
    int search_account(account ** arr_account, char * name);

    void open_acc(int client_socket_fd, int *account_num, account ***arr_account, char * name, int *session, pthread_mutex_t * openLock);

    void start_acc(int client_socket_fd,  account ***arr_account, char * name, int *session);

    void deposit_acc(int client_socket_fd, account ***arr_account, char * amount, int *session);

    void withdraw_acc(int client_socket_fd, account ***arr_account, char * amount, int *session);

    void balance_acc(int client_socket_fd,  account ***arr_account, int *session);

    void finish_acc(int client_socket_fd, account ***arr_account,  int *session);

    //void accExit(int client_socket_fd, int *session);
#endif
