/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 * 
 */

#ifndef _PROCINFO_H_
#define _PROCINFO_H_

#define REFRESH_EVERY 25 // ticks

#include "processdata.h"

void print_proc_header(int ticks, int ms_past, int proc_count);
void print_proc_footer(char *complete, int cols);
void print_proc_cmdline(char *buf);
int print_proc_line(int pid, char *buf, char *pidbuf);
int print_all_pid(int *pid, char* complete, char* buf, char *pidbuf, int rows);
void print_proc_info(int *pid, int ticks, int ms_past, process_data *current, int proc_count);


#endif