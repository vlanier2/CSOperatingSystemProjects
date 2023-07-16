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
#define HASHSHIFT 31L

static volatile unsigned long SVID = 0UL;
pthread_mutex_t pq_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cb_q_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
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
    const Map *cache;

} TArgs;

// Request Data Struct
typedef struct {

    bool cancelled;
    enum RequestType request;
    struct timeval tv;
    int msecs, repeats, port;
    unsigned long clid, svid;
    char *host, *service;

} RequestData;

// Cache Value Struct
typedef struct {

    BXPConnection bxpc;
    unsigned RequestCount;

} CxnCacheVal;
 
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

int RDequals(void *x1, void *x2) {
    RequestData *rd1 = (RequestData *)x1;
    RequestData *rd2 = (RequestData *)x2;

    // doesnt work
    // return (strcmp(rd1->host, rd2->host) == 0 &&
    //         strcmp(rd1->service, rd2->service) == 0 &&
    //         rd1->port == rd2->port);

    char buffer1[100], buffer2[100];
    sprintf(buffer1, "%s%s%i", rd1->service, rd1->host, rd1->port);
    sprintf(buffer2, "%s%s%i", rd2->service, rd2->host, rd2->port);

    return strcmp(buffer1, buffer2);
}

// Hash Long IDs
long hash(void *x, long N) {
    return ((long)x % N);
}

// Hash Request Data Considering only port service host
// Could be more efficient
long CxnCacheHash(void *x, long N) {
    RequestData *rd = (RequestData *)x;
    unsigned long ans = 0;
    for (char *c = rd->service; *c != '\0'; c++)
        ans = HASHSHIFT * ans + (unsigned long)*c;
    for (char *c = rd->host; *c != '\0'; c++)
        ans = HASHSHIFT * ans + (unsigned long)*c;
    ans += (long)rd->port;
    
    return (long)(ans % N);
}

// freeval for cache, disconnects bxpc
void destroyCacheVal(void *v) {
    CxnCacheVal *ccv = (CxnCacheVal *)v;
    bxp_disconnect(ccv->bxpc);
    LOG("CACHE > bxp disconnect called");
    free(ccv);
}

bool cacheAddAndIncr(RequestData *rd, const Map *cache) {
    CxnCacheVal* cv;

    pthread_mutex_lock(&cache_lock);
    bool found = cache->get(cache, (void *)rd, (void **)&cv);

    if (found) {
        cv->RequestCount++;
    } else {
        LOG("ADDING TO CACHE");
        cv = (CxnCacheVal *)malloc(sizeof(CxnCacheVal));
        assert(cv);
        cv->bxpc = bxp_connect(rd->host, rd->port, rd->service, 1, 1);
        if (!cv->bxpc) LOG("CONNECTION FAILED");
        cv->RequestCount = 1;
        if (!cache->putUnique(cache, (void *)rd, (void *)cv)) {
            LOG("FAILED CACHE PUT");
        }
    }
    pthread_mutex_unlock(&cache_lock);

    return found;
}

