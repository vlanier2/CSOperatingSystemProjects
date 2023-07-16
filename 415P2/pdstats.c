#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <regex.h>

#define N_PATTERNS 6

int main (int argc, char *argv[]) {

    int counts[] = {0, 0, 0, 0, 0, 0};
    int match;
    char buffer[BUFSIZ];

    char *patterns[N_PATTERNS] = {
        ".*LOST.*",
        ".*Packet received for PID.*",
        ".*application .* has received a packet.*",
        ".*application .* failed to send a packet descriptor.*",
        ".*application .* has sent a packet descriptor.*",
        ".*packet successfully sent.*"
    };

    regex_t regex[N_PATTERNS];
    for (int i = 0; i < N_PATTERNS; ++i) {
        regcomp(&regex[i], patterns[i], 0);
    }

    while (fgets(buffer, BUFSIZ, stdin)) {

        for(int i = 0; i < N_PATTERNS; ++i) {
            match = regexec(&regex[i], buffer, 0, NULL, 0);
            if (match == 0)
                counts[i]++;
        }
    }

    for (int i = 0; i < N_PATTERNS; ++i) {
        printf("PDSTATS> %s | %i\n", patterns[i], counts[i]);
    }
}