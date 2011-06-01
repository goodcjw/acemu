#include "sessionenum.h"
#include "debugbox.h"
#include "tinyxml.h"
#include "base64.h"

#include <unistd.h>
#include <pthread.h>    
#include <sstream>

#ifdef DEBUG
#include <iostream>
#include <sys/time.h>
#endif // DEBUG

extern "C" {
#include <ccn/ccn.h>
#include <ccn/bloom.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#include <ccn/uri.h>
#include <ccn/schedule.h>
}

using namespace std;

#define BROADCAST_PREFIX ("/ndn/broadcast/conference")
#define EST_USERS 20
#define FRESHNESS 30
#define HOSTNAME_LEN 127

const unsigned char SEED[] = "1412";
static SessionEnum *gsd = NULL;
static pthread_mutex_t ccn_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ccn_bloom {
    int n;
    struct ccn_bloom_wire *wire;
};

static char *ccn_name_comp_to_str(const unsigned char *ccnb,
								  const struct ccn_indexbuf *comps,
								  int index);

static enum ccn_upcall_res incoming_content(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_private_content(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_interest(struct ccn_closure *selfp,
                                             enum ccn_upcall_kind kind,
                                             struct ccn_upcall_info *info);

static enum ccn_upcall_res incoming_private_interest(struct ccn_closure *selfp,
                                                     enum ccn_upcall_kind kind,
                                                     struct ccn_upcall_info *info);

static enum ccn_upcall_res dismiss_signal(struct ccn_closure *selfp,
                                          enum ccn_upcall_kind kind,
                                          struct ccn_upcall_info *info);

static void append_bloom_filter(struct ccn_charbuf *templ, struct ccn_bloom *b);

SessionEnum::SessionEnum(string prefix) {
	gsd = this;
    bRunning = false;
	listPrivate = false;


    // TODO This is not really uuid
    char tmphm[HOSTNAME_LEN];
    memset(tmphm, 0, HOSTNAME_LEN);
    gethostname(tmphm, HOSTNAME_LEN);
    uuid = string(tmphm);

	fetch_announce = (struct ccn_closure *) (calloc(1, sizeof(struct ccn_closure)));
	fetch_announce->p = &incoming_content;
	fetch_private = (struct ccn_closure *) (calloc(1, sizeof(struct ccn_closure)));
	fetch_private->p = &incoming_private_content;
	ccnConnect();
	initKeystoreAndSignedInfo();
	setPrefix(prefix);

    /*
	enumTimer = new QTimer(this);
	connect(enumTimer, SIGNAL(timeout()), this, SLOT(enumerate()));
	enumTimer->start(4000);

	aliveTimer = new QTimer(this);
	connect(aliveTimer, SIGNAL(timeout()), this, SLOT(checkAlive()));
	aliveTimer->start(15000);
    */

	enumerate();
}

SessionEnum::~SessionEnum() {
    if (bRunning) {
        stopThread();
    }
	if (ccn != NULL) {
		ccn_disconnect(ccn);
		ccn_destroy(&ccn);
	}
	if (keystore != NULL) {
		ccn_keystore_destroy(&keystore);
	}
	if (actd_keystore != NULL) {
		ccn_keystore_destroy(&actd_keystore);
	}
    debug("\nBye Bye SessionEnum\n");
}

void SessionEnum::startThread() {
    if (!bRunning) {
        pthread_mutex_lock(&ccn_mutex);
        bRunning = true;
        pthread_mutex_unlock(&ccn_mutex);

        pthread_t ccn_thread;
        pthread_create(&ccn_thread, NULL, &run, (void *) this);
    }
}

void SessionEnum::stopThread() {
    if (bRunning) {
        pthread_mutex_lock(&ccn_mutex);
        bRunning = false;
        pthread_mutex_unlock(&ccn_mutex);        
    }
}

void SessionEnum::enumerate() {
	enumeratePubConf();
	if (listPrivate) {
		enumeratePriConf();
	}
}

void SessionEnum::enumeratePubConf() {
	struct ccn_charbuf *interest = ccn_charbuf_create();
	if (interest == NULL ) {
		critical("interest construction failed");
	}
	int res = ccn_name_from_uri(interest, BROADCAST_PREFIX);
	if (res < 0)
		critical("Bad ccn URI");
	
	ccn_name_append_str(interest, "conference-list");
	
	struct ccn_bloom *exc_filter = ccn_bloom_create(EST_USERS, SEED);
	unsigned char *bloom = exc_filter->wire->bloom;
	memset(bloom, 0, 1024);
    list<FetchedAnnouncement *>::iterator it;
	for (it = pubConferences.begin(); it != pubConferences.end(); it++) {
		FetchedAnnouncement *fa = *it;
		if (fa == NULL) 
			critical("SessionEnum::enumrate");

		if (!fa->needRefresh()) {
			ccn_bloom_insert(exc_filter,
                             fa->getConfName().c_str(), 
                             fa->getConfName().size());
		}
	}
    list<Announcement *>::iterator ia;
	for (ia = myConferences.begin(); ia != myConferences.end(); ia++) {
		Announcement *a = *ia;
		if (a == NULL) 
			critical("SessionEnum::enumrate");

		ccn_bloom_insert(exc_filter,
                         a->getConfName().c_str(), 
                         a->getConfName().size());
	}

    struct ccn_charbuf *templ = ccn_charbuf_create();

    ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG); // <Interest>
    ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG); // <Name>
    ccn_charbuf_append_closer(templ); // </Name> 

    if (ccn_bloom_n(exc_filter) != 0) { 
        // exclusive_filter not empty, append it to the interest
        append_bloom_filter(templ, exc_filter);
    }

    ccn_charbuf_append_closer(templ); // </Interest> 
	
	res = ccn_express_interest(ccn, interest, fetch_announce, templ);
	if (res < 0) {
		critical("express interest failed!");
	}
	ccn_charbuf_destroy(&templ);
	ccn_charbuf_destroy(&interest);
}

