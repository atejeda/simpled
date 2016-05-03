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

        short buffer_size = 10;
        char buffer[buffer_size];
        ssize_t readn;

        FILE *fp = fopen("/tmp/test.txt", "w+");
        int fpd = fileno(fp);
        //int fsync(int fd);
        //int fdatasync(int fd);

        
        for(;;) {

            readn = read(STDIN_FILENO, buffer, buffer_size);
            if (readn == 0) 
                ;//break;
            if (readn == -1) 
                break;
            if (write(fpd, buffer, buffer_size) != readn) {
                break;
            }
        }

        fclose(fp); 
    }

    //cout << "done..." << endl;
    /* portable? */
    /* exit(EXIT_FAILURE); */
    /* exit(EXIT_SUCESS); */
    return 0;
}