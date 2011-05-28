#include "datagen.h"

extern "C" {
#include <pthread.h>    
}

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

DataGen::DataGen() {
    bitRate = 64;       // kbps
    bRunning = false;
}

DataGen::~DataGen() {
    if (bRunning) {
        stopThread();
    }
}

void DataGen::startThread() {
    if (!bRunning) {
        pthread_mutex_lock(&data_mutex);
        bRunning = true;
        pthread_mutex_unlock(&data_mutex);

        pthread_t ccn_thread;
        pthread_create(&ccn_thread, NULL, &run, (void *) this);
    }
}

void DataGen::stopThread() {
    if (bRunning) {
        pthread_mutex_lock(&data_mutex);
        bRunning = false;
        pthread_mutex_unlock(&data_mutex);        
    }
}

void* DataGen::run(void * s) {
    DataGen * dg = (DataGen *) s;
    while (dg->bRunning) {
        // TODO
    }
    return NULL;
}
