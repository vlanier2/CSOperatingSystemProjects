/*
 * CIS 415 PROJECT 2 AUTHORSHIP STATEMENT
 * VINCENT LANIER 
 * vlanier@uoregon.edu 
 * This is my own work with assistance from TA's GE and Prof Sventek
 */
#include <pthread.h>        // for threads
#include <stdbool.h>        // for bool

#include "packetdriver.h"   // this .c 

#include "BoundedBuffer.h"  // for BB
#include "diagnostics.h"    // for DIAGNOSTICS
#include "freepacketdescriptorstore.h"
#include "freepacketdescriptorstore__full.h"
#include "networkdevice.h"
#include "networkdevice__full.h"
#include "packetdescriptor.h"
#include "pid.h"

#define UNUSED __attribute__((unused))
#define NETWORK_DEVICE_SEND_ATTEMPTS 5
#define COMM_BUFFER_SIZE 2

/* Global Variables */

static unsigned long packet_descriptor_count = 0;
static volatile int packets_lost = 0;
static bool shutdown = false;

static BoundedBuffer *outgoing_buffers[MAX_PID + 1];
static BoundedBuffer *network_waiting_queue = NULL;
static BoundedBuffer *mid_pd_buffer = NULL;

struct thread_args {
    NetworkDevice *nd_arg;
    FreePacketDescriptorStore *fpds_arg;
};

/*
 * To be called by applications to hand in a packet for 
 * dispatching by the packet driver.
 *
 * Blocks (does not return)
 * until packet is accepted for dispatch by driver
 * 
 * (performs a blocking write on network waiting queue)
 */
void blocking_send_packet(PacketDescriptor *pd) {
    network_waiting_queue->blockingWrite(
        network_waiting_queue,
        (void*)pd
    );
}

/*
 * To be called by applications to hand in a packet for 
 * dispatching by the packet driver.
 *
 * Immediately returns 1 if successful, 0 if buffer is full
 * 
 * (performs a nonblocking write on network waiting queue)
 */
int  nonblocking_send_packet(PacketDescriptor *pd) {
    int enqueued;
    enqueued = network_waiting_queue->nonblockingWrite(
        network_waiting_queue, 
        (void *)pd
    );
    return enqueued;
}

/*
 * To be called by applications to retrieve a packet from  
 * the network device. Transmitted by the packetdriver.
 *
 * Blocks (does not return) until a packet is aquired by the app
 * 
 * (performs a blocking read on waiting app buffer[PID])
 * Applications must promptly retrieve packets or risk loss
 */
void blocking_get_packet(PacketDescriptor **pd, PID pid) {    
    outgoing_buffers[pid]->blockingRead(
        outgoing_buffers[pid],
        (void **)pd
    );
}

/*
 * To be called by applications to retrieve a packet from 
 * the network device. Transmitted by the packetdriver.
 *
 * Immediately returns 1 if successful, 0 if no packet waiting
 * 
 * (performs a nonblocking read on waiting app buffer[PID])
 * Applications must promptly retrieve packets or risk loss
 */
int  nonblocking_get_packet(PacketDescriptor **pd, PID pid) {    
    int answer = 0;
    answer = outgoing_buffers[pid]->nonblockingRead(
        outgoing_buffers[pid],
        (void **)pd
    );
    return answer;
}

/*
 * Repeatedly wait for packet from the network waiting queue
 * attempt to transmit the packet to the network device driver
 * return packet descriptor to the FPDS
 */
static void *network_sender_routine(void *args) {
    PacketDescriptor *pd;
    struct thread_args *ta = (struct thread_args *)args;

    while(!shutdown) {
        // immediately attempt a blocking read on the queue
        network_waiting_queue->blockingRead(
            network_waiting_queue,
            (void **)&pd
        );
        
        // attempt to send the packet a number of time
        // per networkdevice.h sendpacket may take substantial
        // time to return. if it fails many times accept failure
        for (int i = 0; i < NETWORK_DEVICE_SEND_ATTEMPTS; ++i) 
            if (ta->nd_arg->sendPacket(ta->nd_arg, pd)) break;

        // regardless, clear packet and return to FPDS    
        initPD(pd);
        ta->fpds_arg->blockingPut(ta->fpds_arg, pd);

        // back to top - wait for new packet from queue
    }
    return NULL;
}
/*
 * prioritize readying a free packet descriptor to await
 * a packet from the network device driver
 * immediately attempt to ready a new free descriptor,
 * if packet descriptor shortage the previous is reused and
 * not transmitted to the applications, leading to potential
 * packet loss (instructed by prof Sventek).
 */
static void *network_receiver_routine(void *args) {
    PacketDescriptor *free_pd, *filled_pd;
    struct thread_args *ta = (struct thread_args *)args;
    int answer = 0;

    // Force a free packet get before startup
    ta->fpds_arg->blockingGet(ta->fpds_arg, &free_pd);

    while (!shutdown) {
        initPD(free_pd); // clear pd, register, await
        ta->nd_arg->registerPD(ta->nd_arg, free_pd);
        ta->nd_arg->awaitIncomingPacket(ta->nd_arg);

        // immediately attempt to ready a new pd
        filled_pd = free_pd;
        answer = ta->fpds_arg->nonblockingGet(ta->fpds_arg, &free_pd);
        
        if (answer) // if a new free pd is aquired send the packet
            // a blocking write should be acceptable here
            // since the application sender thread should immediately
            // handle packets from the comm buffer. Regardless
            // I have allocated more than one buffer slot here.
            mid_pd_buffer->blockingWrite(mid_pd_buffer, (void *)filled_pd);
        else // else packet shortage leads to reuse and packet loss
            DIAGNOSTICS("PACKET LOST VIA SHORTAGE | TOTAL LOST : %i \n", ++packets_lost);
    }
    return NULL;
}

