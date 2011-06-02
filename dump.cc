#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include <iostream>
using namespace std;

#include "dump.h"
#include "debugbox.h"

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
    debug("~Dump");
    f.close();
}

void Dump::putline(string line) {
    struct timeval t_now;
    char str_now[BUFFER_LEN];

    gettimeofday(&t_now, NULL);
    sprintf(str_now, "%d.%d, ", (int)t_now.tv_sec, (int)t_now.tv_usec);

    f << string(str_now) << line << endl;
}

void Dump::putline(const char * line) {
    f << string(line) << endl;
    f.flush();
}
