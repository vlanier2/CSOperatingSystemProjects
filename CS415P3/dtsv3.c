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
#include <ADTs/hashmap.h>
#include <ADTs/prioqueue.h>
#include <ADTs/queue.h>

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <valgrind/valgrind.h>
#include <unistd.h>

#define TESTLOGGING 0 // TESTLOGGING 0 FOR FINAL SUBMISSION

#if TESTLOGGING
    #define LOG(fmt, ...) fprintf(stdout, "[LOG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG(fmt, ...) ((void)0)
#endif

#define UNUSED __attribute__((unused))

#define TIMER_INTERVAL_MSECS 10
#define QUERY_SIZE 16384u
#define MAX_WORDS 25
#define PORT 19999
#define SERVICE "DTS"

static volatile unsigned long SVID = 0UL;
pthread_mutex_t pq_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cb_q_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cb_q_cond = PTHREAD_COND_INITIALIZER;

enum RequestType {
    OneShot,
    Repeat,
    Cancel
};

// Thread Arguments
typedef struct {

    BXPEndpoint *ep;
    BXPService *svc;
    const Map *map;
    const PrioQueue *pq;
    const Queue *q;

} TArgs;

// Request Data Struct
typedef struct {

    bool cancelled;
    enum RequestType request;
    struct timeval tv;
    unsigned msecs, repeats, port;
    unsigned long clid, svid;
    char *host, *service;

} RequestData;

// Compare TimeVals
int tvcmp(void *tv1, void *tv2) {
    struct timeval *t1 = (struct timeval *)tv1;
    struct timeval *t2 = (struct timeval *)tv2;

    if (t1->tv_sec < t2->tv_sec) return -1;
    if (t1->tv_sec > t2->tv_sec) return 1;
    if (t1->tv_usec < t2->tv_usec) return -1;
    if (t1->tv_usec > t2->tv_usec) return 1;
    
    return 0;
}

// Compare Longs
int cmp(void *v1, void *v2) {
return (long)v1 - (long)v2;
}

// Hash Long IDs
long hash(void *x, long N) {
    return ((long)x % N);
}

// Fill an Alloced RD With Words arr
void fillOneShotData(RequestData *rd, char *words[]) {
    rd->cancelled = false;
    rd->request = OneShot;
    rd->clid = atoi(words[1]);
    rd->svid = ++SVID;
    rd->tv.tv_sec = atol(words[2]);
    rd->tv.tv_usec = atol(words[3]);
    rd->msecs = -1;
    rd->repeats = -1;
    rd->host = strdup(words[4]);
    rd->service = strdup(words[5]);
    rd->port = atoi(words[6]);
}

// Fill an Alloced RD With Words arr
void fillRepeatData(RequestData *rd, char *words[]) {
    rd->cancelled = false;
    rd->request = Repeat;
    rd->clid = atoi(words[1]);
    rd->svid = ++SVID;
    rd->tv.tv_sec = atol(words[2]);
    rd->tv.tv_usec = atol(words[3]);
    rd->msecs = atoi(words[4]);
    rd->repeats = atoi(words[5]);
    rd->host = strdup(words[6]);
    rd->service = strdup(words[7]);
    rd->port = atoi(words[8]);
}

bool addRD(RequestData *rd, const Map *map, const PrioQueue *pq) {
    bool insert_succ, put_succ;
    pthread_mutex_lock(&pq_lock);

                LOG( "insert to PQ: %lu|%s|%s|%u\n", 
                rd->clid, rd->host, rd->service, rd->port); // debug

    insert_succ = pq->insert(pq, (void *)&(rd->tv), (void *)rd);
    put_succ = map->putUnique(map, (void *)(rd->svid), (void *)rd);

    pthread_mutex_unlock(&pq_lock);

    if (!insert_succ) LOG("Failed insert");
    if (!put_succ) LOG("Failed put");

    VALGRIND_MONITOR_COMMAND("leak_check summary");

    return insert_succ && put_succ;
}

