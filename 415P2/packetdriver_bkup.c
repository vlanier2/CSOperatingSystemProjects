/*
 * CIS 415 PROJECT 2 AUTHORSHIP STATEMENT
 * VINCENT LANIER 
 * vlanier@uoregon.edu 
 * This is my own work
 */
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

#include "packetdriver.h"

#include "BoundedBuffer.h"
#include "destination.h"
#include "diagnostics.h"
#include "fakeapplications.h"
#include "freepacketdescriptorstore.h"
#include "freepacketdescriptorstore__full.h"
#include "networkdevice.h"
#include "networkdevice__full.h"
#include "packetdescriptorcreator.h"
#include "packetdescriptor.h"
#include "pid.h"
#include "queue.h"

//
#include <stdlib.h>
const int logging = 0;
//

#define UNUSED __attribute__((unused))
#define NETWORK_DEVICE_SEND_ATTEMPTS 5

/* Global Variables */

// static volatile int packet_descriptor_count = 0;
static volatile int packets_lost = 0;
static volatile bool shutdown = false;

pthread_mutex_t queue_lock;
pthread_cond_t queue_activity;
Queue *network_waiting_queue = NULL;
BoundedBuffer *outgoing_buffers[MAX_PID + 1];
BoundedBuffer *mid_pd_buffer = NULL;
NetworkDevice *nd_global;
FreePacketDescriptorStore *fpds_global;

// static void onint(UNUSED int sig) {
// 	shutdown = true;
//     // exit(1);
// }

/* These are the calls to be implemented by the students */

void blocking_send_packet(PacketDescriptor *pd) {

    int enqueued;
    pthread_mutex_lock(&queue_lock);
    while(!(enqueued = network_waiting_queue->enqueue(
            network_waiting_queue, pd)))
        pthread_cond_wait(&queue_activity, &queue_lock);
    pthread_cond_broadcast(&queue_activity);
    pthread_mutex_unlock(&queue_lock);
}

int  nonblocking_send_packet(PacketDescriptor *pd) {
    
    int enqueued;
    pthread_mutex_lock(&queue_lock);
    enqueued = network_waiting_queue->enqueue(network_waiting_queue, pd);
    pthread_cond_broadcast(&queue_activity);
    pthread_mutex_unlock(&queue_lock);
    return enqueued;
}
/* These calls hand in a PacketDescriptor for dispatching */
/* The nonblocking call must return promptly, indicating whether or */
/* not the indicated packet has been accepted by your code          */
/* (it might not be if your internal buffer is full) 1=OK, 0=not OK */
/* The blocking call will usually return promptly, but there may be */
/* a delay while it waits for space in your buffers.                */
/* Neither call should delay until the packet is actually sent      */

void blocking_get_packet(PacketDescriptor **pd, PID pid) {
    
    //
    if (logging)
    printf("*** PD GET TO %i ATTEMPTED BLOCKING\n", pid);
    //    
    
    outgoing_buffers[pid]->blockingRead(
        outgoing_buffers[pid],
        (void **)pd);

    //
    if (logging)
    printf("*** PD GET TO %i SUCCEEDED BLOCKING\n", pid);
    //  
}

int  nonblocking_get_packet(PacketDescriptor **pd, PID pid) {
    
    //
    if (logging)
    printf("*** PD GET TO %i ATTEMPTED NONBLOCKING\n", pid);
    //
    
    int answer = 0;
    answer = outgoing_buffers[pid]->nonblockingRead(
        outgoing_buffers[pid],
        (void **)pd);

    //
    if (logging && answer)
        printf("*** PD GET TO %i SUCCEEDED NONBLOCKING\n", pid);
    //  

    return answer;
}

/* These represent requests for packets by the application threads */
/* The nonblocking call must return promptly, with the result 1 if */
/* a packet was found (and the first argument set accordingly) or  */
/* 0 if no packet was waiting.                                     */
/* The blocking call only returns when a packet has been received  */
/* for the indicated process, and the first arg points at it.      */
/* Both calls indicate their process number and should only be     */
/* given appropriate packets. You may use a small bounded buffer   */
/* to hold packets that haven't yet been collected by a process,   */
/* but are also allowed to discard extra packets if at least one   */
/* is waiting uncollected for the same PID. i.e. applications must */
/* collect their packets reasonably promptly, or risk packet loss. */

// void *application_receiver_routine(void *args) {

// }

