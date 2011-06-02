#include "datagen.h"
#include "debugbox.h"

#include <pthread.h>
#include <sys/time.h>    
#include <signal.h>

extern "C" {
#include <ccn/ccn.h>
#include <ccn/bloom.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#include <ccn/schedule.h>
}

#ifdef DEBUG
#include <iostream>
#endif

#define JOIN_INTEST_LEN 7
#define CCNTIMEOUT 5
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static DataGen * st_dg = NULL;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t splist_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timeval dg_start, dg_now;

DataGen::DataGen() {
    bitRate = 64;               // kbps
    sampleRate = 50;            // Hz
    bRunning = false;
    owner = false;
    ttl = 1;                    // sec
    spList_ttl = 100;           // times that run() is called
    mySeq = 0;
    rpSeq = 0;
    opSeq = 0;
    outSeq = 0;
    confName = "";
    confPrefix = "";
    speakName = "";
    m_dump = new Dump();
    st_dg = this;
}

DataGen::DataGen(string t_confName, string t_speakName) {
    bitRate = 64;               // kbps
    sampleRate = 50;            // Hz
    bRunning = false;
    owner = false;
    ttl = 1;                    // sec
    spList_ttl = 100;           // times that run() is called
    mySeq = 0;
    rpSeq = 0;
    opSeq = 0;
    outSeq = 0;
    confName = t_confName;
    confPrefix = "/ndn/broadcast/jiwen/" + confName;
    speakName = t_speakName;
    m_dump = new Dump();
    st_dg = this;
}

DataGen::~DataGen() {
    if (bRunning) {
        stopThread();
    }
    st_dg = NULL;
    if (signed_info) {
        ccn_charbuf_destroy(&signed_info);
    }
    if (keystore) {
        ccn_keystore_destroy(&keystore);
    }
    if (m_dump) {
        delete m_dump;
        debug("Dump released\n");
        m_dump = NULL;
    }
    debug("Bye Bye DataGen\n");
}

void DataGen::ccnConnect() {
    
    debug("ccnConnect");
    ccn = ccn_create();
    if (ccn == NULL || ccn_connect(ccn, NULL) == -1) {
		string qs = "Failed to initialize ccn agent connection";
		critical(qs);
    }

    if (speakName == "1") {
        myPrefix = confPrefix + "/1";
        opPrefix = confPrefix + "/2";
    } else if (speakName == "2") {
        myPrefix = confPrefix + "/2";
        opPrefix = confPrefix + "/1";
    } else {
        critical("unknow speak" + speakName);
    }
    
    initKeystoreAndSignedInfo();

    // incoming content
    dg_content.p = &DataGen::incoming_content;
    // incoming speak list
    dg_join_con.p = &DataGen::incoming_spList;

    if (owner) {
        // Add my name into speak list
        speakList.push_back(speakName);

        // incoming join interest
        struct ccn_charbuf *interest_nm = ccn_charbuf_create();
        if (interest_nm == NULL) {
            string qs = "Failed to allocate or initialize interest filter path";
            critical(qs);
        }
        ccn_name_from_uri(interest_nm, (confPrefix + "/join").c_str());
#ifdef DEBUG
        cout << "I am onwer: " << confPrefix << endl;
#endif
        dg_join_int.p = &DataGen::incoming_join_interest;
        ccn_set_interest_filter(ccn, interest_nm, &dg_join_int);
        ccn_charbuf_destroy(&interest_nm);
    }
}

void DataGen::ccnDisconnect() {
	if (ccn != NULL) {
		ccn_disconnect(ccn);
		ccn_destroy(&ccn);
	}
}

void DataGen::initKeystoreAndSignedInfo() {
	// prepare for ccnx
	keystore = NULL;
	ccn_charbuf *temp = ccn_charbuf_create();
	keystore = ccn_keystore_create();
	ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
	int res = ccn_keystore_init(keystore,
				ccn_charbuf_as_string(temp),
				(char *)"Th1s1sn0t8g00dp8ssw0rd.");
	if (res != 0) {
	    printf("Failed to initialize keystore %s\n", ccn_charbuf_as_string(temp));
	    exit(1);
	}
	
	struct ccn_charbuf *keylocator = ccn_charbuf_create();
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_KeyLocator, CCN_DTAG);
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_Key, CCN_DTAG);
	res = ccn_append_pubkey_blob(keylocator, ccn_keystore_public_key(keystore));
	if (res < 0) {
		ccn_charbuf_destroy(&keylocator);
	} else {
		ccn_charbuf_append_closer(keylocator);
		ccn_charbuf_append_closer(keylocator);
	}

	signed_info = NULL;
	signed_info = ccn_charbuf_create();
	res = ccn_signed_info_create(signed_info,
                                 ccn_keystore_public_key_digest(keystore),
                                 ccn_keystore_public_key_digest_length(keystore),
                                 NULL,
                                 CCN_CONTENT_DATA,
                                 ttl, 
                                 NULL,
                                 keylocator);
	if (res < 0) {
		critical("Failed to create signed_info");
	}

	ccn_charbuf_destroy(&temp);
    ccn_charbuf_destroy(&keylocator);
}

