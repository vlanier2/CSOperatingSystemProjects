/*
* CS415 PROJECT 3 AUTHORSHIP STATEMENT
* VINCENT LANIER
* vlanier@uoregon.edu  
* vlanier
* THIS IS MY OWN WORK EXCEPT FOR ASSISTANCE FROM STAFF
* Special thanks to Prof Sventek for advising on part 4
* and Adam Case for helping track down bugs remarkably fast
*/

#include "BXP/bxp.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define TESTLOGGING 0 // TESTLOGGING 0 FOR FINAL SUBMISSION

#if TESTLOGGING
    #define LOG(fmt, ...) fprintf(stdout, "[LOG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG(fmt, ...) ((void)0)
#endif

#define UNUSED __attribute__((unused))

#define QUERY_SIZE 16384u
#define MAX_WORDS 25u
#define PORT 19999
#define SERVICE "DTS"

typedef struct {

    BXPEndpoint *ep;
    BXPService *svc;

} RequestHandlerArgs;

// Responds to each request by echoing back the received 
// request along with a status byte; 
// the first byte of the response is ‘1’ for success, ‘0’ for failure. 
void* request_handler_routine(void *arg) {
    RequestHandlerArgs *args = (RequestHandlerArgs *)arg;

    char query[QUERY_SIZE], response[QUERY_SIZE + 1];
    unsigned int qlen, rlen;

    while ((qlen = bxp_query(*(args->svc), args->ep, query, QUERY_SIZE)) > 0)
    {
        LOG("RCVD %s", query);

        sprintf(response, "1%s", query);
        rlen = strlen(response) + 1;
        if(!bxp_response(*(args->svc), args->ep, response, rlen))
            LOG("WARNING bxp reponse failed");
    }

    pthread_exit(NULL);
}

int main(UNUSED int argc, UNUSED char* argv[])
{
    int exit_val = EXIT_FAILURE;
    
    BXPEndpoint ep;
    BXPService svc;
    pthread_t request_handler = 0;

    // Initialize the BXP runtime1 so that it can create and accept 
    // encrypted connection requests.
    if (!bxp_init(PORT, 1)) {
        LOG("ERROR bxp_init failed"); 
        goto cleanup;
    }

    if (!(svc = bxp_offer(SERVICE))) {
        LOG("ERROR bxp_offer failed"); 
        goto cleanup;
    }

    // Create a thread that will receive requests from client applications.
    RequestHandlerArgs thread_args = {&ep, &svc};
    if (pthread_create( &request_handler,
                        NULL,
                        request_handler_routine,
                        (void*)&thread_args))
    {
        LOG("ERROR failed to create thread");
        goto cleanup;
    }

    exit_val = EXIT_SUCCESS;

    cleanup:
        if (request_handler) pthread_join(request_handler, NULL);

    return exit_val;
}