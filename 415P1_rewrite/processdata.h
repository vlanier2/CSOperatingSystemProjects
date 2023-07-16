/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 */
#ifndef _PROCESSDATA_H_
#define _PROCESSDATA_H_

#include <unistd.h>
#include <stdbool.h>

// forward typedefs
typedef struct p_node p_node;
typedef struct process_data process_data;

/*
 * struct to represent our simple PCB
 *  
 * contains PID, flag for sigusr1 recieved
 * flag for if process is alive
 */
struct process_data{
    pid_t pid;
    bool sigusr1_rcvd;
    bool alive;
};

/*
 * simple PCB node for a quick single 
 * use linked list.
 * 
 * I thought this made things a little cleaner
 */
struct p_node {
    process_data *p_data;
    p_node *next;
};

/* 
 * send signal 'sig' to process 'pid'
 * 
 * prints a short error and returns 0 if signal failed to send
 * returns 1 if signal sent successfully
 */
int send_signal(int pid, int sig);

/*
 * returns a heap allocated PCB set to default values
 *
 * pid = pid / alive = true / sigusr1_rcvd = false
 * returns null on malloc failure
 */
process_data *new_process_data(pid_t pid);

/*
 * sends correct signal to start process 
 * either sigusr1 or sig_cont depending on PCB state
 */
void start_process(process_data *p_data);

/*
 * sends sigstop signal to process 
 */
void stop_process(process_data *p_data);

/*
 * malloc and append a new PCB node to a p_node llist 
 * 
 * with head ptr and tail ptr
 */
void append_data(p_node **head, p_node **tail, process_data *data);

/*
 * Remove the node containing the pid given.
 * Set the process data flag 'alive' to false.
 * free the p_node.
 * 
 * the p_node is identified by its PID. 
 */
void kill_pid(p_node **head, p_node **tail, pid_t pid);

/*
 * free any remaining nodes in the p_node llist with 
 * head ptr 'head'.
 */
void destroy_list(p_node **head);

#endif
