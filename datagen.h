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
    string confName;
    string speakName;
    string myPrefix;
    string opPrefix;

	struct ccn *ccn;
	struct ccn_closure dg_interest;
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

    void ccnConnect();
    void ccnDisconnect();
    static enum ccn_upcall_res
    incoming_interest(struct ccn_closure *selfp,
                      enum ccn_upcall_kind kind,
                      struct ccn_upcall_info *info);
    static enum ccn_upcall_res
    incoming_content(struct ccn_closure *selfp,
                     enum ccn_upcall_kind kind,
                     struct ccn_upcall_info *info);
};

//static void* run(void * dg);

#endif // DATAGEN_H
