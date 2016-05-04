#ifndef __linux__
#error "Only linux is supported"
#endif

#include <iostream>

#include <cstdlib> 
#include <cstring>
#include <cassert>
#include <cstdio>

#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

/**
 * argv[1]   = pid and lock path
 * argv[2]   = absolute path to the exec file to daemonize,
 *             script must start with #!/bin/env {exec}
 * argv[...] = arguments for the app to daemonize if are needed
 */
int main(int argc, char *argv[], char *envp[]) {

    // lazy validation
    if (argc < 2) {
        cerr << argv[0] << " {exec} \"[args]\"" << endl;
        return -1; //exit(EXIT_FAILURE);
    }

    char *pidf = argv[1];
    char *fexec = argv[2];
    char **fargs = argv + 2;

    /* 0 read, 1 write */
    int pipefd[2];
    
    switch (fork()) {
    case -1: 
        return -1;
    case 0: 
        break;
    default: 
        return 0;
    }

    if(setsid() == -1)
        return -1;

    /* disable the possibility to be a controlling process */
    switch (fork()) {
    case -1: 
        return -1;
    case 0:  
        break;
    default:
        return 0;
    }

    const char *fname = "/home/atejeda/Desktop/daemon/output.log";
    int fd = open(fname, O_APPEND | O_NONBLOCK | O_SYNC | O_WRONLY | O_CREAT, 0640);

    setvbuf(stdout, NULL, _IOLBF, 1024);

    dup2(fd, STDOUT_FILENO);
    //dup2(fd, STDERR_FILENO);
    close(fd);

    pid_t pid = getpid();

    // create the lockfile and write the pid

    umask(0);
    chdir("/");

    printf("I'm about to start ....");

    if (execve(fexec, fargs, envp) == -1) {
        return -1;
    }

    return 0; /* exit(EXIT_SUCESS); , exit(EXIT_FAILURE); */
}