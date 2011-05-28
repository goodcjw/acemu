#include "debugbox.h"

#include <iostream>
using namespace std;

void debug(string msg) {
#ifdef DEBUG
	cout << "+++++Debug: "<<  msg << "\n";
    cout.flush();
#endif    
}

void critical (string msg) {
	cout << "+++++Error: "<<  msg << "\n";
    cout.flush();
	exit(1);
}
