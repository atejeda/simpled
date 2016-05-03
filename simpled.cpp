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

    /* manage pipe redirection */
    if (pipe(pipefd) == -1)
        return -1;

    int fid = fork();

    if (fid == -1) {
        return -1;

    } else if (fid == 0) {

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        pid_t pid = getpid();

        // create the lockfile and write the pid

        umask(0);
        chdir("/");

        if (execve(fexec, fargs, envp) == -1) {
            return -1;
        }

    } else {

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        short buffer_size = 1024;
        char buffer[buffer_size];
        ssize_t readn;

        const char *fname = "/export/home/valhalla/atejeda/Desktop/simpled/output.log";
        // FILE *fp = fopen(fname, "a+");
        // int fd = fileno(fp);

        int fd = open(fname, O_APPEND | O_NONBLOCK | O_SYNC | O_WRONLY | O_CREAT, 0640);

        //setbuf(fp, NULL);

        //setvbuf(fp, NULL, _IOLBF, 0);

        for(;;) {
            readn = read(STDIN_FILENO, buffer, buffer_size);
            if (readn == 0) 
                break;
            if (readn == -1) 
                break;
            if (write(fd, buffer, readn) != readn) {
                break;
            }
        }

        //write(STDIN_FILENO, "\n", 1);
        close(fd);
    }

    //cout << "done..." << endl;
    /* portable? */
    /* exit(EXIT_FAILURE); */
    /* exit(EXIT_SUCESS); */
    return 0;
}