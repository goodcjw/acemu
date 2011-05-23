#include "debugbox.h"

#include <iostream>
using namespace std;

void debug(string msg) {
	cout << "+++++Debug: "<<  msg << "\n";
}

void critical (string msg) {
	debug(msg);
	exit(1);
}
