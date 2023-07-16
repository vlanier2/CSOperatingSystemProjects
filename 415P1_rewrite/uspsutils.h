/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 */

#ifndef _USPSUTILS_H_
#define _USPSUTILS_H_

#define BUFFERSIZE 8192
#define MAXPROGRAMS 256
#define MAXARGS 128
#define MAXARGLEN 128
#define MAXQUANTUM 1000
#define MINQUANTUM 20
#define TICK_MS 20

/*
 * allocates an array of char* in arr
 *
 * returns 1 if successful - 0 if failed malloc
 */
int create_str_arr(char ***arr, int xdim);

/* 
 * frees memory allocated for
 *
 * array of char*'s. Frees each string until null
 * 
 * terminator is reached
 */
void clear_str_arr(char **arr);

/* 
 * Allocates new array of char** at arr. returns -1 on malloc error.
 *
 * Reads workload arguments into arr from given
 * 
 * line buffer. Returns number of arguments
 */
int get_wl_args(char **arr, char *linebuf, int chars);

/* 
 * Prints usps usage string to stderr
 */
void usage();

/* 
 * handle arguments and setup for usps scheduler
 * 
 * reads quantum from arguments or environment into quantum_ms
 * reads file descriptor for workload into fd
 * 
 * returns 1 on success 0 on error
 */
int setup_usps(int argc, char *argv[], int *quantum_ms, int *fd);

#endif