bool cacheLookupAndDecr(RequestData *rd, CxnCacheVal **cv, const Map *cache) {
    
    pthread_mutex_lock(&cache_lock);
    bool found = cache->get(cache, (void *)rd, (void **)cv);

    if (found) {

        LOG("FOUND IN CACHE");

        (*cv)->RequestCount--;
    } else {
        // should never happen
        LOG("CACHE LOOKUP MISS");
    }
    pthread_mutex_unlock(&cache_lock);

    return found;
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

bool addRD(RequestData *rd, const Map *map, const PrioQueue *pq, const Map *cache) {
    bool insert_succ, put_succ;

    pthread_mutex_lock(&pq_lock);

                LOG( "insert to PQ: %lu|%s|%s|%u\n", 
                rd->clid, rd->host, rd->service, rd->port); // debug

    insert_succ = pq->insert(pq, (void *)&(rd->tv), (void *)rd);
    put_succ = map->putUnique(map, (void *)(rd->svid), (void *)rd);

    if (insert_succ && put_succ) {
        cacheAddAndIncr(rd, cache);
    }

    pthread_mutex_unlock(&pq_lock);

    if (!insert_succ) LOG("Failed insert");
    if (!put_succ) LOG("Failed put");

    VALGRIND_MONITOR_COMMAND("leak_check summary");

    return insert_succ && put_succ;
}

bool cancelRD(long svid, const Map *map, const Map *cache) {
    bool succ = false;
    RequestData *rd;
    pthread_mutex_lock(&pq_lock);
    if ((succ = map->get(map, (void *)svid, (void **)&rd))) {
        LOG("SETTING CANCEL ON %lu", rd->svid);
        rd->cancelled = true;
    }

    if (!succ) {
        LOG("FAILED TO FIND %lu IN TABLE", svid);
        pthread_mutex_unlock(&pq_lock);
        return succ;
    } // otherwise handle the cache

    CxnCacheVal *cv;
    cacheLookupAndDecr(rd, &cv, cache);
    if (cv->RequestCount < 1) {
        pthread_mutex_lock(&cache_lock);
        LOG("REMOVE FROM CACHE (canceld)");
        cache->remove(cache, (void *)rd);
        pthread_mutex_unlock(&cache_lock);
    }

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
            if (addRD(rd, args->map, args->pq, args->cache))
                sprintf(response, "1%08lu", rd->svid);
            else 
                sprintf(response, "0%08lu", rd->svid);
        }
        
        else if ((strcmp(words[0], "Repeat") == 0) && (wordc == 9)) {
            assert((rd = (RequestData *)malloc(sizeof(RequestData))));
            fillRepeatData(rd, words);
            if (addRD(rd, args->map, args->pq, args->cache))
                sprintf(response, "1%08lu", rd->svid);
            else
                sprintf(response, "0%08lu", rd->svid);
        }
        
        else if ((strcmp(words[0], "Cancel") == 0) && (wordc == 2)) {
            if (cancelRD(atol(words[1]), args->map, args->cache))
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
    // fix bug for submission two
    
    for (;;) {  
        while(harvest(args->pq, args->q, args->map));
        // (void)nanosleep(&ts, NULL); fix bug for submission two
        (void)usleep(TIMER_INTERVAL_MSECS * 1000);
    }

    LOG("Timer thread exited");

    pthread_exit(NULL);
}

void *callback_routine(void *arg) {
    TArgs *args = (TArgs *)arg;

    RequestData *rd = NULL;
    unsigned reqlen, resplen;
    char req[QUERY_SIZE], resp[QUERY_SIZE + 1];

    while(true) {
        pthread_mutex_lock(&cb_q_lock); 
        while(!(args->q->dequeue(args->q, (void **)&rd))) {
            pthread_cond_wait(&cb_q_cond, &cb_q_lock);
        }
        pthread_mutex_unlock(&cb_q_lock);

        printf( "Event fired: %lu|%s|%s|%u\n", 
                rd->clid, rd->host, rd->service, rd->port); // required print

        CxnCacheVal *cv = NULL;
        assert(cacheLookupAndDecr(rd, &cv, args->cache));
        assert(cv->bxpc);

        reqlen = (unsigned)(sprintf(req, "%lu", rd->clid) + 1);
        bxp_call(cv->bxpc, req, reqlen, resp, sizeof resp, &resplen);

        if ((rd->repeats > -1) && (rd->repeats != 1)) // repeat, keep cache
        {
            long secs = rd->msecs / 1000;
            long usecs = (rd->msecs % 1000) * 1000;
            rd->tv.tv_sec += secs;
            rd->tv.tv_usec += usecs;
            if (rd->repeats > 0) --rd->repeats; 
            addRD(rd, args->map, args->pq, args->cache);
        }
        else if (cv->RequestCount < 1) // ok to free cache and destroy rd
        {
            pthread_mutex_lock(&cache_lock);
            LOG("REMOVE FROM CACHE");
            args->cache->remove(args->cache, (void *)rd);
            pthread_mutex_unlock(&cache_lock);
            destroyRD(rd);
            VALGRIND_MONITOR_COMMAND("leak_check summary");
        } 
        // else it isnt an active repeat, but other requests using cache
    }
    pthread_exit(NULL);
}

int main(UNUSED int argc, UNUSED char* argv[])
{
    int exit_val = EXIT_FAILURE;
    
    BXPEndpoint ep;
    BXPService svc;
    const Map *map = NULL;
    const Map *cache = NULL;
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

    if(!(cache = HashMap(1024L, 2.0, CxnCacheHash, RDequals,
                        doNothing, destroyCacheVal))) {
        LOG("ERROR failed to create cache");
    }

    if(!(q = Queue_create(doNothing))) {
        LOG("ERROR failed to create queue");
        goto cleanup;
    }

    // Create a thread that will receive requests from client applications.
    TArgs thread_args = {&ep, &svc, map, pq, q, cache};

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
        pthread_mutex_destroy(&cache_lock);        

    return exit_val;
}