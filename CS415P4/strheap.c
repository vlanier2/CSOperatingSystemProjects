/*
 * CS415 Project 4 Authorship Statement
 * Vincent Lanier
 * vlanier@uoregon.edu
 * vlanier
 * 
 * The following submission is my own work aside from assistance
 * from 415 staff
 */

#include "strheap.h"
#include <pthread.h>
#include <ADTs/hashcskmap.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define TESTLOGGING 0 // TESTLOGGING 0 FOR FINAL SUBMISSION

#if TESTLOGGING
    #define LOG(fmt, ...) fprintf(stdout, "[LOG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG(fmt, ...) ((void)0)
#endif

/* global refs */
static bool isinit = false;
static const CSKMap *map = NULL;
static pthread_mutex_t heap_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {

    char *str;
    unsigned long count;

} HeapEntry;

static void onexit() {
    if (map) map->destroy(map);
    pthread_mutex_destroy(&heap_mutex);
}

static void freeEntry(void *entry) {
    HeapEntry *he = (HeapEntry *)entry;
    free(he->str);
    free(he);
}

static void initialize_heap() {
    LOG("STRHEAP INITIALIZED");
    map = HashCSKMap(10, 5.0, freeEntry); // safe to use
    assert(map);
    assert(atexit(onexit) == 0);
    isinit = true;
}

char *str_malloc(char *string) 
{
    HeapEntry *he;
    pthread_mutex_lock(&heap_mutex);

    if (!isinit) initialize_heap();

    bool found = map->get(map, string, (void **)&he);
    if (found) {
        LOG("%s FOUND ON MALLOC", string);
        he->count++;
    } else {
        LOG("%s NOT FOUND ON MALLOC", string);
        he = (HeapEntry *)malloc(sizeof(HeapEntry));
        assert(he);
        he->str = strdup(string);
        he->count = 1;
        if (!map->putUnique(map, string, (void *)he)) {
            LOG("ERROR %s NOT UNIQUE", string);
        }
    }
    pthread_mutex_unlock(&heap_mutex);

    return he->str;
}

bool str_free(char *string) 
{
    HeapEntry *he;
    pthread_mutex_lock(&heap_mutex);

    if (!isinit) return false; // you have called free without malloc

    bool found = map->get(map, string, (void **)&he);
    if (found) {
        LOG("%s FOUND ON FREE", string);
        he->count--;
        if (he->count == 0) map->remove(map, string);
    } else 
        LOG("%s NOT FOUND ON FREE", string);
    pthread_mutex_unlock(&heap_mutex);

    return found;
}