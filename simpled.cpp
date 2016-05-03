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

    /* manage pipe redirection, parent process read, child writes */

    if (pipe(pipefd) == -1)
        return -1;

    int fid = fork();

    if (fid == -1) {
        return -1;
    } else if (fid == 0) {
        /* pipe pipefd writer, child */
        if (close(pipefd[0]))
            return -1;

        if (pipefd[1] != STDOUT_FILENO) {
            if (dup2(pipefd[1], STDOUT_FILENO) == -1)
                return -1;
            if (close(pipefd[1]) == -1)
                return -1;
        }

        pid_t pid = getpid();

        umask(0);
        chdir("/");

        if (execve(fexec, fargs, envp)
            return -1;
    } else {
        /* pipe pipefd reader, parent */
        close(pipefd[1]);
        if (pipefd[1] != STDIN_FILENO) {
            if (dup2(pipefd[0], STDIN_FILENO) == -1)
                return -1;
            if (close(pipefd[1]) == -1)
                return -1;
        }
    }

    /* portable? */
    /* exit(EXIT_FAILURE); */
    /* exit(EXIT_SUCESS); */
    return 0;
}