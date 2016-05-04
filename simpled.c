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
    printf("[ ");
    for (char **c = args; *c != NULL; c++)
        printf("%s ", *c);
    printf("]\n");
}

int main(int argc, char *argv[], char *envp[]) {
    char helpm[90];
    sprintf(helpm, "Usage: %s -p /path/pidfile -l /path/logfile -- /path/exec [args]\n", argv[0]);

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

    /* start to daemonize process */

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

    /* avoid to be SID leader */
    switch (fork()) {
    case -1: 
        return -1;
    case 0:  
        break;
    default:
        return 0;
    }

    /* redirect stdout to the logfile */
    int logfd = open(iargs.logf, O_APPEND | O_NONBLOCK | O_SYNC | O_WRONLY | O_CREAT, 0640);
    setvbuf(stdout, NULL, _IOLBF, 1024);
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);

    argvp(iargs.fargs);

    /* register flock process pid */

    pid_t pid = getpid();
    char pidc[8];
    sprintf(pidc, "%d", pid);

    FILE *pidff = fopen(iargs.pidf, "w+");
    fputs(pidc, pidff);
    
    int pidfd = fileno(pidff);
    if (flock(pidfd, LOCK_EX) == -1) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "already locked...");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "error locking the file");
    }

    fclose(pidff);

    time_t t = time(NULL);
    struct tm *local = gmtime(&t);
    printf("PID = %s, UTC = %s", pidc, asctime(local));

    umask(0);
    chdir("/");

    if (execve(iargs.fexec, iargs.fargs, envp) == -1) {
        // flock(int fd, LOCK_UN); 
        exit(EXIT_FAILURE);
    }

    return 0;
}

/*
rm -rf simpled &&  gcc -std=gnu99 -w simpled.c -o simpled && ./simpled  -l $PWD/ping.log -p $PWD/ping.pid -- `which ping` -c4 localhost
*/

/*
The best solution I know is using pipes to communicate the success or failure of exec:

    Before forking, open a pipe in the parent process.
    After forking, the parent closes the writing end of the pipe and reads from the reading end.
    The child closes the reading end and sets the close-on-exec flag for the writing end.
    The child calls exec.
    If exec fails, the child writes the error code back to the parent using the pipe, then exits.
    The parent reads eof (a zero-length read) if the child successfully performed exec, since close-on-exec made successful exec close the writing end of the pipe. Or, if exec failed, the parent reads the error code and can proceed accordingly. Either way, the parent blocks until the child calls exec.
    The parent closes the reading end of the pipe.

*/