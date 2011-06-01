#include <time.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "dump.h"

#define BUFFER_LEN 128

Dump::Dump() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[BUFFER_LEN];
    int r;

    time(&rawtime);
    timeinfo = gmtime(&rawtime);
    strftime(buffer, 80, "ndn-%Y%m%d-%H%M%S.tr", timeinfo);

    string str_time = string(buffer);
    r = gethostname(buffer, BUFFER_LEN);
    string str_hostname = string(buffer);
    string filename = str_hostname + "-" + str_time;

    f.open(filename.c_str());
}

Dump::~Dump() {
    f.close();
}

void Dump::putline(string line) {
    f << line << endl;
    f.flush();
}

void Dump::putline(const char * line) {
    f << string(line) << endl;
    f.flush();
}