bool cancelRD(long svid, const Map *map) {
    bool succ = false;
    RequestData *rd;
    pthread_mutex_lock(&pq_lock);
    if ((succ = map->get(map, (void *)svid, (void **)&rd))) {
        LOG("SETTING CANCEL ON %lu", rd->svid);
        rd->cancelled = true;
    } else LOG("FAILED TO FIND %lu IN TABLE", svid);
    pthread_mutex_unlock(&pq_lock);
    return succ;
}

void destroyRD(RequestData *rd) {
    free(rd->host);
    free(rd->service);
    free(rd);
}

bool harvest(const PrioQueue *from, const Queue *to, const Map *map) {
    struct timeval tv, *priority;
    RequestData *rd;
    bool harvested = false;

    gettimeofday(&tv, NULL);
    
    pthread_mutex_lock(&pq_lock);
    pthread_mutex_lock(&cb_q_lock);

    if (from->min(from, (void **)&priority, (void**)&rd)) {

        // LOG("PEEK %li PRIORITY %li", rd->svid, priority->tv_sec);

        if((harvested = (tvcmp((void *)&tv, (void*)priority) >= 0))) {
            from->removeMin(from, (void **)&priority, (void**)&rd);
            map->remove(map, (void *)(rd->svid));

            // LOG("HARVESTED %li WITH PRIORITY %li, %s", rd->svid, 
            // priority->tv_sec, rd->cancelled? "CANCELED" : "ACTIVE");
            
            if (rd->cancelled) {
                LOG("REQUEST %li CANCELED NOT MOVED TO CB QUEUE", rd->svid);
                destroyRD(rd);
                VALGRIND_MONITOR_COMMAND("leak_check summary");

            } else {                
                LOG( "enqueued for callback: %lu|%s|%s|%u\n", 
                rd->clid, rd->host, rd->service, rd->port); // debug
                
                to->enqueue(to, (void *)rd);
                pthread_cond_signal(&cb_q_cond);
            }
        }
    }

    pthread_mutex_unlock(&cb_q_lock);
    pthread_mutex_unlock(&pq_lock);

    return harvested;
}

// FROM LAB 7 - with security for too long queries
int extractWords(char *buf, char *sep, char *words[]) {
    int i;
    char *p;

    for (   p = strtok(buf, sep), i = 0; 
            p != NULL && i < MAX_WORDS; 
            p = strtok(NULL, sep), i++)
        words[i] = p;
    words[i] = NULL;
    return i;
}

void *request_handler_routine(void *arg) {
    TArgs *args = (TArgs *)arg;

    char query[QUERY_SIZE], response[QUERY_SIZE + 1], *words[MAX_WORDS];
    unsigned int qlen, rlen;
    int wordc;
    RequestData *rd;

    while ((qlen = bxp_query(*(args->svc), args->ep, query, QUERY_SIZE)) > 0)
    {
        LOG("RCVD %s", query);
        wordc = extractWords(query, "|", words);

        if ((strcmp(words[0], "OneShot") == 0) && (wordc == 7)) {
            assert((rd = (RequestData *)malloc(sizeof(RequestData))));
            fillOneShotData(rd, words);
            if (addRD(rd, args->map, args->pq))
                sprintf(response, "1%08lu", rd->svid);
            else 
                sprintf(response, "0%08lu", rd->svid);
        }
        
        else if ((strcmp(words[0], "Repeat") == 0) && (wordc == 9)) {
            sprintf(response, "0%s", query);
        }
        
        else if ((strcmp(words[0], "Cancel") == 0) && (wordc == 2)) {
            if (cancelRD(atol(words[1]), args->map))
                sprintf(response, "1%08lu", atol(words[1]));
            else
                sprintf(response, "0%08lu", atol(words[1]));
        } 
        
        else {
            sprintf(response, "0%s", query);
        }

        rlen = strlen(response) + 1;
        // LOG("%s", response);
        if(!bxp_response(*(args->svc), args->ep, response, rlen))
            LOG("WARNING bxp reponse failed");
    }

    pthread_exit(NULL);
}

