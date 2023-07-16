/*
 *   AUTHORSHIP STATEMENT
 *   VINCENT LANIER
 *   VLANIER
 *   CIS 415 Project 1
 *   This is my own work.
 */

#include "procinfo.h"
#include "p1fxns.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

void pad_right(char *buf, int packsize) {
    int i = 0;
    for (; buf[i] != '\0'; ++i);
    if (i >= packsize) return;
    for (; i < packsize; ++i) {
        buf[i] = ' ';
    }
    buf[i] = '\0';
}

void print_proc_header(int ticks, int ms_past, int proc_count) {
    p1putstr(1, "\033[7m");  // enable inverted colors
    p1putstr(1, "..,.,,.xXX* U5P5V4 B1G 8R0TH3R *XXx.,,.,..\n");
    p1putstr(1, "\033[0m");  // disable inverted colors
    
    p1putstr(1, "TICK "); p1putint(1, ticks); p1putstr(1, " | ");
    p1putstr(1, "SECONDS "); p1putint(1, ms_past / 1000); p1putstr(1, " | ");
    p1putstr(1, "PROCESS COUNT "); p1putint(1, proc_count); p1putstr(1, " | ");
    p1putchr(1, '\n');
    p1putchr(1, '\n');
    p1putstr(1, "CMDLINE | PID | CPU SEC (USR / KER) | MEMORY KBs | I/O CALLS (READ / WRITE) \n");
    p1putchr(1, '\n'); 
}

void print_proc_footer(char *complete, int cols) {
    p1putchr(1, '\n');
    p1putstr(1, "TERMINATED : ");
    for(int i = 0; complete[i] != '\0' && i < cols; ++i) {
        p1putchr(1, complete[i]);
    }
    p1strcpy(complete, "");
}

void print_proc_cmdline(char *buf) {

    p1strcat(buf, "/cmdline");

    int fd = open(buf, O_RDONLY);
    if (fd == -1) return;
    int cc = read(fd, buf, 256);
    int i;
    for (i = 0; i < cc; i++) {
        if (buf[i] == '\0') {
            buf[i] = ' ';
        }
    }
    buf[i] = '\0';
    close(fd);

    pad_right(buf, 25);
    p1putstr(1, buf); p1putstr(1, " | ");
}

void print_proc_io(char *buf) {
    p1strcat(buf, "/io");

    int fd = open(buf, O_RDONLY);
    if (fd == -1) return;
    char wordbuf[30];

    p1getline(fd, buf, 256);
    p1getline(fd, buf, 256);
    p1getline(fd, buf, 256);

    p1getword(buf, 6, wordbuf);

    long int reads = p1atoi(wordbuf);

    p1putint(1, reads);
    p1putstr(1, " / ");

    p1getline(fd, buf, 256);

    p1getword(buf, 6, wordbuf);

    long int writes = p1atoi(wordbuf);

    p1putint(1, writes);
    p1putstr(1, " | ");

    close(fd);
}

void print_proc_status(char *buf) {

    p1strcat(buf, "/status");

    int fd = open(buf, O_RDONLY);
    if (fd == -1) return;

    int i = 0;
    for (; i < 22; ++i){
        p1getline(fd, buf, 256);
    }

    char wordbuf[30];
    p1getword(buf, 6, wordbuf);

    p1putstr(1, wordbuf);
    p1putstr(1, " | ");

    close(fd);
}

void print_proc_stats(char *buf) {

    int index = 0;
    p1strcat(buf, "/stat");

    int fd = open(buf, O_RDONLY);
    if (fd == -1) return;

    p1getline(fd, buf, 256);

    char wordbuf[30];
    for (int i = 0; i < 14; ++i) {
        index = p1getword(buf, index, wordbuf);
    }

    long hz = sysconf(_SC_CLK_TCK);

    int utime = p1atoi(wordbuf) / hz;
    index = p1getword(buf, index, wordbuf);
    int stime = p1atoi(wordbuf) / hz;    
 
    p1putint(1, utime); p1putstr(1, " / "); p1putint(1, stime);
    p1putstr(1, " | ");

    // for (int i = 0; i < 8; ++i) {
    //     index = p1getword(buf, index, wordbuf);
    // }

    // int vsz = p1atoi(wordbuf);
    // p1putint(1, vsz); p1putstr(1, " | ");
    close(fd);
}


int print_proc_line(int pid, char *buf, char *pidbuf) {
    char procpath[56];
    p1strcpy(buf, "/proc/");
    p1itoa(pid, pidbuf);
    p1strcat(buf, pidbuf);
    p1strcpy(procpath, buf);
    
    if (access(procpath, F_OK) != 0) {return 0;}

    print_proc_cmdline(buf);
    
    p1putstr(1, pidbuf); p1putstr(1, " | ");
    
    p1strcpy(buf, procpath);
    print_proc_stats(buf);
    p1strcpy(buf, procpath);
    print_proc_status(buf);
    p1strcpy(buf, procpath);
    print_proc_io(buf);
    p1putstr(1, "\n");

    return 1;
}

int print_all_pid(int *pid, char* complete, char* buf, char* pidbuf, int rows) {
    int i = 0;
    int can_print = rows;
    for (; pid[i] != -1 && can_print; ++i) {
        if(!print_proc_line(pid[i], buf, pidbuf)) {
            p1strcat(complete, " ");
            p1itoa(pid[i], pidbuf);
            p1strcat(complete, pidbuf);
        }
        else can_print--;
    }
    return i;
}

void print_cur_process(process_data *current, char* buf, char* pidbuf) {
    if (current->alive) {
        print_proc_line(current->pid, buf, pidbuf);
    }
}

void print_proc_info(int *pid, int ticks, int ms_past, process_data *current, int proc_count) {
    
    // get window size
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int rows = w.ws_row - 13; //exclude header and footer 
    // and few more lines for misc
    int cols = w.ws_col - 13;

    char complete[256] = "\0";
    char buf[256] = "\0";
    char pidbuf[12] = "\0";

    p1putstr(1, "\033[2J\033[0;0H");
    print_proc_header(ticks, ms_past, proc_count);
    p1putstr(1, "CURRENT : \n");
    print_cur_process(current, buf, pidbuf);
    p1putstr(1, "\nALL :\n");
    print_all_pid(pid, complete, buf, pidbuf, rows);
    print_proc_footer(complete, cols);
    p1putstr(1, "\nOUT : ");
}