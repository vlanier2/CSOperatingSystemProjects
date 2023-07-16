#include <stdlib.h>
#include <stdio.h>
#include <ADTs/hashmap.h>
#include <ADTs/prioqueue.h>
#include <sys/time.h>

int cmp(void *a, void *b) {
    return (long)a - (long)b;
}

long hash(void *x, long N) {
    return ((long)x % N);
}

typedef struct {
    int key;
    int len;
    char *str;
} TestData;

void assign(TestData *td, int key, int len, char *str) {
    td->key = key;
    td->len = len;
    td->str = str;
}

int main(int argc, char* argv[]) {

    struct timeval tv;

    gettimeofday(&tv, NULL);

    suseconds_t r;

    const PrioQueue *q = PrioQueue_create(cmp, doNothing, free);
    const Map *dict = HashMap(1024L, 2.0, hash, cmp, doNothing, free);

    TestData *A = (TestData *)malloc(sizeof(TestData) * 3);

    assign(&A[0], 1, 3, "ABC");
    assign(&A[1], 2, 4, "DEFG");
    assign(&A[2], 3, 5, "GHIDD");

    for (int i = 0; i < 3; ++i) { 
        q->insert(q, (void *)&A[i].len, (void *)&A[i]);
        dict->putUnique(dict, (void *)&A[i].key, (void*)&A[i]);
    }

    int *n; TestData *td;
    q->removeMin(q, (void **)&n, (void *)&td);

    printf("%s \n", td->str);
    printf("%i \n", td->len);
    printf("%i \n", *n);

    q->removeMin(q, (void *)&n, (void *)&td);

    printf("%s \n", td->str);
    printf("%i \n", td->len);
    printf("%i \n", *n);

    dict->get(dict, (void *)&A[0].key, (void *)&td);

    printf("%s \n", td->str);

    dict->get(dict, (void *)&A[2].key, (void *)&td);

    printf("%s \n", td->str);

    return 0;
}