#ifndef DATAGEN_H
#define DATAGEN_H

class DataGen {

private:
    int bitRate;
    bool bRunning;

public:
    DataGen();
    ~DataGen();
    bool isRunning() { return bRunning; }

	void startThread();
	void stopThread();    

private:
    static void* run(void * dg);
};

//static void* run(void * dg);

#endif // DATAGEN_H