void *network_sender_routine(UNUSED void *args) {
    PacketDescriptor *pd;
    pthread_mutex_lock(&queue_lock);
    while(!shutdown) {
        pthread_cond_wait(&queue_activity, &queue_lock);
        if (network_waiting_queue->dequeue(
            network_waiting_queue,
            (void **)&pd))
        {
            for (int i = 0; i < NETWORK_DEVICE_SEND_ATTEMPTS; ++i) 
                if (nd_global->sendPacket(nd_global, pd)) break;
            
            initPD(pd);
            fpds_global->blockingPut(fpds_global, pd);
        }
        pthread_cond_broadcast(&queue_activity);
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

void *network_receiver_routine(UNUSED void *args) {
    PacketDescriptor *free_pd, *filled_pd;
    int answer = 0;
    fpds_global->blockingGet(fpds_global, &free_pd);

    while (!shutdown) {
        initPD(free_pd);
        nd_global->registerPD(nd_global, free_pd);
        nd_global->awaitIncomingPacket(nd_global);
        filled_pd = free_pd;
        answer = fpds_global->nonblockingGet(fpds_global, &free_pd);
        if (answer) {
            mid_pd_buffer->blockingWrite(mid_pd_buffer, (void *)filled_pd);
        } else {
            packets_lost++; 
            printf("PACKETS LOST %i \n", packets_lost);
        }
    }
    return NULL;
}

void *application_sender_routine(UNUSED void *args) {
    PacketDescriptor *filled_pd;
    PID target;
    int answer = 0;

    //
    // printf("started app sender routine \n");
    //

    while (!shutdown) {
        mid_pd_buffer->blockingRead(mid_pd_buffer, (void **)&filled_pd);
        target = getPID(filled_pd);

        //        
        if (logging)
        printf("*** PD DEST TO %i TO APP WAITING BUFFERS\n", target);
        //

        //
        // printf("read a pd from mid buffer for target %i \n", target);
        //
        
        answer = outgoing_buffers[target]->nonblockingWrite(
            outgoing_buffers[target], (void *)filled_pd
        );
        // PACKET LOSS HERE
        if (answer == 0) {
            packets_lost++; 
            printf("PACKETS LOST %i \n", packets_lost);
            initPD(filled_pd);
            fpds_global->blockingPut(fpds_global, filled_pd);
        }
    }
    return NULL;
}



void init_packet_driver(NetworkDevice               *nd, 
                        void                        *mem_start, 
                        unsigned long               mem_length,
                        FreePacketDescriptorStore **fpds_ptr)
{

// application_receiver, 

    pthread_t application_sender,
              network_sender, network_receiver;

    // signal(SIGINT, onint);

    fpds_global = FreePacketDescriptorStore_create(
        mem_start, mem_length
    );
    if (!(fpds_global)) {
        fprintf(stderr, "Malloc Error - free packet desc store\n");
    }

    // int packet_descriptor_count = create_free_packet_descriptors(
    //     fpds_global, mem_start, mem_length
    // );
    // if (!packet_descriptor_count) {
    //     fprintf(stderr, "Malloc Error - packet descriptor\n");
    // }

    network_waiting_queue = Queue_create();
    if (!network_waiting_queue) {
        fprintf(stderr, "Malloc Error - network waiting queue\n");
    }

    for (int i = 0; i < MAX_PID + 1; ++i) {
        outgoing_buffers[i] = BoundedBuffer_create(1);
        if (!outgoing_buffers[i]) {
            fprintf(stderr, "Malloc Error - bounded buffer\n");
        }
    }

    mid_pd_buffer = BoundedBuffer_create(1);
    if (!mid_pd_buffer) {
        fprintf(stderr, "Malloc Error - bounded buffer\n");
    }

    nd_global = nd;
    *fpds_ptr = fpds_global;

    // if (pthread_create( &application_receiver, 
    //                     NULL, 
    //                     application_receiver_routine,
    //                     NULL)) 
    // {
    //     fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
    //                     "application receiver");
    // }
    
    if (pthread_create( &application_sender, 
                        NULL, 
                        application_sender_routine,
                        NULL)) 
    {
        fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
                        "application sender");
    }

    if (pthread_create( &network_receiver, 
                        NULL, 
                        network_receiver_routine,
                        NULL)) 
    {
        fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
                        "network receiver");
    }

    if (pthread_create( &network_sender, 
                        NULL, 
                        network_sender_routine,
                        NULL)) 
    {
        fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
                        "network sender");
    }

    return;
}
/* Called before any other methods, to allow you to initialise */
/* data structures and start any internal threads.             */ 
/* Arguments:                                                  */
/*   nd: the NetworkDevice that you must drive,                */
/*   mem_start, mem_length: some memory for PacketDescriptors  */
/*   fpds_ptr: You hand back a FreePacketDescriptorStore into  */
/*             which you have put the divided up memory        */
/* Hint: just divide the memory up into pieces of the right size */
/*       passing in pointers to each of them                     */ 