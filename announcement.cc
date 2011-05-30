#include "announcement.h"
#include "debugbox.h"
#include "tinyxml.h"
#include "base64.h"

#include <sstream>
#include <stdlib.h>

using namespace std;

#define REFRESH_INTERVAL 30 
#define REMOVE_INTERVAL (2 * REFRESH_INTERVAL + 5) 

Announcement::Announcement()
{
	confName = string("");
	organizer = string("");
	email = string("");
	own = false;
	audio = false;
	video = false;
    isPrivate = false;
	desc = string("");
	uuid = string("");
	memset(digest, 0, SHA_DIGEST_LENGTH);
	memset(conferenceKey, 0, KEY_LENGTH);
	memset(audioSessionKey, 0, KEY_LENGTH);
}

void Announcement::setDigest(unsigned char *hash) {
	if (hash == NULL)
		return;
	
	memcpy(digest, hash, SHA_DIGEST_LENGTH);
    char * c_ds = base64((unsigned char *)digest, SHA_DIGEST_LENGTH);
    char * c_hs = base64((unsigned char *)hash, SHA_DIGEST_LENGTH);
    string ds(c_ds);
    string hs(c_hs);
	debug("digest is " + ds);
	debug("hash is " + hs);
    free(c_ds);
    free(c_hs);
}

bool Announcement::equalDigest(unsigned char *hash) {

	if (hash == NULL)
		return false;
	
    char * c_ds = base64((unsigned char *)digest, SHA_DIGEST_LENGTH);
    char * c_hs = base64((unsigned char *)hash, SHA_DIGEST_LENGTH);
    string ds(c_ds);
    string hs(c_hs);
    free(c_ds);
    free(c_hs);
    
	debug("digest is " + ds);
	debug("hash is " + hs);
	int res = memcmp(digest, hash, SHA_DIGEST_LENGTH);
	if (res == 0) {
		debug("Announcement " + confName + " : yes, this is equal digest!");
		return true;
	}
	
	// debug("memcmp result is " + string(res));
	return false;
}

void Announcement::copy(Announcement *a) {
	confName = string(a->confName);
	organizer = string(a->organizer);
	this->email = a->email;
	this->own = a->own;
	this->audio = a->audio;
	this->video = a->video;
	this->desc = a->desc;
	this->date = a->date;
	this->str_time = a->str_time;
	this->hours = a->hours;
	this->minutes = a->minutes;
	this->uuid = a->uuid;
	this->isPrivate = a->isPrivate;
	this->certs = a->certs;
	this->opaqueName = a->opaqueName;
	this->out = a->out;
	memcpy(this->conferenceKey, a->conferenceKey, KEY_LENGTH);
	memcpy(this->audioSessionKey, a->audioSessionKey, KEY_LENGTH);
	memcpy(this->digest, a->digest, SHA_DIGEST_LENGTH);
}

void Announcement::initConferenceKey() {
	int res = 0;
	while(res == 0) {
		res = RAND_bytes(conferenceKey, KEY_LENGTH);
	}
}

void Announcement::initAudioSessionKey() {
	int res = 0;
	while(res == 0) {
		res = RAND_bytes(audioSessionKey, KEY_LENGTH);
	}
}

string Announcement::toXml() {

    stringstream ss;
    string out;

	out.append("<conference>");
	out.append("<audio>");
	if (getAudio()) 
		out.append("true");
	else
		out.append("false");
	out.append("</audio>");

	out.append("<video>");
	if (getVideo()) 
		out.append("true");
	else
		out.append("false");
	out.append("</video>");

    debug("toXml 1");
	out.append("<confName>");
	out.append(getConfName());
	out.append("</confName>");
		
    debug("toXml 2");
	out.append("<organizer>");
	out.append(getOrganizer());
	out.append("</organizer>");

    debug("toXml 3");
	out.append("<email>");
	out.append(getEmail());
	out.append("</email>");

    debug("toXml 4");
	out.append("<desc>");
	out.append(getDesc());
	out.append("</desc>");

    debug("toXml 5");
	out.append("<date>");
	out.append(getDate());
	out.append("</date>");

	out.append("<time>");
	out.append(getTime());
	out.append("</time>");

	out.append("<hours>");
    ss << getHours();
    string str_hours;
    ss >> str_hours;
	out.append(str_hours);
	out.append("</hours>");

	out.append("<minutes>");
    ss << getMinutes();
    string str_minutes;
    ss >> str_minutes;
	out.append(str_minutes);
	out.append("</minutes>");

	out.append("<uuid>");
	out.append(getUuid());
	out.append("</uuid>");

	out.append("</conference>");
	return out;
}

void Announcement::loadFromXml(const char * buff, int len) {
    if (strlen(buff) != (size_t)len) {
        debug("bad format xml");
    }
    loadFromXml(string(buff));
}

void Announcement::loadFromXml(string buff) {
    stringstream ss;
    ss << buff;
    TiXmlDocument xmldoc = TiXmlDocument();
    ss >> xmldoc;
    TiXmlElement* root = xmldoc.FirstChildElement();    // <conference>
    if (!root) {
        return;
    }
    TiXmlNode* node = root->FirstChild();
    TiXmlElement* elem = root->FirstChildElement();
    while (node) {
        string attr = node->ValueStr();
        string value = "";
        if (elem->GetText()) {
            value = string(elem->GetText());
        }
        if (attr == "audio") {
            if (value == "true") {
                setAudio(true);
            } else {
                setAudio(false);
            }
        }
        else if (attr == "video") {
            if (value == "true") {
                setVideo(true);
            } else {
                setVideo(false);
            }
        }
        else if (attr == "confName") {
            setConfName(value);
        }
        else if (attr == "organizer") {
            setOrganizer(value);
        }
        else if (attr == "email") {
            setEmail(value);
        }
        else if (attr == "date") {
            setDate(value);
        }
        else if (attr == "time") {
            setTime(value);
        }
        else if (attr == "hours") {
            if (value != "") {
                setHours(atoi(value.c_str()));
            } else {
                setHours(0);
            }
        }
        else if (attr == "minutes") {
            if (value != "") {
                setMinutes(atoi(value.c_str()));
            } else {
                setMinutes(0);
            }
        }
        else if (attr == "desc") {
            setDesc(value);
        }
        else if (attr == "uuid") {
            setUuid(value);
        }
		else {
			critical("Unknown xml attribute");
		}
        /* Go to next */
        node = node->NextSibling();
        elem = elem->NextSiblingElement();
    }
}

FetchedAnnouncement::FetchedAnnouncement()
{
	Announcement();
	timestamp = time(NULL);
	dismissed = false;
}

void FetchedAnnouncement::refreshReceived() {
	timestamp = time(NULL);	
}

bool FetchedAnnouncement::needRefresh() {
	time_t now = time(NULL);
	if (now - timestamp > REFRESH_INTERVAL) {
		return true;
	}
	return false;
}

bool FetchedAnnouncement::isStaled() {
	time_t now = time(NULL);
	if (now - timestamp > REMOVE_INTERVAL) {
		return true;
	}
	return false;
}


