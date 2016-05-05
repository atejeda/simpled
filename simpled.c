/*
 TODO
 - Needs error checking everywhere (forks, descriptors, streams)
 - Lot of code needs to be refactored in reusable functions
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
        char pidc[5];
        fgets(pidc, 5, pidfs);
        close(pidfs);
        
        char *endp;
        int pid = strtol(pidc, &endp, 10);

        if (kill(pid, 0) == -1) {
            return 1;
        }

        return 0;
    } else {
        return 1;
    } 
}

void killd(const char *pidf) {
    if (access(pidf, F_OK ) != -1) {
        FILE *pidfs = fopen(pidf, "r");
        char pidc[5];
        fgets(pidc, 5, pidfs);
        close(pidfs);
        
        char *endp;
        int pid = strtol(pidc, &endp, 10);

        kill(pid, SIGINT);
        remove(pidf);
    } 
}

int main(int argc, char *argv[], char *envp[]) {
    char helpm[90];
    char *helpc = "Usage: %s -p /path/pidfile -l /path/logfile -a {start|stop|status} -- /path/exec [args]\n";
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
            if (!strcmp(optarg, "start") || !strcmp(optarg, "status") || !strcmp(optarg, "stop")) {
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

    if (!strcmp(iargs.action, "status")) {
        if (!isalive(iargs.pidf)) {
            exit(EXIT_SUCCESS);
        } else {
            exit(EXIT_FAILURE);
        }
    }

    if (!strcmp(iargs.action, "stop")) {
        killd(iargs.pidf);
        exit(EXIT_SUCCESS);
    }

    /* +2 = argv[0], command */
    if (optind < 7 || argc < optind + 2) {
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

        pid_t daemonpid = -9;
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
                daemonpid = strtol(buffer, &endp, 10);
                dprintf(STDOUT_FILENO, "pid = %s\n", buffer);
                break;
            }
        }

        /* check if process really exists*/
        sleep(3);

        if (kill(daemonpid, 0) == -1) {
            fprintf(stderr, "Doesn't seems to be alive, check log file: %s\n", iargs.logf);
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
    int logfd = open(iargs.logf, O_APPEND | O_NONBLOCK | O_SYNC | O_WRONLY | O_CREAT, 0640);
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

    if (flock(open(iargs.pidf, O_WRONLY), LOCK_EX) == -1) {
        fprintf(stderr, "file lock: %s", strerror(errno));
        dprintf(epiped[1], "file lock: %s", strerror(errno));
        _exit(EXIT_FAILURE);
    }

    time_t t = time(NULL);
    struct tm *local = gmtime(&t);
    dprintf(spiped[1], "%i", pid);
    printf("[%s] %s", pidc, asctime(local));

    printf("\n");

    umask(0);
    chdir("/");

    if (execve(iargs.fexec, iargs.fargs, envp) == -1) {
        // refactor it to use dprintf instead
        char execveerr[512];
        sprintf(execveerr, "%s: %s\n", iargs.fexec, strerror(errno));

        fprintf(stderr, execveerr);
        dprintf(epiped[1], execveerr);

        flock(open(iargs.pidf, O_WRONLY), LOCK_UN);

        remove(iargs.pidf);
        
        _exit(EXIT_FAILURE);
    }

    return 0;
}

/*
rm -rf simpled &&  gcc -std=gnu99 -w simpled.c -o simpled && ./simpled  -l $PWD/ping.log -p $PWD/ping.pid -- `which ping` -c4 localhost



#include <stdio.h>
#include <string.h>

int main ()
{
   int ret;
   FILE *fp;
   char filename[] = "file.txt";

   fp = fopen(filename, "w");

   fprintf(fp, "%s", "This is tutorialspoint.com");
   fclose(fp);
   
   ret = remove(filename);

   if(ret == 0) 
   {
      printf("File deleted successfully");
   }
   else 
   {
      printf("Error: unable to delete the file");
   }
   
   return(0);
}

*/