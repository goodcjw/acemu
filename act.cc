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
    }
    if (dg) {
        delete dg;
    }
}

/*
 * Read the script and run according to it
 */
void Act::run() {
    test_run();
}

void Act::test_run() {
    se->startThread();
    newConference();
    listPubConferences();
    joinConference("acenum");

    /* Do something and stop tread */
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

void Act::joinConference(string confName) {
    debug("joinConference: " + confName);
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
    for (it = lista.begin(); it != lista.end(); it++) {
        Announcement *a = *it;
        cout << "Conf: " << a->getConfName()
             << "\tOrg: "  << a->getOrganizer()
             << "\temail: "<< a->getEmail() << endl;
    }
}

void Act::listPriConferences() {
    list<Announcement *> lista = se->listAllPriConference();
    list<Announcement *>::iterator it;
    for (it = lista.begin(); it != lista.end(); it++) {
        Announcement *a = *it;
        cout << "Conf: " << a->getConfName()
             << "\tOrg: "  << a->getOrganizer()
             << "\temail: "<< a->getEmail() << endl;
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
    debug("leave test_announcement");
}