void DataGen::generateData() {

    pthread_mutex_lock(&data_mutex);

    struct ccn_charbuf *temp = NULL;
    struct ccn_charbuf *interest_nm = NULL;

    temp = ccn_charbuf_create();
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
                                   signed_info,
                                   buf,
                                   bsize,
                                   NULL,
                                   ccn_keystore_private_key(keystore));
	if (res < 0) {
		critical("generate content failed!");
	}

    //debug("generateData 6");
    res = ccn_put(ccn, temp->buf, temp->length);
#ifdef DEBUG
    if (mySeq % 50 == 0) {
        cout << "put content seq: " << mySeq << endl;
    }
#endif
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

    pthread_mutex_unlock(&data_mutex);
}

void DataGen::expressInterest() {

    pthread_mutex_lock(&data_mutex);
    
    struct ccn_charbuf *temp = NULL;
    struct ccn_charbuf *interest_nm = NULL;
    int res, t_cnt;
    
    temp = ccn_charbuf_create();
    interest_nm = ccn_charbuf_create();

    // Pre-send interests, but no more than 10 interest in 20ms
    for(t_cnt = 0; t_cnt++ < 10 && outSeq < opSeq + sampleRate; outSeq++) {
        // Generate speaker's prefix
        interest_nm->length = 0;
        ccn_name_from_uri(interest_nm, opPrefix.c_str());
        // Append sequence number
        temp->length = 0;
        ccn_charbuf_putf(temp, "%d", outSeq);
#ifdef DEBUG
        if (outSeq % 50 == 0) {
            cout << "Interest: outSeq: " << outSeq << "\topSeq: " << opSeq << endl;
        }
#endif
        ccn_name_append(interest_nm, temp->buf, temp->length);
        temp->length = 0;

        res = ccn_express_interest(ccn, interest_nm, &dg_content, NULL); 
        if (res < 0) {
            critical("express interest failed!");
        }
    }

    ccn_charbuf_destroy(&temp);
    ccn_charbuf_destroy(&interest_nm);
    pthread_mutex_unlock(&data_mutex);    
}

void DataGen::refreshSpList() {
    struct ccn_charbuf *temp = NULL;
    struct ccn_charbuf *interest_nm = NULL;
    
    temp = ccn_charbuf_create();
    interest_nm = ccn_charbuf_create();

    // Generate speaker's prefix
    interest_nm->length = 0;
    ccn_name_from_uri(interest_nm, (confPrefix + "/join/" + speakName).c_str());
#ifdef DEBUG
    debug(confPrefix + "/join/" + speakName);
#endif
    int res = ccn_express_interest(ccn, interest_nm, &dg_join_con, NULL); 
    if (res < 0) {
        critical("express interest failed!");
    }
    
    ccn_charbuf_destroy(&temp);
    ccn_charbuf_destroy(&interest_nm);
}

string DataGen::speakListToXml() {
    string temp = "";
    temp += "<speakList>";
    list<string>::iterator its;
    for (its = speakList.begin(); its != speakList.end(); its++) {
        temp += "<speak>";
        temp += *its;
        temp += "</speak>";
    }
    temp += "</speakList>";
    return temp;
}

void DataGen::loadSpeakList(string t_xml) {
}

void DataGen::handleContent(struct ccn_upcall_info *info) {
    // cout << string((const char*)info->interest_ccnb) << endl;
    uint32_t seq;
    size_t seq_size = 0;
    struct ccn_indexbuf *comps = info->content_comps;
    const unsigned char *ccnb = info->content_ccnb;
    const unsigned char *seqptr = NULL;
    int k = comps->n - 2;
    seq = ccn_ref_tagged_BLOB(CCN_DTAG_Component, ccnb,
                              comps->buf[k], comps->buf[k + 1],
                              &seqptr, &seq_size);
    if (seq >= 0) {
        seq = (uint32_t) atoi((const char*)seqptr);
        opSeq = MAX(opSeq, seq);
#ifdef DEBUG
        if (seq % 50 == 0) {
            cout << "Content: Seq: " << seq << endl;
        }
#endif
    }
}

