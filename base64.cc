#include "base64.h"

#include <string.h>
extern "C" {
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
}

char *unbase64(const unsigned char *input, int length)
{
    BIO *b64, *bmem;

    char *inbuff = (char *)malloc(length + 1);
    memcpy(inbuff, input, length);
    inbuff[length] = '\n';

    char *buffer = (char *)malloc(length);
    memset(buffer, 0, length);

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf((void *)inbuff, length + 1);
    bmem = BIO_push(b64, bmem);

    BIO_read(bmem, buffer, length);
    BIO_free_all(bmem);

    free(inbuff);

    return buffer;
}

char *base64(const unsigned char *input, int length)
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