void *timer_harvester_routine(void *arg) {
    TArgs *args = (TArgs *)arg;
    // struct timespec ts = {0, TIMER_INTERVAL_MSECS * 1000};
    
    for (;;) {  
        while(harvest(args->pq, args->q, args->map));
        // (void)nanosleep(&ts, NULL);
        (void)usleep(TIMER_INTERVAL_MSECS * 1000);

    }

    LOG("Timer thread exited");

    pthread_exit(NULL);
}

void *callback_routine(void *arg) {
    TArgs *args = (TArgs *)arg;

    RequestData *rd = NULL;
    BXPConnection bxpc;
    unsigned reqlen, resplen;
    char req[QUERY_SIZE], resp[QUERY_SIZE + 1];

    while(true) {
        pthread_mutex_lock(&cb_q_lock); 
        while(!(args->q->dequeue(args->q, (void **)&rd))) {
            pthread_cond_wait(&cb_q_cond, &cb_q_lock);
        }
        pthread_mutex_unlock(&cb_q_lock);

        // CALLBACK TO CLIENT
        // LOG("ATTEMPTING CB TO CLIENT REQUEST %li", rd->svid);

        printf( "Event fired: %lu|%s|%s|%u\n", 
                rd->clid, rd->host, rd->service, rd->port); // required print
    
        bxpc = bxp_connect(rd->host, rd->port, rd->service, 1, 1);
        if (bxpc) {
            reqlen = (unsigned)(sprintf(req, "%lu", rd->clid) + 1);
            bxp_call(bxpc, req, reqlen, resp, sizeof resp, &resplen);
            bxp_disconnect(bxpc);
        } else {LOG("CONNECTION FAILED");}
        destroyRD(rd);

        VALGRIND_MONITOR_COMMAND("leak_check summary");

    }
    pthread_exit(NULL);
}

int main(UNUSED int argc, UNUSED char* argv[])
{
    int exit_val = EXIT_FAILURE;
    
    BXPEndpoint ep;
    BXPService svc;
    const Map *map = NULL;
    const PrioQueue *pq = NULL;
    const Queue *q = NULL;
    pthread_t request_handler = 0, timer = 0, callback_handler = 0;

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

    if(!(pq = PrioQueue_create(tvcmp, doNothing, doNothing))) {
        LOG("ERROR failed to create priority queue");
        goto cleanup;
    }

    if(!(map = HashMap(1024L, 2.0, hash, cmp, doNothing, doNothing))) {
        LOG("ERROR failed to create hashmap");
        goto cleanup;
    }

    if(!(q = Queue_create(doNothing))) {
        LOG("ERROR failed to create queue");
        goto cleanup;
    }

    // Create a thread that will receive requests from client applications.
    TArgs thread_args = {&ep, &svc, map, pq, q};

    if (pthread_create( &callback_handler,
                        NULL,
                        callback_routine,
                        (void *)&thread_args)) 
    {
        LOG("ERROR failed to create callback thread");
        goto cleanup;
    }

    if (pthread_create( &request_handler,
                        NULL,
                        request_handler_routine,
                        (void*)&thread_args))
    {
        LOG("ERROR failed to create request handling thread");
        goto cleanup;
    }

    if (pthread_create( &timer,
                        NULL,
                        timer_harvester_routine,
                        (void*)&thread_args))
    {
        LOG("ERROR failed to create timer thread");
        goto cleanup;
    }

    VALGRIND_MONITOR_COMMAND("leak_check summary");

    exit_val = EXIT_SUCCESS;
    cleanup:
        if (request_handler) pthread_join(request_handler, NULL);
        if (timer) pthread_join(timer, NULL);
        if (callback_handler) pthread_join(callback_handler, NULL);
        if (pq) pq->destroy(pq);
        if (map) map->destroy(map);
        if (q) q->destroy(q);
        pthread_cond_destroy(&cb_q_cond);
        pthread_mutex_destroy(&pq_lock);
        pthread_mutex_destroy(&cb_q_lock);

    return exit_val;
}