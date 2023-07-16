/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 * 
 */

#include "uspsutils.h"
#include "p1fxns.h"

#include <unistd.h> // opts
#include <stdlib.h> // malloc
#include <fcntl.h> // open

/*
 * allocates an array of char* in arr
 *
 * returns 1 if successful - 0 if failed malloc
 */
int create_str_arr(char ***arr, int xdim) {
    char **tmp = (char **)malloc(xdim * sizeof(char *));
    if (!tmp) return 0;
    *arr = tmp;
    return 1;
}

/* 
 * frees memory allocated for
 *
 * array of char*'s. Frees each string until null
 * 
 * terminator is reached
 */
void clear_str_arr(char **arr) {
    int i = 0;
    char *str;
    while((str = arr[i++])) {
        free(str);
    }
}

/* 
 * Allocates workload arguments into arr from given
 * 
 * line buffer. Returns number of arguments, -1 on malloc error
 */
int get_wl_args(char **arr, char *linebuf, int chars) {
    char wordbuf[BUFFERSIZE];
    int arg_count = 0, index = 0;
    linebuf[chars - 1] = '\0';
    while((index = p1getword(linebuf, index, wordbuf)) != -1) {
        arr[arg_count] = p1strdup(wordbuf);
        if(!arr[arg_count++]) return -1;
    }
    arr[arg_count] = NULL;
    return arg_count;
}

/* 
 * Prints usps usage string to stderr
 */
void usage()
{
    char *usage_str = "usage: ./uspsv1 [-q <quantum in msec>] [workload_file]\n";
    p1putstr(2, usage_str);
}

/* 
 * handle arguments and setup for usps scheduler
 * 
 * reads quantum from arguments or environment into quantum_ms
 * reads file descriptor for workload into fd
 * 
 * returns 1 on success 0 on error
 */
int setup_usps(int argc, char *argv[], int *quantum_ms, int *fd) {
    *fd = 0;
    int opt;
    char *env_var;
    bool quantum_found;

        // Look for quantum from environment variables
    if ((env_var = getenv("USPS_QUANTUM_MSEC")) != NULL) {
        *quantum_ms = p1atoi(env_var);
        quantum_found = true;
    }

        // Check for q flag to overwite quantum
    while ((opt = getopt(argc, argv, "q:")) != -1) {
        switch (opt) {
        case 'q':
            *quantum_ms = p1atoi(optarg);
            quantum_found = true;
            break;
        default:
            usage();
            return 0;
        }
    }

        // Ensure valid quantum is found
    if (!quantum_found)
    {
        p1putstr(2, "Error: No quantum variable found\n");
        return 0;
    }

    if (*quantum_ms > MAXQUANTUM) {
        p1putstr(2, "Error: quantum maximum is 1000ms.\n");
        return 0;
    }

    if (*quantum_ms < MINQUANTUM) {
        p1putstr(2, "Error: quantum minimum is 20ms .\n");
        return 0;
    }

        // Check for workload file - exit if multiple
    if (argc == optind) p1putstr(1, "read from stdin\n");

    else if (argc == optind + 1) {
        *fd = open(argv[optind], O_RDONLY);
        if (*fd == -1) {
            p1putstr(2, "Error: Could not open workload file.\n");
            return 0;
        }
    }

    else {
        usage();
        return 0;
    }

    return 1;
}