#ifndef DATAGEN_H
#define DATAGEN_H

#include <string>
using namespace std;

class DataGen {

private:
    int bitRate;
    int sampleRate;
    bool bRunning;
    string confName;
    string speakName;

public:
    DataGen();
    DataGen(string t_confName, string t_speakName);
    ~DataGen();
    bool isRunning() { return bRunning; }

    void startThread();
    void stopThread();    

    void setConfName(string t_confName) {confName = t_confName;}
    void setSpeakName(string t_speakName) {speakName = t_speakName;}

private:
    static void* run(void * dg);
};

//static void* run(void * dg);

#endif // DATAGEN_H
