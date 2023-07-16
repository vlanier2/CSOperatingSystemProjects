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

static volatile int proc_count = 0;

int main(int argc, char *argv[]) {

    int fd, quantum_ms, status, chars, pid[MAXPROGRAMS];
    char *arguments[MAXARGS];
    int exit_val = EXIT_FAILURE;
    char linebuf[BUFFERSIZE];

    if (!setup_usps(argc, argv, &quantum_ms, &fd)) goto CLEANUP;

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