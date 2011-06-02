#include "act.h"
#include "debugbox.h"

#include <iostream>
using namespace std;

#define DEFAULT_PREFIX ("/ndn/broadcast/conference")

static void test_announcement(Announcement * a);

Act::Act(string sc_path) {
    script_path = sc_path;
    se = new SessionEnum(DEFAULT_PREFIX);
    dg = new DataGen();    
}

Act::~Act() {
    if (se) {
        delete se;
        se = NULL;
    }
    debug("SessionEmu released\n");
    if (dg) {
        delete dg;
        dg = NULL;
    }
    debug("DataGen released\n");
    debug("\nBye Bye Act\n");
}

/*
 * Read the script and run according to it
 */
void Act::run() {
    test_run();
}

void Act::test_run() {
    se->startThread();
    sleep(5);
    newConference();
    listPubConferences();
    string confName = "acenum";
    Announcement * a = findAnnouncementByName(confName);
    if (a == NULL) {
        debug("No conference find: " + confName);
        return;
    }
    joinConference(a);

    /* Do something and stop tread */
    while(true) {}
    sleep(60);
    quitConference();
    se->stopThread();
    debug("ccn thread killed");
    sleep(10);
}

void Act::newConference() {
    debug("newConference");
	Announcement *a = new Announcement();
    ConfWizard cw("conf.xml");
    if (cw.getAnnouncement() == NULL) {
        return;
    }
    a->copy(cw.getAnnouncement());
    se->addToMyConferences(a);
    test_announcement(a);
}

Announcement * Act::findAnnouncementByName(string confName) {
    list<Announcement *> listpk = se->listAllPubConference();
    list<Announcement *>::iterator it;
    debug("findAnnouncementByName: " + confName);
    for (it = listpk.begin(); it != listpk.end(); it++) {
        Announcement *a = *it;
        if (a->getConfName() == confName) {
            return a;
        }
    }
    // TODO deal with private conferences
    return NULL;
}

void Act::joinConference(Announcement* a) {
    list<Announcement *> lista = se->listAllPubConference();
    string confName = a->getConfName();
    string speakName;
    if (lista.size() == 1) {
        speakName = "1";
    }
    else if (lista.size() == 2) {
        speakName = "2";
    }
    else {
        debug("joinConference bug: wrong lista.size()");
    }
    debug("joinConference: /acemu/" + confName + "/" + speakName);
	// should be indicated from current item in the future
	bool audio = true;
	bool video = false;
	if (video) {
		// no video yet; do nothing
	}
	if (audio) {
        if (dg->isRunning()) {
            dg->stopThread();
            delete dg;
            dg = new DataGen();
        }
        dg->setConfName(confName);
        dg->setSpeakName(speakName);
        if (a->getOwner()) {
            dg->setOwner(true);
        }
        dg->startThread();
    }
}

void Act::quitConference() {
    debug("quitConference");
    if (dg->isRunning()) {
        dg->stopThread();
    }
}

void Act::listPubConferences() {
    list<Announcement *> lista = se->listAllPubConference();
    list<Announcement *>::iterator it;
    debug("listPubConferences");
    cout << lista.size() << " public conferences" << endl;
    for (it = lista.begin(); it != lista.end(); it++) {
        Announcement *a = *it;
        cout << "Conf: " << a->getConfName()
             << "\tOrg: "  << a->getOrganizer()
             << "\temail: "<< a->getEmail();
        if (a->getOwner()) {
            cout << "\t*";
        }
        cout << endl;
    }
}

void Act::listPriConferences() {
    list<Announcement *> lista = se->listAllPriConference();
    list<Announcement *>::iterator it;
    for (it = lista.begin(); it != lista.end(); it++) {
        Announcement *a = *it;
        cout << "Conf: " << a->getConfName()
             << "\tOrg: "  << a->getOrganizer()
             << "\temail: "<< a->getEmail();
        if (a->getOwner()) {
            cout << "\t*";
        }
        cout << endl;
    }
}

/*
 * Unit tests
 */
static void test_announcement(Announcement * a) {
    debug("test_announcement");
    debug(a->getConfName());
    debug(a->getOrganizer());
    debug(a->getEmail());
    debug(a->getDesc());
    debug(a->getDate());
    debug(a->getTime());
}
