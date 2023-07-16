#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define UNUSED __attribute__((unused))

volatile int ALRM_seen = 0;

static void onalrm(UNUSED int sig) {
   ALRM_seen++;
}

int main(int argc, char **argv) {
   int j, minutes = 1, number = 6000000;
   char b[1000];
   int fd = open("/dev/null", O_WRONLY);
   char name[128];
   int opt;

/*
 * process environment variable and command line arguments
 */
   sprintf(name, "%x", getpid());
   opterr = 0;
   while ((opt = getopt(argc, argv, "m:n:")) != -1) {
       switch(opt) {
       case 'm': minutes = atoi(optarg); break;
       case 'n': strcpy(name, optarg); break;
       default: fprintf(stderr, "illegal option: -%c\n", optopt);
                fprintf(stderr, "usage: %s [-m <minutes>] [-n <name>]\n",
				argv[0]);
		return EXIT_FAILURE;
       }
   }
/*
 * establish SIGALRM handler
 */
   if (signal(SIGALRM, onalrm) == SIG_ERR) {
      fprintf(stderr, "Unable to establish SIGALRM handler\n");
      exit(1);
   }
/*
 * set alarm in the future
 */
   (void) alarm((unsigned int) 60 * minutes);
   while (! ALRM_seen) {
      for (j = 0; j < number; j++)
          (void) write(fd, b, sizeof(b));
   }
   return EXIT_SUCCESS;
}
