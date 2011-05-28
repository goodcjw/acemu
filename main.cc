#include "act.h"

#include <iostream>
#include <sstream>
using namespace std;

void testXML();

int main(int argc, char *argv[]) {
    Act a = Act("conf.act");
    a.run();
    return 0;
}

