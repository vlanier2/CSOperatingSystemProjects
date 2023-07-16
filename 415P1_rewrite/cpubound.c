#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#define UNUSED __attribute__((unused))

volatile int ALRM_seen = 0;

static void onalrm(UNUSED int sig) {
   ALRM_seen++;
}

int main(int argc, char **argv) {
   long long i;
   int minutes = 1;
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
 * establish ARLM signal handler
 */
   if (signal(SIGALRM, onalrm) == SIG_ERR) {
      fprintf(stderr, "Can't establish SIGALRM handler\n");
      exit(EXIT_FAILURE);
   }
/*
 * set timer into the future
 */
   (void)alarm((unsigned int) 60 * minutes);
   while (! ALRM_seen) {
      for (i = 0; i < 300000000; i++) {
         ;
      }
   }
   return EXIT_SUCCESS;
}