/*
 * immediately clear the comm buffer of packets indexing each to 
 * the appropriate destination app's 'mailbox buffer'.
 * From instructions here, packet loss is permitted. 
 * If a packet is already waiting the new packet will be lost
 */
static void *application_sender_routine(void *args) {
    PacketDescriptor *filled_pd;
    PID target;
    struct thread_args *ta = (struct thread_args *)args;
    int answer = 0;

    while (!shutdown) {
        // immediately be ready to read from the comm buffer
        mid_pd_buffer->blockingRead(mid_pd_buffer, (void **)&filled_pd);
        target = getPID(filled_pd); // get target app
        // attempt a write to the app's 'mailbox buffer'
        answer = outgoing_buffers[target]->nonblockingWrite(
            outgoing_buffers[target], (void *)filled_pd
        );
        if (answer == 0) { 
            // failure implies app was slow to pickup
            // packet loss is permitted 
            DIAGNOSTICS("PACKET LOST VIA SLOW APP | TOTAL LOST : %i \n", ++packets_lost);
            
        // clear packet and return to FPDS regardless of success
        initPD(filled_pd);
        ta->fpds_arg->blockingPut(ta->fpds_arg, filled_pd);
        }
    }
    return NULL;
}


/*
 * Initialize the packet driver.
 * 
 * Creates the following:
 *      array of buffers of size 1 (one for each app)
 *      communication buffer for sender thread and receiver thread
 *      outgoing buffer to hold packets sent from apps
 *      Free Packet Descriptor Store of size (mem_length) at mem_start
 * 
 * Starts three threads for operation of packet driver
 *      - Network sender thread to manage the outgoing queue and
 *      send to network device driver
 *      - Network receiver thread to handle registering PD's with
 *      the network device driver and handoff to app sender thread
 *      - App sender thread to manage array of buffers and clear
 *      comm buffer 
 */
void init_packet_driver(NetworkDevice               *nd, 
                        void                        *mem_start, 
                        unsigned long               mem_length,
                        FreePacketDescriptorStore **fpds_ptr)
{
    pthread_t application_sender, // to index packets outbound to apps
              network_sender,     // to send accepted packets to network  
              network_receiver;   // to receive packets from the network

    FreePacketDescriptorStore *fpds_arg = NULL;

    /* Create Needed Data Structures */
    
    // create FPDS
    fpds_arg = FreePacketDescriptorStore_create(
        mem_start, mem_length
    );
    if (!(fpds_arg)) {
        fprintf(stderr, "Malloc Error - free packet desc store\n");
    }
    // get count of PDs
    packet_descriptor_count = fpds_arg->size(fpds_arg);

    // create communication buffer between app sender and network rcvr
    mid_pd_buffer = BoundedBuffer_create(COMM_BUFFER_SIZE);
    if (!mid_pd_buffer) {
        fprintf(stderr, "Malloc Error - bounded buffer\n");
    }

    // create outgoing to network queue buffer
    // size of total PD - total apps - size of comm buffer
    network_waiting_queue = BoundedBuffer_create(
        packet_descriptor_count - (MAX_PID + 1) - COMM_BUFFER_SIZE
    );
    if (!network_waiting_queue) {
        fprintf(stderr, "Malloc Error - network waiting queue\n");
    }

    // create array of 'mailbox' buffers for apps
    // allows O(1) getpackets for apps
    for (int i = 0; i < MAX_PID + 1; ++i) {
        outgoing_buffers[i] = BoundedBuffer_create(1);
        if (!outgoing_buffers[i]) {
            fprintf(stderr, "Malloc Error - bounded buffer\n");
        }
    }

    // nd_arg = nd; // set global ref to network for threads
    struct thread_args args;
    args.nd_arg = nd;
    args.fpds_arg = *fpds_ptr;
    *fpds_ptr = fpds_arg; // return fdps

    DIAGNOSTICS("init> Size of FPDS : %li\n", fpds_arg->size(fpds_arg));
    DIAGNOSTICS("init> Size of Queue : %li\n", 
    packet_descriptor_count - (MAX_PID + 1) - COMM_BUFFER_SIZE);
    DIAGNOSTICS("init> Size of Comm buffer : %i\n", COMM_BUFFER_SIZE);
    DIAGNOSTICS("init> Size of mailboxes array : %i\n", MAX_PID + 1);

    /* Create Threads in Reverse Order */

    if (pthread_create( &application_sender, 
                        NULL, 
                        application_sender_routine,
                        (void *)&args)) 
    {
        fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
                        "application sender");
    }

    if (pthread_create( &network_receiver, 
                        NULL, 
                        network_receiver_routine,
                        (void *)&args)) 
    {
        fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
                        "network receiver");
    }

    if (pthread_create( &network_sender, 
                        NULL, 
                        network_sender_routine,
                        (void *)&args)) 
    {
        fprintf(stderr, "FAILED TO CREATE THREAD %s \n", 
                        "network sender");
    }

    return;
}
