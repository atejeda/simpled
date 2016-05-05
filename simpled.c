/**
    ```
    rm -rf simpled && gcc -std=gnu99 -w simpled.cpp -o simpled && ./simpled $PWD/ping.pid $PWD/ping.log -- `which ping` -c4 localhost
    ````
*/

#ifndef __linux__
#error "Only linux is supported"
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#include <stdbool.h>
#include <time.h>

#include <unistd.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
    char *pidf;
    char *logf;
    char *fexec;
    char **fargs;
} iargs_t;

void argvp(const char **args) {
    for (char **c = args; *c != NULL; c++)
        printf("%s ", *c);
    printf("\n");
}

int main(int argc, char *argv[], char *envp[]) {
    char helpm[90];
    char *helpc = "Usage: %s -p /path/pidfile -l /path/logfile -- /path/exec [args]\n";
    sprintf(helpm, helpc, argv[0]);

    iargs_t iargs;

    int opt;
    while ((opt = getopt(argc, argv, "p:l:")) != -1) {
        switch (opt) {
        case 'p':
            iargs.pidf = optarg;
            break;
        case 'l':
            iargs.logf = optarg;
            break;  
        default:
            fprintf(stderr, helpm);
            exit(EXIT_FAILURE);
        }
    }

    /* +2 = argv[0], command */
    if (optind < 4 || argc < optind + 2) {
        fprintf(stderr, helpm);
        exit(EXIT_FAILURE);
    }

    /* double dash */
    bool ddash = false;
    for (char **c = argv; *c != NULL; c++) {
        if (ddash = !strcmp(*c, "--"))
            break;
    }

    if (!ddash) {
        fprintf(stderr, helpm);
        exit(EXIT_FAILURE);
    }

    iargs.fexec = argv[optind];
    iargs.fargs = argv + optind;

    argvp(iargs.fargs);

    /* 0 r, 1 w*/
    int epiped[2];
    pipe2(epiped, O_CLOEXEC);

    int spiped[2];
    pipe2(spiped, O_CLOEXEC);

    /* start to daemonize process */

    switch (fork()) {
    case -1: 
        exit(EXIT_FAILURE);
    case 0: 
        break;
    default:
        close(epiped[1]);
        close(spiped[1]);

        ssize_t readn;
        char buffer[512];

        /* check error */
        for (;;) {
            readn = read(epiped[0], buffer, sizeof(buffer));
            switch(readn) {
            case -1:
                fprintf(stderr, "%s: %s", iargs.fexec);
                close(epiped[0]);
                close(spiped[0]);
                exit(EXIT_FAILURE);
            case 0:
                close(epiped[0]);
                break; // end of file
            default:
                // write... readn
                dprintf(STDOUT_FILENO, buffer);
                close(epiped[0]);
                close(spiped[0]);
                exit(EXIT_FAILURE);
            }
        }

        /* read success */
        for (;;) {
            readn = read(spiped[0], buffer, sizeof(buffer));
            switch(readn) {
            case -1:
                fprintf(stderr, "%s: %s", iargs.fexec);
                close(epiped[0]);
                close(spiped[0]);
                exit(EXIT_FAILURE);
            case 0:
                close(spiped[0]);
                break;
            default:
                dprintf(STDOUT_FILENO, buffer);
                close(epiped[0]);
                close(spiped[0]);
                exit(EXIT_FAILURE);
            }
        }
        
        exit(EXIT_SUCCESS);
    }

    if(setsid() == -1)
        return -1;

    /* avoid to be SID leader */
    switch (fork()) {
    case -1: 
        exit(EXIT_FAILURE);
    case 0:
        close(epiped[0]);
        close(spiped[0]);  
        break;
    default:
        close(epiped[1]);
        close(epiped[0]);
        close(spiped[1]);
        close(spiped[0]);
        exit(EXIT_SUCCESS);
    }

    /* redirect stdout, stderr to the logfile */
    int logfd = open(iargs.logf, O_APPEND | O_NONBLOCK | O_SYNC | O_WRONLY | O_CREAT, 0640);
    setvbuf(stdout, NULL, _IOLBF, 1024);
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);

    argvp(iargs.fargs);

    /* register flock and process pid */

    pid_t pid = getpid();
    char pidc[8];
    sprintf(pidc, "%d", pid);

    FILE *pidff = fopen(iargs.pidf, "w+");
    fputs(pidc, pidff);
    fclose(pidff);

    if (flock(open(iargs.pidf, O_WRONLY), LOCK_EX) == -1) {
        fprintf(stderr, "file lock: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    time_t t = time(NULL);
    struct tm *local = gmtime(&t);
    dprintf("[%s]", pidc);
    printf("[%s] %s", pidc, asctime(local));

    umask(0);
    chdir("/");

    if (execve(iargs.fexec, iargs.fargs, envp) == -1) {
        // refactor it to use dprintf instead
        char execveerr[512];
        sprintf(execveerr, "%s: %s\n", iargs.fexec, strerror(errno));

        fprintf(stderr, execveerr);
        dprintf(epiped[1], execveerr);

        flock(open(iargs.pidf, O_WRONLY), LOCK_UN);

        // delete pid file
        
        _exit(EXIT_FAILURE);
    }

    return 0;
}

/*
rm -rf simpled &&  gcc -std=gnu99 -w simpled.c -o simpled && ./simpled  -l $PWD/ping.log -p $PWD/ping.pid -- `which ping` -c4 localhost
*/

/*

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

The best solution I know is using pipes to communicate the success or failure of exec:

    Before forking, open a pipe in the parent process.
    After forking, the parent closes the writing end of the pipe and reads from the reading end.
    The child closes the reading end and sets the close-on-exec flag for the writing end.
    The child calls exec.
    If exec fails, the child writes the error code back to the parent using the pipe, then exits.
    The parent reads eof (a zero-length read) if the child successfully performed exec, since close-on-exec made successful exec close the writing end of the pipe. Or, if exec failed, the parent reads the error code and can proceed accordingly. Either way, the parent blocks until the child calls exec.
    The parent closes the reading end of the pipe.

*/