#ifndef BASE64_H
#define BASE64_H

char *base64(const unsigned char *input, int length);
char *unbase64(const unsigned char *input, int length);

#endif // BASE64_H
