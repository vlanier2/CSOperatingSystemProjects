/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 */

#include "uspsutils.h"
#include "p1fxns.h"
#include <stdlib.h>
#include <unistd.h> // for close
#include <sys/wait.h> // for wait
#include <signal.h> // for signal
#include <time.h> // for nanosleep

#define UNUSED __attribute__((unused))

static volatile int proc_count = 0;
volatile bool sigusr1_rcvd = false;

void signal_handler(UNUSED int sig) {
    sigusr1_rcvd = true;
}

int signal_all(pid_t *pid, int proc_count, int sig) {
    for (int i = 0; i < proc_count; ++i) {
        if(kill(pid[i], sig) == -1) {
            p1putstr(2, "Attempted signal ");
            p1putint(2, sig);
            p1putstr(2, "on pid ");
            p1putint(2, pid[i]); p1putchr(2, '\n');
            p1perror(2, "failed to send signal");
        }
        // sleep(1);
        // for (int i = 0; i < 9999; i++) {
        //     for (int j = 0; j < 9999; j++) {}
        // }
    }
    return 1;
}

int main(int argc, char *argv[]) {

    int fd, quantum_ms, status, chars, pid[MAXPROGRAMS];
    char *arguments[MAXARGS];
    int exit_val = EXIT_FAILURE;
    char linebuf[BUFFERSIZE];
    struct timespec ms20 = {0, 20000000};

    if (!setup_usps(argc, argv, &quantum_ms, &fd)) goto CLEANUP;

    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        p1perror(2, "Error: could not establish signal handler");
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
                    // p1putstr(1, "Process Fired. PID: ");
                    // p1putint(1, pid[proc_count]); p1putchr(1, '\n');
                    clear_str_arr(arguments);
                    ++proc_count;
            }
        }
    }

    signal_all(pid, proc_count, SIGUSR1);
    signal_all(pid, proc_count, SIGSTOP);
    signal_all(pid, proc_count, SIGCONT);

    for (int i = 0; i < proc_count; ++i) {
        (void)wait(&status);
        // if (WEXITSTATUS(status) == EXIT_SUCCESS) success++;
    }

    // printf("%d OF %d EXITED SUCCESSFULLY\n", success, proc_count);

    exit_val = EXIT_SUCCESS;
    CLEANUP:
    if (fd) close(fd);
    return exit_val;
}   