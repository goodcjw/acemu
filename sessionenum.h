#ifndef SESSIONENUM_H
#define SESSIONENUM_H

extern "C" {
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <ccn/ccn.h>
#include <ccn/bloom.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#include <ccn/schedule.h>
#include <errno.h>
}

#include "announcement.h"

class SessionEnum {
private:
    string prefix;
	list<Announcement *> myConferences;
	list<Announcement *> myPrivateConferences;
	list<FetchedAnnouncement *> pubConferences;
	list<FetchedAnnouncement *> priConferences;
	bool bRunning;
	struct ccn *ccn;
	struct ccn_closure *to_announce;
	struct ccn_closure *to_announce_private;
	struct ccn_closure *fetch_announce;
	struct ccn_closure *fetch_private;
	struct ccn_closure *handle_dismiss;
	struct ccn_keystore *keystore;
	struct ccn_keystore *actd_keystore;
	struct ccn_charbuf *signed_info;
	//QTimer *enumTimer;
	//QTimer *aliveTimer;
	string uuid;
	bool listPrivate;
	list<struct ccn_pkey *>publicKeys;

public:
	SessionEnum(string prefix);
	SessionEnum();
	~SessionEnum();
	void startThread();
	void stopThread();
	void setPrefix(string prefix) { this->prefix = prefix; }
	void removeFromMyConferences(Announcement *a);
	void addToMyConferences(Announcement *a);
	void addToConferences(Announcement *a, bool pub);
    list<Announcement *> listAllPubConference();
    list<Announcement *> listAllPriConference();
	void handleDismissEvent(struct ccn_upcall_info *info);
	void handleEnumInterest(struct ccn_upcall_info *info);
	void handleEnumContent(struct ccn_upcall_info *info);
	void handleEnumPrivateInterest(struct ccn_upcall_info *info);
	void handleEnumPrivateContent(struct ccn_upcall_info *info);
	bool isConferenceRefresh(unsigned char *hash, bool pub);
	void sendDismissSignal(Announcement *a);
	void setListPrivate(bool b);

private:
	void enumerate();
	void enumeratePriConf();
	void enumeratePubConf();
	void checkAlive();
	void ccnConnect();
	void initKeystoreAndSignedInfo();
	void loadPublicAndPrivateKeys();
	static int pubKeyEncrypt(EVP_PKEY *public_key,
                             const unsigned char *data, 
                             size_t data_length,
					         unsigned char **encrypted_output,
                             size_t *encrypted_output_length);
	static int priKeyDecrypt(EVP_PKEY *private_key,
                             const unsigned char *ciphertext,
                             size_t ciphertext_length,
					         unsigned char **decrypted_output,
                             size_t *decrypted_output_length);
	static int symDecrypt(const unsigned char *key,
                          const unsigned char *iv, 
                          const unsigned char *ciphertext, 
						  size_t ciphertext_length,
                          unsigned char **plaintext,
                          size_t *plaintext_length, 
						  size_t plaintext_padding);
	static int symEncrypt(const unsigned char *key,
                          const unsigned char *iv,
                          const unsigned char *plaintext, 
						  size_t plaintext_length,
                          unsigned char **ciphertext,
                          size_t *ciphertext_length,
						  size_t padding); 
    static void* run(void * se);
};

//static void* run(void * se);

#endif // SESSIONENUM_H