void SessionEnum::enumeratePriConf() {
	struct ccn_charbuf *interest = ccn_charbuf_create();
	if (interest == NULL ) {
		critical("interest construction failed");
	}
	int res = ccn_name_from_uri(interest, BROADCAST_PREFIX);
	if (res < 0)
		critical("Bad ccn URI");
	
	ccn_name_append_str(interest, "private-list");
	
	struct ccn_bloom *exc_filter = ccn_bloom_create(EST_USERS, SEED);
	unsigned char *bloom = exc_filter->wire->bloom;
	memset(bloom, 0, 1024);
    list<FetchedAnnouncement *>::iterator af; 
	for (af = priConferences.begin(); af != priConferences.end(); af++) {
		FetchedAnnouncement *fa = *af;
		if (fa == NULL) 
			critical("SessionEnum::enumrate");

		if (!fa->needRefresh()) {
			ccn_bloom_insert(exc_filter,
                             fa->getOpaqueName().c_str(),
                             fa->getOpaqueName().size());

			debug("append " + fa->getOpaqueName() + " to private exclude filter");
		}
	}

    list<Announcement *>::iterator ia;    
	for (ia = myPrivateConferences.begin();
         ia != myPrivateConferences.end(); 
         ia++) {
		Announcement *a = *ia;
		if (a == NULL)
			critical("sessionEnum:: enumrate");

		ccn_bloom_insert(exc_filter,
                         a->getOpaqueName().c_str(),
                         a->getOpaqueName().size());
	}

    struct ccn_charbuf *templ = ccn_charbuf_create();

    ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG);  // <Interest>
    ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG);      // <Name>
    ccn_charbuf_append_closer(templ);                           // </Name> 

    if (ccn_bloom_n(exc_filter) != 0) { 
        // exclusive_filter not empty, append it to the interest
        append_bloom_filter(templ, exc_filter);
    }

    ccn_charbuf_append_closer(templ);                           // </Interest> 
	
	res = ccn_express_interest(ccn, interest, fetch_private, templ);
	if (res < 0) {
		critical("express interest failed!");
	}
	debug("expressing private interest");
	ccn_charbuf_destroy(&templ);
	ccn_charbuf_destroy(&interest);    
}


