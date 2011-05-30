#include "datagen.h"
#include "debugbox.h"

extern "C" {
#include <pthread.h>    
}

static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

DataGen::DataGen() {
    bitRate = 64;               // kbps
    sampleRate = 50;            // Hz
    bRunning = false;
    confName = "";
    speakName = "";    
}

DataGen::DataGen(string t_confName, string t_speakName) {
    bitRate = 64;               // kbps
    sampleRate = 50;            // Hz
    bRunning = false;
    confName = t_confName;
    speakName = t_speakName;
}

DataGen::~DataGen() {
    if (bRunning) {
        stopThread();
    }
}

void DataGen::ccnConnect() {
    ccn = ccn_create();

    if (speakName == "1") {
        myPrefix = "/acemu/" + confName + "/1";
        opPrefix = "/acemu/" + confName + "/2";
    } else if (speakName == "2") {
        myPrefix = "/acemu/" + confName + "/2";
        opPrefix = "/acemu/" + confName + "/1";
    } else {
        critical("unknow speak");
    }

    // incoming interest
    struct ccn_charbuf *interest_nm = ccn_charbuf_create();
    if (interest_nm == NULL) {
        string qs = "Failed to allocate or initialize interest filter path";
		critical(qs);
    }
	ccn_name_from_uri(interest_nm, myPrefix.c_str());
    dg_interest.p = &DataGen::incoming_interest;
    ccn_set_interest_filter(ccn, interest_nm, &dg_interest);
    ccn_charbuf_destroy(&interest_nm);

    // incoming content
    dg_content.p = &DataGen::incoming_content;
}

void DataGen::ccnDisconnect() {
    ccn_destroy(&ccn);
    ccn = NULL;
}

void DataGen::startThread() {
    if (!bRunning) {
        pthread_mutex_lock(&data_mutex);
        bRunning = true;
        pthread_mutex_unlock(&data_mutex);

        ccnConnect();
        // ready to go
        pthread_t ccn_thread;
        pthread_create(&ccn_thread, NULL, &run, (void *) this);
    }
}

void DataGen::stopThread() {
    if (bRunning) {
        pthread_mutex_lock(&data_mutex);
        bRunning = false;
        pthread_mutex_unlock(&data_mutex);        

        ccnDisconnect();
    }
}

void* DataGen::run(void * s) {
    DataGen * dg = (DataGen *) s;
    int res = 0;
    while (dg->bRunning) {
        if (res >= 0) {
            res = ccn_run(dg->ccn, 5);
        }
    }
    return NULL;
}

enum ccn_upcall_res DataGen::incoming_interest(
        struct ccn_closure *selfp,
        enum ccn_upcall_kind kind,
        struct ccn_upcall_info *info) {
    return(CCN_UPCALL_RESULT_OK);    
}

enum ccn_upcall_res DataGen::incoming_content(
        struct ccn_closure *selfp,
        enum ccn_upcall_kind kind,
        struct ccn_upcall_info *info) {
    return(CCN_UPCALL_RESULT_OK);
}
