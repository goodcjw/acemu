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
#include <list>
#include <map>
using namespace std;

#include "dump.h"

class DataGen {

private:
    int bitRate;
    int sampleRate;
    bool bRunning;
    bool owner;
    int ttl;
    int spList_ttl;
    uint32_t mySeq;     // Data we have generated

    string confName;
    string confPrefix;
    string speakName;   // My name
    string opPrefix;

	struct ccn *ccn;
	struct ccn_closure dg_content;
    struct ccn_closure dg_join_int;
    struct ccn_closure dg_join_con;
	struct ccn_keystore *keystore;
	struct ccn_charbuf *signed_info;
    Dump * m_dump;

    list<string> speakList;
    map<string, uint32_t> opSeqs;   // Content we have received
    map<string, uint32_t> outSeqs;  // Interest we have expressed

public:
    DataGen();
    DataGen(string t_confName, string t_speakName);
    ~DataGen();
    bool isRunning() { return bRunning; }

    void startThread();
    void stopThread();    

    void setConfName(string t_confName) {
        confName = t_confName;
        confPrefix = "/ndn/broadcast/jiwen/" + confName;
    }
    void setSpeakName(string t_speakName) {speakName = t_speakName;}
    void setOwner(bool isOwner) {owner = isOwner;}

private:
    static void* run(void * dg);
    static void dg_timeout(int param);

    void ccnConnect();
    void ccnDisconnect();
    void initKeystoreAndSignedInfo();
    void generateData();
    void expressInterest();
    void refreshSpList();
    
    string speakListToXml();
    void loadSpeakList(string t_xml);
    void updateSeqs();

    void handleContent(struct ccn_upcall_info *info);
    void handleJoinInterest(struct ccn_upcall_info *info);
    void handleSpList(struct ccn_upcall_info *info);

    static enum ccn_upcall_res
    incoming_content(struct ccn_closure *selfp,
                     enum ccn_upcall_kind kind,
                     struct ccn_upcall_info *info);
    static enum ccn_upcall_res
    incoming_join_interest(struct ccn_closure *selfp,
                     enum ccn_upcall_kind kind,
                     struct ccn_upcall_info *info);
    static enum ccn_upcall_res
    incoming_spList(struct ccn_closure *selfp,
                    enum ccn_upcall_kind kind,
                    struct ccn_upcall_info *info);
};

#endif // DATAGEN_H
