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
    ttl = 10;                   // sec
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
    ttl = 10;                   // sec
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
    
    debug("ccnConnect");
    ccn = ccn_create();
    if (ccn == NULL || ccn_connect(ccn, NULL) == -1) {
		string qs = "Failed to initialize ccn agent connection";
		critical(qs);
    }

    if (speakName == "1") {
        myPrefix = "/ndn/broadcast/acemu/" + confName + "/1";
        opPrefix = "/ndn/broadcast/acemu/" + confName + "/2";
    } else if (speakName == "2") {
        myPrefix = "/ndn/broadcast/acemu/" + confName + "/2";
        opPrefix = "/ndn/broadcast/acemu/" + confName + "/1";
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
#ifdef DEBUG
    cout << "I have: " << myPrefix << endl;
#endif
    ccn_charbuf_destroy(&interest_nm);

    // incoming content
    dg_content.p = &DataGen::incoming_content;
}

void DataGen::ccnDisconnect() {
	if (ccn != NULL) {
		ccn_disconnect(ccn);
		ccn_destroy(&ccn);
	}
}

void DataGen::generateData() {

    struct ccn_charbuf *temp = NULL;
    struct ccn_charbuf *interest_nm = NULL;
    struct ccn_charbuf *signed_info = NULL;
    struct ccn_keystore *keystore = NULL;

    temp = ccn_charbuf_create();
    interest_nm = ccn_charbuf_create();
    signed_info = ccn_charbuf_create();
    keystore = ccn_keystore_create();

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

    // Deal with key
    temp->length = 0;
    ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
    res = ccn_keystore_init(keystore,
                            ccn_charbuf_as_string(temp),
                            (char *) "Th1s1sn0t8g00dp8ssw0rd.");
    
    signed_info->length = 0;
    res = ccn_signed_info_create(signed_info,
                                 ccn_keystore_public_key_digest(keystore),
                                 ccn_keystore_public_key_digest_length(keystore),
                                 NULL,
                                 CCN_CONTENT_DATA,
                                 -1,
                                 NULL,
                                 NULL);
    
    temp->length = 0;
    res = ccn_encode_ContentObject(temp,
                                   interest_nm,
                                   signed_info,
                                   buf,
                                   bsize,
                                   NULL,
                                   ccn_keystore_private_key(keystore));
	if (res < 0) {
		critical("generate content failed!");
	}

    res = ccn_put(ccn, temp->buf, temp->length);
    // TODO resolve res
	if (res < 0) {
		critical("put content failed!");
	}

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
    struct ccn_charbuf *interest_nm = NULL;
    int res;
    
    temp = ccn_charbuf_create();
    interest_nm = ccn_charbuf_create();

    // Pre-send 50 interests
    for(; outSeq < opSeq + sampleRate; outSeq++) {
        // Generate speaker's prefix
        interest_nm->length = 0;
        ccn_name_from_uri(interest_nm, opPrefix.c_str());
        // Append sequence number
        temp->length = 0;
        ccn_charbuf_putf(temp, "%d", outSeq);
        debug(opPrefix + "/" + string(ccn_charbuf_as_string(temp)));
#ifdef DEBUG
        cout << "outSeq: " << outSeq << "\topSeq: " << opSeq << endl;
#endif
        ccn_name_append(interest_nm, temp->buf, temp->length);
        temp->length = 0;

        res = ccn_express_interest(ccn, interest_nm, &dg_content, NULL); 
        if (res < 0) {
            critical("express interest failed!");
        }
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

#ifdef DEBUG
    int cnt = 0;
    struct timeval start, end;
    gettimeofday(&start, NULL);
    debug("DataGen::run");
#endif

    int res = 0;
    while (dg->bRunning) {
        // if (res >= 0) {
        if (true) {
            if (dg->ccn == NULL) {
                critical("invalid ccnd");
            }
            res = ccn_run(dg->ccn, -1);
            if (res < 0) {
                cout << "ccn_run fails: erron=" << res << endl;
            }
#ifdef DEBUG
            gettimeofday(&end, NULL);  
            int seconds  = end.tv_sec  - start.tv_sec;
            int useconds = end.tv_usec - start.tv_usec;
            int mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
            cnt++;
            if (cnt % 100 == 0) {
                cout << "DataGen " << cnt / 100
                     << " elapsed time: " << mtime << " milliseconds" << endl;
            }
#endif
        }
    }
    debug("leave DataGen::run");
    return NULL;
}

enum ccn_upcall_res DataGen::incoming_content(
        struct ccn_closure *selfp,
        enum ccn_upcall_kind kind,
        struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);

	case CCN_UPCALL_CONTENT: {
		debug("incoming public content");
		// gsd->handleEnumContent(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("unverified content!");
		return (CCN_UPCALL_RESULT_OK);
	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
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
