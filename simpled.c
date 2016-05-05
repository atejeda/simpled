/**
 * Process/application daemonizer for linux systems
 * Copyright (C) 2016 https://github.com/atejeda/simpled
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file simpled.c
 * @author https://github.com/atejeda/simpled
 * @date May 2016
 * @brief Process/application daemonizer for linux systems (portable?).
 *
 * This tiny C application is a process daemonizer, it executes it as a deamon
 * (system background "?") with no terminal attached and with stdout and 
 * stderr redirection to a log file, also saves the pid in a file that is used
 * as a lock file (flock) as well.
 * Even though it works, there's serveral things needs to be improved like
 * code reusage by using functions (DRY approach) and error checking for
 * the system calls, this daemon can be used for learning purposes, use it
 * under your own responsability.
 * Build it by: gcc -std=gnu99 -w simpled.c -o simpled.
 *
 * @see https://github.com/atejeda/simpled
 * @see http://man7.org/linux/man-pages/index.html
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
#include <signal.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
    char *pidf;
    char *logf;
    char *fexec;
    char **fargs;
    char *action;
} iargs_t;

void argvp(const char **args) {
    for (char **c = args; *c != NULL; c++)
        printf("%s ", *c);
    printf("\n");
}

int isalive(const char *pidf) {
    if (access(pidf, F_OK) != -1) {
        FILE *pidfs = fopen(pidf, "r");
        char pidc[6];
        fgets(pidc, 6, pidfs);
        close(pidfs);
        
        char *endp;
        int pid = strtol(pidc, &endp, 10);
        printf("%i\n", pid);

        if (kill(pid, 0) == -1)
            return 1;
        return 0;
    } else {
        return 1;
    } 
}

void killd(const char *pidf) {
    if (access(pidf, F_OK ) != -1) {
        FILE *pidfs = fopen(pidf, "r");
        char pidc[6];
        fgets(pidc, 6, pidfs);
        close(pidfs);
        
        char *endp;
        int pid = strtol(pidc, &endp, 10);

        printf("%i\n", pid);

        kill(pid, SIGKILL);
        remove(pidf);
    } 
}

int main(int argc, char *argv[], char *envp[]) {
    char helpm[90];
    char *helpc = "Usage: %s -p "
                  "/path/pidfile -l "
                  "/path/logfile -a {start|stop|status}"
                  "-- "
                  "/path/exec [args]\n";  
    sprintf(helpm, helpc, argv[0]);

    iargs_t iargs;

    int opt;
    while ((opt = getopt(argc, argv, "p:l:a:")) != -1) {
        switch (opt) {
        case 'p':
            iargs.pidf = optarg;
            break;
        case 'l':
            iargs.logf = optarg;
            break;
        case 'a':
            if (!strcmp(optarg, "start") || 
                !strcmp(optarg, "status") || 
                !strcmp(optarg, "stop")) {
                iargs.action = optarg;
            } else {
                fprintf(stderr, helpm);
                exit(EXIT_FAILURE); 
            }
            break;
        default:
            fprintf(stderr, helpm);
            exit(EXIT_FAILURE);
        }
    }

    /* get status */
    if (!strcmp(iargs.action, "status")) {
        if (!isalive(iargs.pidf)) {
            exit(EXIT_SUCCESS);
        } else {
            exit(EXIT_FAILURE);
        }
    }

    /* stop daemon process */
    if (!strcmp(iargs.action, "stop")) {
        killd(iargs.pidf);
        exit(EXIT_SUCCESS);
    }

    /* +2 = argv[0], command */
    if (optind < 7 || argc < optind + 2) {
        fprintf(stderr, helpm);
        exit(EXIT_FAILURE);
    }

    /* double dash option checking */
    bool ddash = false;
    for (char **c = argv; *c != NULL; c++) {
        if (ddash = !strcmp(*c, "--"))
            break;
    }

    if (!ddash) {
        fprintf(stderr, helpm);
        exit(EXIT_FAILURE);
    }

    /* so far so good */

    iargs.fexec = argv[optind];
    iargs.fargs = argv + optind;

    argvp(iargs.fargs);

    /* epiped for errors, and spiped for get the double forked process pid */
    int epiped[2];
    int spiped[2];
    pipe2(epiped, O_CLOEXEC);
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

        bool dobreak;

        /* check error */
        dobreak = false;
        for (; !dobreak;) {
            readn = read(epiped[0], buffer, sizeof(buffer));
            switch(readn) {
            case -1:
                fprintf(stderr, "%s: %s", iargs.fexec);
                close(epiped[0]);
                close(spiped[0]);
                exit(EXIT_FAILURE);
            case 0:
                dobreak = true; 
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

        pid_t dpid = -9;
        char *endp;

        dobreak = false;
        for (; !dobreak;) {
            readn = read(spiped[0], buffer, sizeof(buffer));
            switch(readn) {
            case -1:
                fprintf(stderr, "%s: %s", iargs.fexec);
                close(epiped[0]);
                close(spiped[0]);
                exit(EXIT_FAILURE);
            case 0:
                dobreak = true;
                close(spiped[0]);
                break;
            default:
                dpid = strtol(buffer, &endp, 10);
                dprintf(STDOUT_FILENO, "pid = %s\n", buffer);
                break;
            }
        }

        /* check if process really exists*/
        sleep(3);

        if (kill(dpid, 0) == -1) {
            fprintf(stderr, "Is alive?, check log file: %s\n", iargs.logf);
            exit(EXIT_FAILURE);
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
    int logfd_f =  O_APPEND | O_NONBLOCK | O_SYNC | O_WRONLY | O_CREAT;
    int logfd = open(iargs.logf, logfd_f, 0640);
    setvbuf(stdout, NULL, _IOLBF, 1024);
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);

    printf("\n");
    argvp(iargs.fargs);

    /* register flock and process pid */

    pid_t pid = getpid();
    char pidc[8];
    sprintf(pidc, "%d", pid);

    FILE *pidff = fopen(iargs.pidf, "w+");
    fputs(pidc, pidff);
    fclose(pidff);

    int pidfdl = open(iargs.pidf, O_WRONLY);
    if (flock(pidfdl, LOCK_EX) == -1) {
        fprintf(stderr, "file lock: %s", strerror(errno));
        dprintf(epiped[1], "file lock: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    time_t t = time(NULL);
    struct tm *local = gmtime(&t);
    dprintf(spiped[1], "%i", pid);
    printf("[%s] %s\n", pidc, asctime(local));

    umask(0);
    chdir("/");

    if (execve(iargs.fexec, iargs.fargs, envp) == -1) {
        char execveerr[512];
        sprintf(execveerr, "%s: %s\n", iargs.fexec, strerror(errno));
        fprintf(stderr, execveerr);
        dprintf(epiped[1], execveerr);

        flock(pidfdl, LOCK_UN);

        remove(iargs.pidf);
        
        _exit(EXIT_FAILURE);
    }

    return 0;
}