void SessionEnum::ccnConnect() {
    ccn = NULL;

    ccn = ccn_create();
    if (ccn == NULL || ccn_connect(ccn, NULL) == -1) {
		string qs = "Failed to initialize ccn agent connection";
		critical(qs);
    }

	// public conf
    struct ccn_charbuf *enum_interest = ccn_charbuf_create();
    if (enum_interest == NULL) {
        string qs = "Failed to allocate or initialize interest filter path";
		critical(qs);
    }
	ccn_name_from_uri(enum_interest, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(enum_interest, "conference-list");
	to_announce = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	to_announce->p = &incoming_interest;
    ccn_set_interest_filter(ccn, enum_interest, to_announce);
    ccn_charbuf_destroy(&enum_interest);

	// private conf
    struct ccn_charbuf *private_interest = ccn_charbuf_create();
    if (private_interest == NULL) {
        string qs = string("Failed to allocate or initialize interest filter path");
		critical(qs);
    }
	ccn_name_from_uri(private_interest, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(private_interest, "private-list");
	to_announce_private = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	to_announce_private->p = &incoming_private_interest;
    ccn_set_interest_filter(ccn, private_interest, to_announce_private);
    ccn_charbuf_destroy(&private_interest);

	// dismiss interest
    struct ccn_charbuf *dismiss_interest = ccn_charbuf_create();
    if (dismiss_interest == NULL) {
        string qs = string("Failed to allocate or initialize interest filter path");
		critical(qs);
    }
	ccn_name_from_uri(dismiss_interest, (const char *) BROADCAST_PREFIX);
	ccn_name_append_str(dismiss_interest, "dismiss");
	handle_dismiss = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	handle_dismiss->p = &dismiss_signal;
    ccn_set_interest_filter(ccn, dismiss_interest, handle_dismiss);
    ccn_charbuf_destroy(&dismiss_interest);
}

void SessionEnum::initKeystoreAndSignedInfo() {
	// prepare for ccnx
	keystore = NULL;
	ccn_charbuf *temp = ccn_charbuf_create();
	keystore = ccn_keystore_create();
	ccn_charbuf_putf(temp, "%s/.ccnx/.ccnx_keystore", getenv("HOME"));
	int res = ccn_keystore_init(keystore,
				ccn_charbuf_as_string(temp),
				(char *)"Th1s1sn0t8g00dp8ssw0rd.");
	if (res != 0) {
	    printf("Failed to initialize keystore %s\n", ccn_charbuf_as_string(temp));
	    exit(1);
	}
	ccn_charbuf_destroy(&temp);
	
	struct ccn_charbuf *keylocator = ccn_charbuf_create();
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_KeyLocator, CCN_DTAG);
	ccn_charbuf_append_tt(keylocator, CCN_DTAG_Key, CCN_DTAG);
	res = ccn_append_pubkey_blob(keylocator, ccn_keystore_public_key(keystore));
	if (res < 0) {
		ccn_charbuf_destroy(&keylocator);
	}else {
		ccn_charbuf_append_closer(keylocator);
		ccn_charbuf_append_closer(keylocator);
	}

	signed_info = NULL;
	signed_info = ccn_charbuf_create();
	res = ccn_signed_info_create(signed_info,
                                 ccn_keystore_public_key_digest(keystore),
                                 ccn_keystore_public_key_digest_length(keystore),
                                 NULL,
                                 CCN_CONTENT_DATA,
                                 FRESHNESS, 
                                 NULL,
                                 keylocator);
	if (res < 0) {
		critical("Failed to create signed_info");
	}

	// public & private key pair for actd
	actd_keystore = NULL;
	actd_keystore = ccn_keystore_create();
	temp = ccn_charbuf_create();
	ccn_charbuf_putf(temp, "%s/.actd/.actd_keystore", getenv("HOME"));
	res = ccn_keystore_init(actd_keystore,
								ccn_charbuf_as_string(temp),
								(char *)"Th1s1s@p8ssw0rdf0r8ctd.");
	if (res != 0) 
		critical("Failed to initialze keystore for actd");
	ccn_charbuf_destroy(&temp);
}

void SessionEnum::setListPrivate(bool b) {
	listPrivate = b;
	if (listPrivate) {
		enumeratePriConf();
	}
	if (!listPrivate && priConferences.size() > 0) {
        list<FetchedAnnouncement *>::iterator it;    
        for (it = priConferences.begin(); it != priConferences.end(); it++) {
			FetchedAnnouncement *a = *it;
			if (a != NULL) {
				string confName = a->getConfName();
				string organizer = a->getOrganizer();
				free(a);
			}
		}
		priConferences.clear();
	}
}

void SessionEnum::handleEnumContent(struct ccn_upcall_info *info) {
    debug("handleEnumContent");
    const unsigned char *value = NULL;
    size_t len = 0;
    int res =ccn_content_get_value(info->content_ccnb,
                                   info->pco->offset[CCN_PCO_E],
                                   info->pco, &value, &len);
    if (res < 0)
        critical("failed to parse content object");

    unsigned char hash[SHA_DIGEST_LENGTH];	
    SHA1(value, len, hash);
    // Debug
    char * c_hs = base64((unsigned char *)hash, SHA_DIGEST_LENGTH);
    string hs(c_hs);
    debug("SHA-1 is : " + hs);
    free(c_hs);

    if (isConferenceRefresh(hash, true))
        return;

    Announcement *a = new Announcement();
    a->loadFromXml((const char *)value, len);

    a->setIsPrivate(false);
    a->setDigest(hash);

    if (a->getConfName() == "")
        critical("Fetched Conference Name is empty");
    debug("call addToConferences");
    addToConferences(a, true);
}

void SessionEnum::handleEnumInterest(struct ccn_upcall_info *info) {
    debug("handleEnumInterest");
	string val = "conference-list";
	if (ccn_name_comp_strcmp(info->interest_ccnb,
                             info->interest_comps,
                             3, val.c_str()) != 0) {
		debug("Public Listing: trash interest received");
		return;
	}
	// /ndn/broadcast/conference/conference-list
    list<Announcement *>::iterator it;
    for (it = myConferences.begin(); it != myConferences.end(); it++) {
		Announcement *a = *it;
        debug("handleEnumInterest: " + a->getConfName());
		struct ccn_charbuf *name = NULL;
		struct ccn_charbuf *content = NULL;

		name = ccn_charbuf_create();
		ccn_name_init(name);
		int nameEnd = info->interest_comps->n - 1;
		ccn_name_append_components(name, info->interest_ccnb,
                                   info->interest_comps->buf[0],
                                   info->interest_comps->buf[nameEnd]);
		string confName = a->getConfName();
		ccn_name_append_str(name, confName.c_str());

		string qba;
		qba = a->toXml();
		// QByteArray qba = qsData.toLocal8Bit();
		char *buffer = static_cast<char *>(calloc(qba.size() + 1, sizeof(char)));
		memcpy(buffer, qba.c_str(), qba.size());
		buffer[qba.size()] = '\0';

		content = ccn_charbuf_create();
		int res = ccn_encode_ContentObject(content, name, signed_info,
                                           (const char *)buffer,
                                           strlen(buffer),
                                           NULL,
                                           ccn_keystore_private_key(keystore));
		if (res)
			critical("failed to create content");

		ccn_put(info->h, content->buf, content->length);
		ccn_charbuf_destroy(&name);
		ccn_charbuf_destroy(&content);
		if (buffer) {
			free((void *)buffer);
			buffer = NULL;
		}
		name = NULL;
		content = NULL;
	}
}

void SessionEnum::handleEnumPrivateInterest(struct ccn_upcall_info *info) {
	string val = "private-list";
	if (0 != ccn_name_comp_strcmp(info->interest_ccnb,
                                  info->interest_comps,
                                  3, val.c_str())){
		debug("Private Listing: trash interest received");
		return;
	}

	// /ndn/broadcast/conference/private-list
    list<Announcement *>::iterator it;
    for (it = myPrivateConferences.begin(); it != myPrivateConferences.end(); it++) {
		Announcement *a = *it;
		string opaqueName = a->getOpaqueName();

		string out;
		if (a->getXmlOut() == "") {
			out.append("<bundle>");
			// <Enc-data></Enc-data><Enc-data></Enc-data>...<SK></SK>..<Desc></Desc>
			list<string> certs = a->getCerts();

            list<string>::iterator its;
            for (its = certs.begin(); its != certs.end(); its++) {
                string path = *its;
				FILE *fp = fopen(path.c_str(), "r");
				if (!fp) {
					//critical("Can not open cert " + path);
					debug("no certs of participants specified");
					return;
				}
				X509 *cert= PEM_read_X509(fp, NULL, NULL, NULL);
				fclose(fp);
				EVP_PKEY *public_key = X509_get_pubkey(cert);
				char *to_enc = (char *)malloc(sizeof(a->conferenceKey) + 5);
				to_enc[0] = 'A';
                to_enc[1] = 'T';
                to_enc[2] = 'H';
                to_enc[3] = 'U';
                to_enc[4] ='\0';
				memcpy(to_enc + 5, a->conferenceKey, sizeof(a->conferenceKey));
				char *enc_data = NULL;
				size_t enc_len = 0;

				int res = pubKeyEncrypt(public_key,
                                        (const unsigned char *)to_enc,
                                        (size_t)(sizeof(a->conferenceKey) + 5), 
                                        (unsigned char **)&enc_data, &enc_len);
				if (res != 0) 
                    critical("public key encryption failed!");
                char * qba = base64((unsigned char *)enc_data, (int)enc_len);
				string base64(qba);
                free(qba);
				out.append("<Enc-Data>");
				out.append(base64);
				out.append("</Enc-Data>");
				if (fp)
					free(fp);
				if (cert)
					free(cert);
				if (public_key)
					free(public_key);
				if (to_enc)
					free(to_enc);
				if (enc_data)
					free(enc_data);
			}

			char *enc_session_key = NULL;
			size_t enc_key_len = 0;
			char iv[sizeof(a->audioSessionKey)];
			RAND_bytes((unsigned char*)iv, sizeof(iv));

			int res = symEncrypt(a->conferenceKey, 
                                 (unsigned char *)iv, 
                                 (const unsigned char *)a->audioSessionKey,
                                 sizeof(a->audioSessionKey),
                                 (unsigned char **)&enc_session_key, 
                                 &enc_key_len, 
                                 (size_t)AES_BLOCK_SIZE);
			if (res != 0) 
				critical("sym encryption by conference key failed");

            char * qbaSK = base64((unsigned char *)enc_session_key, (int)enc_key_len);
			string base64SK(qbaSK);
            free(qbaSK);

            char * qbaIV = base64((unsigned char *)iv, strlen(iv));
			string base64IV(qbaIV);
            free(qbaIV);

			out.append("<Enc-SK>");
			out.append("<iv>");
			out.append(base64IV);
			out.append("</iv>");
			out.append("<SK>");
			out.append(base64SK);
			out.append("</SK>");
			out.append("</Enc-SK>");
			if (!enc_session_key)
				free(enc_session_key);
			
			string qsData = a->toXml();
			const char *qba = qsData.c_str();
			size_t buf_len = strlen(qba) + 1;
			char *buffer = (char *)malloc(buf_len);
			memcpy(buffer, qba, strlen(qba));
			buffer[buf_len - 1] = '\0';
			char *enc_desc = NULL;
			size_t enc_desc_len = 0;
			res = symEncrypt(a->conferenceKey,
                             NULL,
                             (const unsigned char *)buffer,
                             buf_len, 
                             (unsigned char **) &enc_desc,
                             &enc_desc_len,
                             (size_t)AES_BLOCK_SIZE);
			if (res != 0)
				critical("conf desc encryption by conference key failed");
            char * qbaDesc = base64((unsigned char *)enc_desc, (int)enc_desc_len);
			string base64Desc(qbaDesc);
            free(qbaDesc);
			out.append("<Enc-Desc>");
			out.append(base64Desc);
			out.append("</Enc-Desc>");
			if (enc_desc)
				free(enc_desc);

			out.append("</bundle>");
			a->setXmlOut(out);
		}
		else 
			out = a->getXmlOut();

		struct ccn_charbuf *name = NULL;
		struct ccn_charbuf *content = NULL;
		name = ccn_charbuf_create();
		ccn_name_init(name);
		int nameEnd = info->interest_comps->n - 1;
		ccn_name_append_components(name, info->interest_ccnb,
                                   info->interest_comps->buf[0], 
                                   info->interest_comps->buf[nameEnd]);
		ccn_name_append_str(name, opaqueName.c_str());

		char *secret = (char *)malloc(out.size() + 1);
		strcpy(secret, out.c_str());
		content = ccn_charbuf_create();
		int res = ccn_encode_ContentObject(content, name, 
                                           signed_info, secret,
                                           out.size(), NULL,
                                           ccn_keystore_private_key(keystore));
		if (res)
			critical("failed to create content");
		ccn_put(info->h, content->buf, content->length);
		ccn_charbuf_destroy(&name);
		ccn_charbuf_destroy(&content);
		if (secret)
			free(secret);
	}
}

void SessionEnum::handleEnumPrivateContent(struct ccn_upcall_info *info) {
    const unsigned char *value = NULL;
    size_t len = 0;
    int res =ccn_content_get_value(info->content_ccnb,
                                   info->pco->offset[CCN_PCO_E],
                                   info->pco, &value, &len);
    if (res < 0)
        critical("failed to parse content object");

    unsigned char hash[SHA_DIGEST_LENGTH];	
    SHA1(value, len, hash);

    if (isConferenceRefresh(hash, false)) 
        return;

    string s_value = string((char *)value);
    if (s_value.size() !=  len) {
        debug("unexpected connect");
    }
    /* TODO how encryption is carried out? */
    stringstream ss;
    ss.read((char *)value, len);

    bool eligible = false;
    Announcement *a = NULL;
    TiXmlDocument xmldoc = TiXmlDocument();
    ss >> xmldoc;
    TiXmlElement* root = xmldoc.FirstChildElement();    // <conference>
    if (!root) {
        return;
    }
    TiXmlNode* node = root->FirstChild();               // <Enc-Data>
    TiXmlElement* elem = root->FirstChildElement();
    
    while (node) {
        string attr = node->ValueStr();
        string value = string(elem->GetText());
        if (attr == "Enc-Data") {
            debug("decrypting enc-data");
            char *dout = NULL;
            size_t dout_len = 0;
            struct ccn_pkey *priKey =
                (struct ccn_pkey *)ccn_keystore_private_key(actd_keystore);
            char * enc_data = unbase64((unsigned char*)value.c_str(),
                                       value.size());
            res = priKeyDecrypt((EVP_PKEY*) priKey,
                                (unsigned char *)enc_data,
                                strlen(enc_data),
                                (unsigned char **)&dout,
                                &dout_len);
            if (res != 0) {
                debug("decrypt failed\n");
                node = node->NextSibling();
                elem = elem->NextSiblingElement();
                continue;
            }

            char jargon[5];
            memcpy(jargon, dout, 5);
            if (strcmp(jargon, "ATHU") == 0) {
                eligible = true;
                a = new Announcement();
                memcpy(a->conferenceKey, dout + 5, dout_len - 5);
                if (dout)
                    free(dout);
            }
            if (dout)
                free(dout);
        } else
        if (attr == "Enc-SK") { //<Enc-SK>
            if (!eligible)
                return;

            TiXmlNode* skNode = node->FirstChild();
            TiXmlElement* skElem = node->FirstChildElement();
            
            if (!skNode || skNode->ValueStr() != "iv") {
                return;
            }
            string skIV = string(skElem->GetText());
            char * qbaIV = unbase64((unsigned char*)skIV.c_str(),
                                    skIV.size());

            skNode = skNode->NextSibling();
            skElem = skElem->NextSiblingElement();
            if (!skNode || skNode->ValueStr() != "SK") {
                return;
            }
            string encSK = string(skElem->GetText());
            char * enc_sk = unbase64((unsigned char*)encSK.c_str(),
                                    encSK.size());

            char *session_key = NULL;
            size_t session_key_len = 0;
            res = symDecrypt(a->conferenceKey,
                             (unsigned char *)qbaIV,
                             (unsigned char *)enc_sk,
                             strlen(enc_sk),
                             (unsigned char **)&session_key,
                             &session_key_len, AES_BLOCK_SIZE);
            if (res != 0) 
                critical("can not decrypt sessionkey");

            memcpy(a->audioSessionKey, session_key, session_key_len);
            if (session_key)
                free(session_key);
            if (qbaIV)
                free(qbaIV);
            if (enc_sk)
                free(enc_sk);
        } else 
        if (attr == "Enc-Desc") {
            if (!eligible)
                return;
            char *desc = NULL;
            size_t desc_len = 0;
            char * enc_desc = unbase64((unsigned char*)value.c_str(),
                                       value.size());
            res = symDecrypt(a->conferenceKey,
                             NULL,
                             (unsigned char *)enc_desc,
                             strlen(enc_desc),
                             (unsigned char **)&desc,
                                 &desc_len, AES_BLOCK_SIZE);
            if (res != 0) 
                critical("can not decrypt desc");

            a->loadFromXml(desc, desc_len);
            // Free if (!desc) ???
            if (desc)
                free(desc);
            if (enc_desc)
                free(enc_desc);
        }
        else {
            critical("Unknown xml attribute");
        }
        node = node->NextSibling();
        elem = elem->NextSiblingElement();
    }
    // /ndn/broadcast/conference/private-list/opaque-name	
    char *opaqueName = NULL;
    opaqueName = ccn_name_comp_to_str(info->content_ccnb, info->content_comps, 4);
    if (opaqueName == NULL)
        critical("can not get opaque name!");

    a->setOpaqueName(opaqueName);

    a->setIsPrivate(true);
    a->setDigest(hash);

    if (a->getConfName() == "")
        critical("Fetched Conference Name is empty");

    addToConferences(a, false);
    if (!value)
        free((void *)value);

    debug("handle private content done");
}

void SessionEnum::handleDismissEvent(struct ccn_upcall_info *info) {
	char *dUuid = NULL;
	char *dConfName = NULL;
	char *dOrganizer = NULL;
	const unsigned char *ccnb = info->interest_ccnb;
	const struct ccn_indexbuf *comps = info->interest_comps;
	dUuid = ccn_name_comp_to_str(ccnb, comps, 4);
	dConfName = ccn_name_comp_to_str(ccnb, comps, 5);
	dOrganizer = ccn_name_comp_to_str(ccnb, comps, 6);
    list<FetchedAnnouncement *>::iterator it;
	for (it = pubConferences.begin(); it != pubConferences.end(); it++) {
		FetchedAnnouncement *fa = *it;
		if (fa == NULL)
			critical("NULL encountered unexpectedly");

		if (fa->getUuid() == dUuid && 
            fa->getConfName() == dConfName && 
            fa->getOrganizer() == dOrganizer) {
			// mark it as dismissed, but leave it to be cleaned by checkAlive,
			// so that this conference is not displayed to the user, 
            // but still kept by SessionEnum
			// until it is timed out.
			// this is to avoid fetch the cached information of this conference
            // in the intermediate routers as soon
			// as we remove it from the publist
			// when it times out here, the cached information should also have timed out.
			fa->setDismissed(true);
			break;
		}
	}
    for (it = priConferences.begin(); it != priConferences.end(); it++) {
		FetchedAnnouncement *fa = *it;
		if (fa == NULL)
			critical("NULL encountered unexpectedly");

		if (fa->getUuid() == dUuid &&
            fa->getConfName() == dConfName && 
            fa->getOrganizer() == dOrganizer) {
			// mark it as dismissed, but leave it to be cleaned by checkAlive,
			// so that this conference is not displayed to the user, 
            // but still kept by SessionEnum
			// until it is timed out.
			// this is to avoid fetch the cached information of this conference 
            // in the intermediate routers as soon
			// as we remove it from the publist
			// when it times out here, the cached information should also have timed out.
			fa->setDismissed(true);
			break;
		}
	}

}

void SessionEnum::addToConferences(Announcement *a, bool pub) {
	if (a->getUuid() == uuid) {
        debug("addToConferences same uuid: " + uuid);
		return;
    }
	FetchedAnnouncement *fa = new FetchedAnnouncement();
	fa->copy(a);
	if (pub) {
		pubConferences.push_back(fa);
		// TODO emit add(fa);
		enumeratePubConf();
	}
	else {
		priConferences.push_back(fa);
		if (listPrivate) {
			// TODO emit add(fa);
			enumeratePriConf();
		}
	}
}

void SessionEnum::addToMyConferences(Announcement *a) {
	a->setUuid(uuid);
	if (a->getIsPrivate()) {
		myPrivateConferences.push_back(a);
	}
	else {
		myConferences.push_back(a);
	}
}

list<Announcement *> SessionEnum::listAllPubConference() {
    debug("listAllPubConference");
    list<Announcement *> all_pubconf;
    list<Announcement *>::iterator it;
    cout << myConferences.size() << " myConf" << endl;
    for (it = myConferences.begin(); it != myConferences.end(); it++) {
        all_pubconf.push_back(*it);
    }
    cout << pubConferences.size() << " PubConf" << endl;
    list<FetchedAnnouncement *>::iterator ia;
    for (ia = pubConferences.begin(); ia != pubConferences.end(); ia++) {
        all_pubconf.push_back((Announcement *) *ia);
    }
    return all_pubconf;
}

list<Announcement *> SessionEnum::listAllPriConference() {
    list<Announcement *> all_priconf;
    list<Announcement *>::iterator it;
    for (it = myPrivateConferences.begin(); it != myPrivateConferences.end(); it++) {
        all_priconf.push_back(*it);
    }
    list<FetchedAnnouncement *>::iterator ia;
    for (ia = priConferences.begin(); ia != priConferences.end(); ia++) {
        all_priconf.push_back(*it);
    }
    return all_priconf;
}

bool SessionEnum::isConferenceRefresh(unsigned char *hash, bool pub) {
    list<FetchedAnnouncement *>::iterator it;    
	if (pub) {
        for (it = pubConferences.begin(); it != pubConferences.end(); it++) {
			FetchedAnnouncement *fa = *it;
			if (fa->equalDigest(hash)) {
				fa->refreshReceived();
				enumeratePubConf();
				return true;
			}
		}
	} else {
        for (it = priConferences.begin(); it != priConferences.end(); it++) {
			FetchedAnnouncement *fa = *it;
			if (fa->equalDigest(hash)) {
				fa->refreshReceived();
				enumeratePriConf();
				return true;
			}
		}
	}
	return false;
}

int SessionEnum::pubKeyEncrypt(EVP_PKEY *public_key,
							   const unsigned char *data, size_t data_length,
							   unsigned char **encrypted_output,
							   size_t *encrypted_output_length) {

    int openssl_result = 0;
    unsigned char *eptr = NULL;

    if ((NULL == data) || (0 == data_length) || (NULL == public_key))
        return EINVAL;

    *encrypted_output_length = ccn_pubkey_size((struct ccn_pkey *)public_key);
	eptr = (unsigned char *)malloc(*encrypted_output_length);

    memset(eptr, 0, *encrypted_output_length);

	RSA *trsa = EVP_PKEY_get1_RSA((EVP_PKEY *)public_key);
	openssl_result = RSA_public_encrypt(data_length, data, eptr, trsa, RSA_PKCS1_PADDING);
	RSA_free(trsa);

    if (openssl_result < 0) {
        if (NULL == *encrypted_output) {
            free(eptr);
        }
        return openssl_result;
    }
    *encrypted_output = eptr;
	*encrypted_output_length = openssl_result;
    return 0;
}

int SessionEnum::priKeyDecrypt(EVP_PKEY *private_key,
                               const unsigned char *ciphertext,
                               size_t ciphertext_length,
                               unsigned char **decrypted_output,
                               size_t *decrypted_output_length) {

    unsigned char *dptr = NULL;
	int openssl_result = 0;

    if ((NULL == ciphertext) || (0 == ciphertext_length) || (NULL == private_key))
        return EINVAL;

    *decrypted_output_length = EVP_PKEY_size((EVP_PKEY *)private_key);
	dptr = (unsigned char *)malloc(*decrypted_output_length);
    memset(dptr, 0, *decrypted_output_length);

	RSA *trsa = EVP_PKEY_get1_RSA(private_key);	
	openssl_result = RSA_private_decrypt(ciphertext_length,
                                         ciphertext, 
                                         dptr,
                                         trsa,
                                         RSA_PKCS1_PADDING);
	RSA_free(trsa);

    if (openssl_result < 0) {
        if (NULL == *decrypted_output) {
            free(dptr);
        }
        return openssl_result;
    }
    *decrypted_output = dptr;
	*decrypted_output_length = openssl_result;
    return 0;
}

int SessionEnum::symEncrypt(const unsigned char *key,
							const unsigned char *iv,
							const unsigned char *plaintext, 
							size_t plaintext_length,
							unsigned char **ciphertext, 
							size_t *ciphertext_length,
							size_t ciphertext_padding) {
    EVP_CIPHER_CTX ctx;
    unsigned char *cptr = *ciphertext;
    unsigned char *eptr = NULL;
    /* maximum length of ciphertext plus user-requested extra */
    size_t ciphertext_buf_len = plaintext_length + AES_BLOCK_SIZE-1 + ciphertext_padding;
    size_t encrypt_len = 0;
    size_t alloc_buf_len = ciphertext_buf_len;
    size_t alloc_iv_len = 0;

    if ((NULL == ciphertext) || (NULL == ciphertext_length) || (NULL == key) || (NULL == plaintext))
        return EINVAL;

    if (NULL == iv) {
        alloc_buf_len += AES_BLOCK_SIZE;
    }

    if ((NULL != *ciphertext) && (*ciphertext_length < alloc_buf_len))
        return ENOBUFS;

    if (NULL == cptr) {
        cptr = (unsigned char *)calloc(1, alloc_buf_len);
        if (NULL == cptr)
            return ENOMEM;
    }
    *ciphertext_length = 0;

    if (NULL == iv) {
        iv = cptr;
        eptr = cptr + AES_BLOCK_SIZE; /* put iv at start of block */

        if (1 != RAND_bytes((unsigned char *)iv, AES_BLOCK_SIZE)) {
            if (NULL == *ciphertext)
                free(cptr);
            return -1;
        }

        alloc_iv_len = AES_BLOCK_SIZE;
        fprintf(stderr, "ccn_encrypt: Generated IV\n");
    } else {
        eptr = cptr;
    }

    if (1 != EVP_EncryptInit(&ctx, EVP_aes_128_cbc(),
                             key, iv)) {
        if (NULL == *ciphertext)
            free(cptr);
        return -128;
    }

    if (1 != EVP_EncryptUpdate(&ctx, eptr, (int *)&encrypt_len, plaintext, plaintext_length)) {
        if (NULL == *ciphertext)
            free(cptr);
        return -127;
    }
    *ciphertext_length += encrypt_len;

    if (1 != EVP_EncryptFinal(&ctx, eptr+encrypt_len, (int *)&encrypt_len)) {
        if (NULL == *ciphertext)
            free(cptr);
        return -126;
    }

    /* don't include padding length in ciphertext length, caller knows its there. */
    *ciphertext_length += encrypt_len;
    *ciphertext = cptr;							   

    /*
      print_block("ccn_encrypt: key:", key, AES_BLOCK_SIZE);
      print_block("ccn_encrypt: iv:", iv, AES_BLOCK_SIZE);
      print_block("ccn_encrypt: ciphertext:", eptr, *ciphertext_length);
    */
    /* now add in any generated iv */
    *ciphertext_length += alloc_iv_len;
    return 0;
}

int SessionEnum::symDecrypt(const unsigned char *key,
							const unsigned char *iv,
							const unsigned char *ciphertext, 
							size_t ciphertext_length,
							unsigned char **plaintext, 
							size_t *plaintext_length, 
							size_t plaintext_padding) {
    EVP_CIPHER_CTX ctx;
    unsigned char *pptr = *plaintext;
    const unsigned char *dptr = NULL;
    size_t plaintext_buf_len = ciphertext_length + plaintext_padding;
    size_t decrypt_len = 0;

    if ((NULL == ciphertext) || 
        (NULL == plaintext_length) || 
        (NULL == key) ||
        (NULL == plaintext)) {
        return EINVAL;
    }

    if (NULL == iv) {
        plaintext_buf_len -= AES_BLOCK_SIZE;
    }

    if ((NULL != *plaintext) && (*plaintext_length < plaintext_buf_len))
        return ENOBUFS;

    if (NULL == pptr) {
        pptr = (unsigned char *)calloc(1, plaintext_buf_len);
        if (NULL == pptr)
            return ENOMEM;
    }

    if (NULL == iv) {
        iv = ciphertext;
        dptr = ciphertext + AES_BLOCK_SIZE;
        ciphertext_length -= AES_BLOCK_SIZE;
    } else {
        dptr = ciphertext;
    }

    /*
      print_block("ccn_decrypt: key:", key, AES_BLOCK_SIZE);
      print_block("ccn_decrypt: iv:", iv, AES_BLOCK_SIZE);
      print_block("ccn_decrypt: ciphertext:", dptr, ciphertext_length);
    */
    if (1 != EVP_DecryptInit(&ctx, EVP_aes_128_cbc(),
                             key, iv)) {
        if (NULL == *plaintext)
            free(pptr);
        return -128;
    }

    if (1 != EVP_DecryptUpdate(&ctx, pptr, 
                               (int *)&decrypt_len,
                               dptr,
                               ciphertext_length)) {
        if (NULL == *plaintext)
            free(pptr);
        return -127;
    }
    *plaintext_length = decrypt_len + plaintext_padding;
    if (1 != EVP_DecryptFinal(&ctx, pptr+decrypt_len, (int *)&decrypt_len)) {
        if (NULL == *plaintext)
            free(pptr);
        return -126;
    }
    *plaintext_length += decrypt_len;
    *plaintext = pptr;
    /* this is supposed to happen automatically,
     * but sometimes we seem to be running over the end... */
    memset(*plaintext + *plaintext_length - plaintext_padding, 0, plaintext_padding);
    return 0;
}

void SessionEnum::sendDismissSignal(Announcement *a) {
	struct ccn_charbuf *interest = ccn_charbuf_create();
	if (interest == NULL ) {
		critical("interest construction failed");
	}
	int res = ccn_name_from_uri(interest, BROADCAST_PREFIX);
	if (res < 0)
		critical("Bad ccn URI");
	
	ccn_name_append_str(interest, "dismiss");
	ccn_name_append_str(interest, a->getUuid().c_str());
	ccn_name_append_str(interest, a->getConfName().c_str());
	ccn_name_append_str(interest, a->getOrganizer().c_str());

	// fetch_announce handler should never be triggered in this case
	res = ccn_express_interest(ccn, interest, fetch_announce, NULL);
	if (res < 0) {
		critical("express dismiss interest failed!");
	}
	debug("dismiss interest sent");
	ccn_charbuf_destroy(&interest);
}

void SessionEnum::checkAlive() {
	debug("++++++SessionEnum::checkAlive()");
    list<FetchedAnnouncement *>::iterator it;
    for (it = pubConferences.begin(); it != pubConferences.end(); it++) {
        FetchedAnnouncement * fa = *it;
		if (fa == NULL)
			critical("conference announcement is null");

		if (fa->isStaled()) {
			if (!fa->isDismissed()) {
				string confName = fa->getConfName();
				string organizer = fa->getOrganizer();
				// TODO emit expired(confName, organizer);
			}
			delete fa;
			pubConferences.erase(it);
		}
	}

    for (it = priConferences.begin(); it != priConferences.end(); it++) {
        FetchedAnnouncement * fa = *it;
		if (fa == NULL)
			critical("conference announcement is NULL");

		if (fa->isStaled()) {
			if (!fa->isDismissed()) {
				string confName = fa->getConfName();
				string organizer = fa->getOrganizer();
				// TODO emit expired(confName, organizer);
			}
			delete fa;
			priConferences.erase(it);
		}
	}
}

void* SessionEnum::run(void * s) {
    SessionEnum * se = (SessionEnum *) s;
    int res = 0;
    while (se->bRunning) {
        if (res >= 0) {
            res = ccn_run(se->ccn, 5);
        }
    }
    return NULL;
}

static char *ccn_name_comp_to_str(const unsigned char *ccnb,
								  const struct ccn_indexbuf *comps,
								  int index) {
	size_t comp_size;
	const unsigned char *comp_ptr;
	char *str;
	if (ccn_name_comp_get(ccnb, comps, index, &comp_ptr, &comp_size) == 0) {
		str = (char *)malloc(sizeof(char) *(comp_size + 1));
		strncpy(str, (const char *)comp_ptr, comp_size);
		str[comp_size] = '\0';
		return str;
	}
	else {
		debug("can not get name comp");
		return NULL;
	}
}

/*
 * Handlers for incoming ccn packets
 */

static enum ccn_upcall_res incoming_content(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);

	case CCN_UPCALL_CONTENT: {
		debug("incoming public content");
		gsd->handleEnumContent(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("unverified content!");
		return (CCN_UPCALL_RESULT_OK);

	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

static enum ccn_upcall_res incoming_private_content(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info) {
	switch (kind) {
	case CCN_UPCALL_FINAL:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);

	case CCN_UPCALL_CONTENT: {
		debug("incoming private content");
		gsd->handleEnumPrivateContent(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	case CCN_UPCALL_CONTENT_UNVERIFIED:
	{
		debug("unverified content!");
		return (CCN_UPCALL_RESULT_OK);

	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

static enum ccn_upcall_res incoming_interest(struct ccn_closure *selfp,
											enum ccn_upcall_kind kind,
											struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST:
	{
		debug("incoming public interest");
		gsd->handleEnumInterest(info);		
		return (CCN_UPCALL_RESULT_OK);
	}
	
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

static enum ccn_upcall_res incoming_private_interest(struct ccn_closure *selfp,
													enum ccn_upcall_kind kind,
													struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST:
	{
		debug("incoming private interest");

		gsd->handleEnumPrivateInterest(info);		
		return (CCN_UPCALL_RESULT_OK);
	}
	
	default:
		return (CCN_UPCALL_RESULT_OK);

	}
}

static enum ccn_upcall_res dismiss_signal(struct ccn_closure *selfp,
										enum ccn_upcall_kind kind,
										struct ccn_upcall_info *info) {

	switch (kind) {
	case CCN_UPCALL_FINAL:
	case CCN_UPCALL_INTEREST_TIMED_OUT:
		return (CCN_UPCALL_RESULT_OK);
	
	case CCN_UPCALL_INTEREST: {
		// /ndn/broadcast/conference/dismiss/uuid/confName/organizer
		debug("dismiss interest received");
		gsd->handleDismissEvent(info);
		return (CCN_UPCALL_RESULT_OK);
	}
	default:
		return (CCN_UPCALL_RESULT_OK);
	}
}

static void append_bloom_filter(struct ccn_charbuf *templ, struct ccn_bloom *b) {
    ccn_charbuf_append_tt(templ, CCN_DTAG_Exclude, CCN_DTAG);
    ccn_charbuf_append_tt(templ, CCN_DTAG_Bloom, CCN_DTAG);
    int wireSize = ccn_bloom_wiresize(b);
    ccn_charbuf_append_tt(templ, wireSize, CCN_BLOB);
    ccn_bloom_store_wire(b, ccn_charbuf_reserve(templ, wireSize), wireSize);
    templ->length += wireSize;
    ccn_charbuf_append_closer(templ);
    ccn_charbuf_append_closer(templ);
}

