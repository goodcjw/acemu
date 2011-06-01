#include <signal.h>

#include <iostream>
#include <sstream>
using namespace std;

#include "act.h"

void testXML();

static Act * a;

void exit_func (int i) {
    signal(SIGINT,exit_func);
    delete a;
    printf("\nBye Bye ActEmu\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    
    signal(SIGINT,exit_func);
    a = new Act("conf.act");
    a->run();
    return 0;
}

