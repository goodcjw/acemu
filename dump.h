#ifndef DUMP_H
#define DUMP_H

#include <string>
#include <fstream>
using namespace std;

class Dump {

private:
    ofstream f;
private:

public:
    Dump();
    ~Dump();
    void putline(string line);
    void putline(const char * line);
};

#endif

/*
int main () {
  ofstream myfile;
  myfile.open ("example.txt");
  myfile << "Writing this to a file.\n";
  myfile.close();
  return 0;
}
*/
