/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 */

#include "uspsutils.h"
#include "p1fxns.h"
#include "ADTs/llistqueue.h"
#include "processdata.h"
#include <stdlib.h>
#include <unistd.h> // for close
#include <sys/wait.h> // for wait
#include <sys/time.h>// for itimer
#include <signal.h> // for signal
#include <time.h> // for nanosleep

//
// #include <stdio.h>
//

#define UNUSED __attribute__((unused))

static volatile int ticks, proc_count = 0;
volatile bool sigusr1_rcvd = false;
int quantum_ms;
static process_data *current_process = NULL;

// Process Wait/Ready queue
const Queue *ready_queue;

//llist for alive pcb data
static p_node *head = NULL, *tail = NULL;

// flip sigusr1 flag for forked process waiting
void sigusr1_handler(UNUSED int sig) {
    sigusr1_rcvd = true;
}

// just to wake main process
static void sigusr2_handler(UNUSED int sig) {}

// on each tick runs 
// preempt running process on quantum expiration
// harvest terminated processes on tick
static void sigalrm_handler(UNUSED int sig) {
    
    if (ticks > 0) {
        --ticks;
        // printf("tickdown\n");
        // printf("tick ");
        // printf("***CURRENT %d \n", current_process);

        if (current_process->alive) return;
        free(current_process);
        current_process = NULL;
    }
    else {
        ticks = quantum_ms / TICK_MS; // reset ticks
        if (current_process->alive) stop_process(current_process);
        ready_queue->enqueue(ready_queue, (void *)current_process);
    }
    // do { // this loop is not needed since i am harvesting each tick it does not improve 
            // total performance. also it leaks memory since i forgot to requeue
    //     ready_queue->dequeue(ready_queue, (void **)&current_process);
    // } while (!current_process->alive);
    // start_process(current_process);
    ready_queue->dequeue(ready_queue, (void **)&current_process);
    if (current_process->alive) start_process(current_process);
}

// on child termination decrement process count
// set process data alive flag to false
// signal parent to wake
static void sigchld_handler(UNUSED int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            kill_pid(&head, &tail, pid);
            --proc_count;
            // printf("pid in sigchild handler died %i\n", pid);
            send_signal(getpid(), SIGUSR2);
        }
    }
}

// helper function to set and start real interval timer
// based on tick length
int start_itimer(struct itimerval *timer) {
    timer->it_value.tv_sec = TICK_MS/1000;
    timer->it_value.tv_usec = (TICK_MS * 1000) % 1000000;
    timer->it_interval = timer->it_value;
    if(setitimer(ITIMER_REAL, timer, NULL) == -1) return 0;
    return 1;
}

int main(int argc, char *argv[]) {

    int fd, chars, pid[MAXPROGRAMS];
    char *arguments[MAXARGS];
    int exit_val = EXIT_FAILURE;
    char linebuf[BUFFERSIZE];
    struct timespec ms20 = {0, 20000000};
    struct itimerval itimer;
    process_data *p_data;

    if (!(ready_queue = Queue_create(free))) {
        p1perror(2, "Error: (malloc) could not create Queue");
        goto CLEANUP;
    }

    if (!setup_usps(argc, argv, &quantum_ms, &fd)) goto CLEANUP;

    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
        p1perror(2, "Error: could not establish sigusr1 handler");
        goto CLEANUP;
    }
    
    if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
        p1perror(2, "Error: could not establish sigusr2 handler");
        goto CLEANUP;
    }

    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        p1perror(2, "Error: could not establish sigchld handler");
        goto CLEANUP;
    }

    if (signal(SIGALRM, sigalrm_handler) == SIG_ERR) {
        p1perror(2, "Error: could not establish sigalrm handler");
        goto CLEANUP;
    }

    while((chars = p1getline(fd, linebuf, BUFFERSIZE))) {
        if ((get_wl_args(arguments, linebuf, chars)) == -1) {
            p1putstr(2, "Error: malloc failed on args arr");
            goto CLEANUP;
        }
        if (arguments[0] != NULL) { // skip empty line
            pid[proc_count] = fork();
            switch(pid[proc_count]) {
                case -1: // fork failed
                    p1perror(2, "Error: fork() call failed!");
                    goto CLEANUP;
                case 0:
                    while(!sigusr1_rcvd) {(void)nanosleep(&ms20, NULL);}
                    // p1putstr(1, "PID "); p1putint(1, getpid()); p1putstr(1, " RESUMED\n");

                    execvp(arguments[0], arguments);
                    p1putstr(2, arguments[0]);
                    p1perror(2, " Attempted - Error: exec failed!");
                    clear_str_arr(arguments);
                    goto CLEANUP;
                default:
                    // p1putstr(1, "Process Waiting. PID: ");
                    // p1putint(1, pid[proc_count]); p1putchr(1, '\n');
                    clear_str_arr(arguments);

                    // allocate new PCB structure
                    if (!(p_data = new_process_data(pid[proc_count]))) {
                        p1perror(2, "Error: malloc error on process data.");
                        goto CLEANUP;
                    }

                    append_data(&head, &tail, p_data); //append to PCB block
                    ready_queue->enqueue(ready_queue, (void *)p_data); //add to ready queue
                    ++proc_count;
            }
        }
    }

    /* start interval timer and fire first process */
    ticks = quantum_ms / TICK_MS; // start ticks at full
    if(ready_queue->dequeue(ready_queue, (void **)&current_process))
        start_process(current_process);

    if (!start_itimer(&itimer)) {
        p1perror(2, "Error: could not establish itimer");
        goto CLEANUP;
    }

    while (proc_count > 0) pause();

    exit_val = EXIT_SUCCESS;
    CLEANUP:
    if (fd) close(fd);    
    if (head) destroy_list(&head);
    if (current_process) free(current_process);
    if (ready_queue) ready_queue->destroy(ready_queue);
    return exit_val;
}
