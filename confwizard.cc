#include <iostream>
#include <string>
using namespace std;

#include "debugbox.h"
#include "tinyxml.h"
#include "base64.h"
#include "confwizard.h"

extern "C" {
#include <openssl/rand.h>
}

ConfWizard::ConfWizard(const char * const filename) {
#ifdef DEBUG
    debug("ConfWizard: " + string(filename));
#endif
    TiXmlDocument xmldoc(filename);
    xmldoc.LoadFile();
    TiXmlElement* root = xmldoc.FirstChildElement();    // <conference>
    if (!root) {
        return;
        critical("ConfWizard: unable to load conf.xml");
    }
    a = new Announcement();
	a->setOwner(true);
    a->setAudio(true);

    TiXmlNode* node = root->FirstChild();
    TiXmlElement* elem = root->FirstChildElement();

    while (node) {
        string attr = node->ValueStr();
        string value = string(elem->GetText());
        if (attr == "confName") {
        	a->setConfName(value);
        } else if (attr == "organizer") {
            a->setOrganizer(value);
        } else if (attr == "email") {
        	a->setEmail(value);    
        } else if (attr == "video") {
            if (value == "true" || value == "True") {
                a->setVideo(true);
            } else {
                a->setVideo(false);
            }
        } else if (attr == "confDesc") {
        	a->setDesc(value);
        } else if (attr == "date") {
        	a->setDate(value);
        } else if (attr == "time") {
        	a->setTime(value);    
        } else if (attr == "hours") {
            a->setHours(atoi(value.c_str()));
        } else if (attr == "minutes") {
            a->setMinutes(atoi(value.c_str()));
        } else if (attr == "private") {
            if (value == "true" || value == "True") {
                a->setIsPrivate(true);

                unsigned char bytes[32];
                RAND_bytes(bytes, 32);
                char * qba = base64((unsigned char *) bytes, int(32));
                string opaqueName(qba);
                free(qba);
                a->setOpaqueName(opaqueName);

                a->initConferenceKey();
                a->initAudioSessionKey();

                /*
                 * TODO
            	 * QSettings settings("UCLA-IRL", "ACTD");
                 * QString qsCerts = settings.value("qsCerts").toString();
                 * settings.setValue("qsCerts", "");
                 * QStringList certs = qsCerts.split(":");
                 * a->setCerts(certs);
                 */
                list<string> certs;
                a->setCerts(certs);
            } else {
                a->setIsPrivate(false);
            }
        }

        node = node->NextSibling();
        elem = elem->NextSiblingElement();        
    }
}

ConfWizard::~ConfWizard() {
    delete a;
}

Announcement *ConfWizard::getAnnouncement() {
	return a;
}
