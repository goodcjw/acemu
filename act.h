#ifndef ACT_H
#define ACT_H

#include "sessionenum.h"
#include "datagen.h"
#include "tinyxml.h"
#include "confwizard.h"

class Act {
private:
    string script_path;
    SessionEnum * se;
    DataGen * dg;

private:
    Act();          // prevent users create an Act object
    
    void newConference();
    void dismissConference();
    void joinConference(string confName);
    void quitConference();
    void listPubConferences();
    void listPriConferences();
    void leaveConference();

    void test_run();

public:
    Act(string sc_path);
    ~Act();
    void run();
};

#endif // ACT_H
