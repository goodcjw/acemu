#ifndef CONFWIZARD_H
#define CONFWIZARD_H

#include "announcement.h"

class ConfWizard {
private:
    Announcement *a;
    ConfWizard();
public:
    ConfWizard(const char * const filename);
    ~ConfWizard();
	Announcement *getAnnouncement();
};

#endif // CONFWIZARD_H
