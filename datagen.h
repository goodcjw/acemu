#ifndef DATAGEN_H
#define DATAGEN_H

extern "C" {
#include <ccn/ccn.h>
#include <ccn/bloom.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#include <ccn/schedule.h>
}

#include <string>
using namespace std;

class DataGen {

private:
    int bitRate;
    int sampleRate;
    bool bRunning;
    int ttl;
    uint32_t mySeq;     // Data we have generated
    uint32_t rpSeq;     // Interest we have replied, no use if put
    uint32_t opSeq;     // Content we have received
    uint32_t outSeq;    // Interest we have expressed

    string confName;
    string speakName;
    string myPrefix;
    string opPrefix;

	struct ccn *ccn;
	struct ccn_closure dg_content;

public:
    DataGen();
    DataGen(string t_confName, string t_speakName);
    ~DataGen();
    bool isRunning() { return bRunning; }

    void startThread();
    void stopThread();    

    void setConfName(string t_confName) {confName = t_confName;}
    void setSpeakName(string t_speakName) {speakName = t_speakName;}

private:
    static void* run(void * dg);
    static void dg_timeout(int param);

    void ccnConnect();
    void ccnDisconnect();
    void generateData();
    void expressInterest();

    void handleContent(struct ccn_upcall_info *info);

    static enum ccn_upcall_res
    incoming_content(struct ccn_closure *selfp,
                     enum ccn_upcall_kind kind,
                     struct ccn_upcall_info *info);
};

#endif // DATAGEN_H
