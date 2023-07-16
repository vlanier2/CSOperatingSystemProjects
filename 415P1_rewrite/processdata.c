/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 */

#include "processdata.h"
#include "p1fxns.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>

//
// #include <stdio.h>
//

/* 
 * send signal 'sig' to process 'pid'
 * 
 * prints a short error and returns 0 if signal failed to send
 * returns 1 if signal sent successfully
 */
int send_signal(int pid, int sig) {
    if(kill(pid, sig) == -1) {
            p1perror(2, "failed to send signal");
            return 0;
        }
    return 1;
}

/*
 * returns a heap allocated PCB set to default values
 *
 * pid = pid / alive = true / sigusr1_rcvd = false
 * returns null on malloc failure
 */
process_data *new_process_data(pid_t pid) {
    process_data *tmp = (process_data *)malloc(sizeof(process_data));
    if (tmp) {
        tmp->pid = pid;
        tmp->alive = true;
        tmp->sigusr1_rcvd = false;
    } 
    return tmp;
}

/*
 * sends correct signal to start process 
 * either sigusr1 or sig_cont depending on PCB state
 */
void start_process(process_data *p_data) {
    // printf("process started %i\n", p_data->pid); //
    // printf("info su1r %d alve %d\n", p_data->sigusr1_rcvd, p_data->alive);
    if (p_data->sigusr1_rcvd) {
        send_signal(p_data->pid, SIGCONT);
    }
    else {
        // printf("sent sigusr1 \n");
        p_data->sigusr1_rcvd = true;
        send_signal(p_data->pid, SIGUSR1);
    }
}

/*
 * sends sigstop signal to process 
 */
void stop_process(process_data *p_data) {
    // printf("process stopped %i\n", p_data->pid); //
    send_signal(p_data->pid, SIGSTOP);
}

/*
 * malloc and append a new PCB node to a p_node llist 
 * 
 * with head ptr and tail ptr
 */
void append_data(p_node **head, p_node **tail, process_data *data) {
    p_node *node = (p_node *)malloc(sizeof(p_node));
    node->p_data = data;
    node->next = NULL;
    if (!(*head)) {
        *head = node;
        *tail = node;
    } else {
        (*tail)->next = node;
        (*tail) = node;
    }
}

/*
 * Remove the node containing the pid given.
 * Set the process data flag 'alive' to false.
 * free the p_node.
 * 
 * the p_node is identified by its PID. 
 */
void kill_pid(p_node **head, p_node **tail, pid_t pid) {

    // printf("CALLING KILL INSIDE\n");

    p_node *prev = NULL;
    p_node *current = *head;
    while(current->p_data->pid != pid) {
        prev = current;
        current = current->next;
    }
    // printf("****** %p set to false \n", current->p_data);
    current->p_data->alive = false;

    if (current == *head) {
        *head = (*head)->next;
    } else {
        if (current == *tail)
            *tail = prev;
        prev->next = current->next;
    }

    free(current);
}

/*
 * free any remaining nodes in the p_node llist with 
 * head ptr 'head'.
 */
void destroy_list(p_node **head) {
    p_node *prev = NULL;
    p_node *current = *head;
    while(current) {
        prev = current;
        current = current->next;
        free(prev);
    }
}
