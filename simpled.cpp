/*
    Let any given application to run as daemon.
    Sources:
*/

#ifndef __cplusplus
#error "Is a c++ source"
#endif

#ifndef __linux__
#error "Only linux is supported"
#endif

#include <iostream>

#include <cstdlib> 
#include <cstring>
#include <cassert>

#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

int main(int argc, char *argv[], char *envp[]) {
    // argv[1] = pid and lock path
    // argv[2] =  

    // lazy validation
    if (argc < 2) {
        cerr << argv[0] << " {exec} \"[args]\"" << endl;
        return -1; //exit(EXIT_FAILURE);
    }

    // abs exec path, script must start with #!/bin/env {exec}
    const char *fexec = argv[1];
    // arguments offset + 2
    char **fargs =  argv + 1;

    // switch (fork()) {
    // case -1: 
    //     return -1; //exit(EXIT_FAILURE);
    // case 0: 
    //     break;
    // default: 
    //     return 0; //exit(EXIT_SUCESS);
    // }

    // if(setsid() == -1)
    //     return -1; //exit(EXIT_FAILURE);

    // switch (fork()) {
    // case -1: 
    //     return -1; //exit(EXIT_FAILURE);
    // case 0:  
    //     break;
    // default: 
    //     return 0; //exit(EXIT_SUCESS);
    // }

    // pid_t pid = getpid();

    // umask(0);
    // chdir("/");
    
    // // char *const args[];

    if (execve(fexec, fargs, envp)) {
        cerr << "Error" << endl;
    }

    // return 0; //exit(EXIT_SUCESS);
}