void DataGen::handleJoinInterest(struct ccn_upcall_info *info) {
    struct ccn_indexbuf *comps = info->interest_comps;
    const unsigned char *ccnb = info->interest_ccnb;
    const unsigned char *speakName = NULL;
    size_t sn_size = 0;
    if (comps->n != JOIN_INTEST_LEN) {
        debug("handleJoinInterest: wrong format");
    }
    int k = comps->n - 2;
    int r = ccn_ref_tagged_BLOB(CCN_DTAG_Component, ccnb,
                                comps->buf[k], comps->buf[k + 1],
                                &speakName, &sn_size);
    if (r >= 0) {
        string t_spName = string((const char*)speakName);
        speakList.push_back(t_spName);
        speakList.unique();
        string t_spListXml = speakListToXml();
#ifdef DEBUG
        debug("new speaker: " + t_spName);
#endif
        // Reply content
		struct ccn_charbuf *t_name = NULL;
		struct ccn_charbuf *t_content = NULL;
		t_name = ccn_charbuf_create();
		t_content = ccn_charbuf_create();
        // TODO can we just reply to name in info???
        ccn_name_from_uri(t_name, (confPrefix + "/join/" + t_spName).c_str());
		int res = ccn_encode_ContentObject(t_content, t_name, signed_info,
                                           t_spListXml.c_str(),
                                           t_spListXml.size(),
                                           NULL,
                                           ccn_keystore_private_key(keystore));
		if (res)
			critical("failed to create content");
		ccn_put(info->h, t_content->buf, t_content->length);

		ccn_charbuf_destroy(&t_name);
		ccn_charbuf_destroy(&t_content);
    }
}

void DataGen::handleSpList(struct ccn_upcall_info *info) {
    debug("handleSpList");
    const unsigned char *value = NULL;
    size_t len = 0;
    int res =ccn_content_get_value(info->content_ccnb,
                                   info->pco->offset[CCN_PCO_E],
                                   info->pco, &value, &len);
    if (res < 0)
        critical("failed to parse content object");

    string str_xml = string((const char*)value);
    debug(str_xml);
}

void DataGen::startThread() {
    if (!bRunning) {
        pthread_mutex_lock(&flag_mutex);
        bRunning = true;
        pthread_mutex_unlock(&flag_mutex);

        ccnConnect();
        // ready to go
        pthread_t ccn_thread;
        pthread_create(&ccn_thread, NULL, &run, (void *) this);
    }
}

void DataGen::stopThread() {
    if (bRunning) {
        pthread_mutex_lock(&flag_mutex);
        bRunning = false;
        pthread_mutex_unlock(&flag_mutex);        

        ccnDisconnect();
    }
}

void* DataGen::run(void * s) {

    DataGen * dg = (DataGen *) s;

    struct itimerval tout_val;
    tout_val.it_interval.tv_sec = 0;
    tout_val.it_interval.tv_usec = 1000000 / dg->sampleRate;
    tout_val.it_value.tv_sec = 0;
    tout_val.it_value.tv_usec = 1000000 / dg->sampleRate;
    setitimer(ITIMER_REAL, &tout_val, 0);
    signal(SIGALRM, dg_timeout);

#ifdef DEBUG
    gettimeofday(&dg_start, NULL);
    debug("DataGen::run");
#endif

    int res = 0;
    int run_cnt = 0;
    while (dg->bRunning) {
        // if (res >= 0) {
        if (!dg->owner && run_cnt++ == dg->spList_ttl) {
            dg->refreshSpList();
            run_cnt = 0;
        }
        if (dg->ccn == NULL) {
            critical("invalid ccnd");
        }
        dg->expressInterest();
        res = ccn_run(dg->ccn, CCNTIMEOUT);
        if (res < 0) {
            cout << "ccn_run fails: erron=" << res << endl;
        }
    }
    debug("leave DataGen::run");
    return NULL;
}

enum ccn_upcall_res DataGen::incoming_join_interest(
        struct ccn_closure *selfp,
        enum ccn_upcall_kind kind,
        struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST: {
		debug("incoming join interest");
		// TODO gsd->handleEnumInterest(info);
        st_dg->handleJoinInterest(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
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
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
        st_dg->handleContent(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

enum ccn_upcall_res DataGen::incoming_spList(
        struct ccn_closure *selfp,
        enum ccn_upcall_kind kind,
        struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);

	case CCN_UPCALL_CONTENT: {
		debug("incoming join content");
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("incoming join content");
        st_dg->handleContent(info);
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

#ifdef DEBUG
    if (st_dg->mySeq % 50 == 0) {
        gettimeofday(&dg_now, NULL);
        int seconds  = dg_now.tv_sec  - dg_start.tv_sec;
        int useconds = dg_now.tv_usec - dg_start.tv_usec;
        int mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
        cout << "DataGen " << st_dg->mySeq / 50
             << " elapsed time: " << mtime << " milliseconds" << endl;
    }
#endif
    
    st_dg->generateData();
//    st_dg->expressInterest(); // Now express in run loop
}
