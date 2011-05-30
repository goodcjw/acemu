#include "datagen.h"
#include "debugbox.h"

extern "C" {
#include <pthread.h>
#include <sys/time.h>    
}

#ifdef DEBUG
#include <iostream>
#endif

static DataGen * st_dg = NULL;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

DataGen::DataGen() {
    bitRate = 64;               // kbps
    sampleRate = 50;            // Hz
    bRunning = false;
    mySeq = 0;
    rpSeq = 0;
    opSeq = 0;
    outSeq = 0;
    confName = "";
    speakName = "";
    st_dg = this;
}

DataGen::DataGen(string t_confName, string t_speakName) {
    bitRate = 64;               // kbps
    sampleRate = 50;            // Hz
    bRunning = false;
    mySeq = 0;
    rpSeq = 0;
    opSeq = 0;
    outSeq = 0;
    confName = t_confName;
    speakName = t_speakName;
    st_dg = this;
}

DataGen::~DataGen() {
    if (bRunning) {
        stopThread();
    }
    st_dg = NULL;
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

void DataGen::generateData() {

    struct ccn_charbuf *temp = NULL;
    temp = ccn_charbuf_create();
    struct ccn_charbuf *interest_nm = NULL;
    interest_nm = ccn_charbuf_create();

    char * buf = NULL;
    int bsize, res;

    // Generate speaker's prefix
    interest_nm->length = 0;
    ccn_name_from_uri(interest_nm, myPrefix.c_str());

    // Append sequence number
    temp->length = 0;
    ccn_charbuf_putf(temp, "%d", mySeq);
    ccn_name_append(interest_nm, temp->buf, temp->length);

    // Generate some minic audio data
    bsize = bitRate * 1024 / sampleRate / 8;
    buf = (char*) calloc(bsize, sizeof(char));

    temp->length = 0;
    res = ccn_encode_ContentObject(temp,
                                   interest_nm,
                                   NULL,
                                   buf,
                                   bsize,
                                   NULL,
                                   NULL);
    
    res = ccn_put(ccn, temp->buf, temp->length);
    // TODO resolve res

    ccn_charbuf_destroy(&interest_nm);
    ccn_charbuf_destroy(&temp);
    if (buf) {
        free(buf);
    }
    // Assume that data with sequence number smaller than mySeq
    // is considered as generated
    mySeq++;
}

void DataGen::expressInterest() {
    struct ccn_charbuf *temp = NULL;
    temp = ccn_charbuf_create();
    struct ccn_charbuf *interest_nm = NULL;
    int res;
    
    // Pre-send 50 interests
    for(; outSeq < opSeq + sampleRate; outSeq++) {
        // Generate speaker's prefix
        interest_nm->length = 0;
        ccn_name_from_uri(interest_nm, opPrefix.c_str());
        // Append sequence number
        temp->length = 0;
        ccn_charbuf_putf(temp, "%d", outSeq);
        ccn_name_append(interest_nm, temp->buf, temp->length);
        temp->length = 0;

        res = ccn_express_interest(ccn, interest_nm, &dg_content, NULL); 
    }
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

    struct itimerval tout_val;
    tout_val.it_interval.tv_sec = 0;
    tout_val.it_interval.tv_usec = 0;
    tout_val.it_value.tv_sec = 0;
    tout_val.it_value.tv_usec = 1000 / dg->sampleRate;
    setitimer(ITIMER_REAL, &tout_val, 0);
    signal(SIGALRM, dg_timeout);

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

void DataGen::dg_timeout(int param) {
    if (st_dg == NULL) {
        critical("dg_timeout: DataGen not ready");
    }

    struct itimerval tout_val;
    tout_val.it_interval.tv_sec = 0;
    tout_val.it_interval.tv_usec = 0;
    tout_val.it_value.tv_sec = 0;
    tout_val.it_value.tv_usec = 1000 / st_dg->sampleRate;
    st_dg->generateData();
    st_dg->expressInterest();

    setitimer(ITIMER_REAL, &tout_val, 0);
    signal(SIGALRM, DataGen::dg_timeout);    
}
