#ifndef ANNOUNCEMENT_H
#define ANNOUNCEMENT_H

extern "C" {
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
}


#include <ctime>
#include <list>
#include <string>
using namespace std;

#define KEY_LENGTH 512/8
#define SHA_DIGEST_LENGTH 20

class Announcement {

private:
    string confName;
    string organizer;
    string email;
    bool own;
    bool audio;
    bool video;
    bool isPrivate;
    string desc;
    string date;
    string str_time;
    int hours;
    int minutes;
    string uuid;
    list<string> certs;
    string opaqueName;
    string out;

public:
    unsigned char conferenceKey[KEY_LENGTH];
    unsigned char audioSessionKey[KEY_LENGTH];
    unsigned char digest[SHA_DIGEST_LENGTH];

public:
    Announcement();
    void copy(Announcement *a);

    string getConfName() { return confName; }
    string getOrganizer() { return organizer; }
    string getEmail() { return email; }
    string getUuid() {return uuid; }
    bool getAudio() { return audio; }
    bool getVideo() { return video; }
    bool getOwner() { return own; }
    string getDesc() { return desc; }
    string getDate() { return date; }
    string getTime() { return str_time; }
    int getHours() { return hours; }
    int getMinutes() { return minutes; }
    bool getIsPrivate() { return isPrivate; }
    bool equalDigest(unsigned char *hash);
    list<string> getCerts() { return certs; }
    string getOpaqueName() { return opaqueName; }
    string getXmlOut() { return out; }

    void setConfName(string confName) { this->confName = string(confName); }
    void setOrganizer(string organizer) { this->organizer = organizer; }
    void setEmail(string email) { this->email = email; }
    void setUuid(string uuid) { this->uuid = uuid; }
    void setOwner(bool own) { this->own = own; }
    void setAudio(bool audio) { this->audio = audio; }
    void setVideo(bool video) { this->video = video; }
    void setDesc(string desc) { this->desc = desc; }
    void setDate(string date) { this->date = date; }
    void setTime(string time) { this->str_time = time; }
    void setHours(int hours) { this->hours = hours; }
    void setMinutes(int minutes) { this->minutes = minutes; }
    void setIsPrivate(bool b) { isPrivate = b; }
    void setCerts(list<string> certs) { this->certs = certs; }
    void setOpaqueName(string opaqueName) { this->opaqueName = opaqueName; }
    void setXmlOut(string out) { this->out = out; }

    void setDigest(unsigned char *hash);
    void initConferenceKey();
    void initAudioSessionKey();

    string toXml();
    void loadFromXml(const char * buff, int len);
    void loadFromXml(string buff);
};

// QDataStream &operator<<(QDataStream &out, Announcement *a);

class FetchedAnnouncement : public Announcement {

public:
    bool isDismissed() { return dismissed; }
    void setDismissed(bool dismissed) { this->dismissed = dismissed; }
    bool getIsEligible() { return isEligible; }
    void setIsEligible(bool b) { isEligible = b; }

    FetchedAnnouncement();
    void refreshReceived();
    bool isStaled();
    bool needRefresh();

private:
    time_t timestamp;       // Unix timestamp in second
    bool dismissed;
    bool isEligible;
};

// QString &operator<<(QString &out, Announcement *a);
// QDomDocument &operator>>(QDomDocument &in, Announcement *a);

#endif // ANNOUNCEMENT_H
