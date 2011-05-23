#include "announcement.h"
#include "debugbox.h"

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

char * Announcement::base64(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;

    BIO_free_all(b64);

    return buff;
}

void Announcement::setDigest(unsigned char *hash) {
	if (hash == NULL)
		return;
	
	memcpy(digest, hash, SHA_DIGEST_LENGTH);
    string ds(base64((unsigned char *)digest, SHA_DIGEST_LENGTH));
    string hs(base64((unsigned char *)hash, SHA_DIGEST_LENGTH));

	debug("digest is " + ds);
	debug("hash is " + hs);
}

bool Announcement::equalDigest(unsigned char *hash) {

	if (hash == NULL)
		return false;
	
    string ds(base64((unsigned char *)digest, SHA_DIGEST_LENGTH));
    string hs(base64((unsigned char *)hash, SHA_DIGEST_LENGTH));
    
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
	this->own = a->own;
	this->confName = a->confName;
	this->organizer = a->organizer;
	this->email = a->email;
	this->audio = a->audio;
	this->video = a->video;
	this->desc = a->desc;
	this->date = a->date;
	//this->time = a->time;
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

