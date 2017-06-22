# simple daemonizer

## Description

```
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
 * Build it: gcc -std=gnu99 -w simpled.c -o simpled
 *
 * @see https://github.com/atejeda/simpled
 * @see http://man7.org/linux/man-pages/index.html
 */
```
## Build

```
gcc -std=gnu99 -w simpled.c -o simpled
```

## Usage

All paths, I mean all paths must be absolute, including the argumens used by the application to daemonize.

```
Usage: simpled -p /path/pidfile -l /path/logfile -a {start|stop|status} -- /path/exec [args]
```

* start: return 0 on success anything else on error
* stop: return 0, removes the pid file and kill the process with SIGKILL
* status: return 0 if process exists based on the pid written on the pidfile, anything else on error

All the stdout and stderr process to be daemonized is redirected to the logfile. I have disabled chdir to / so this might block your mounted dirs.
