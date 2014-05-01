/*
 * execute.c
 *
 * Copyright (C) Jon Turney 2015
 *
 * This file is part of xwin-xdg-menu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

//
// execute the command for a .desktop entry, logging it's output and exit
// status
//
// Stolen from xserver hw/xwin/winprefs.c
//

#include "execute.h"
#include "menu.h"
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <gio/gdesktopappinfo.h>

static void
LogLineFromFd(int fd, const char *fdname, int pid)
{
#define BUFSIZE 512
    char buf[BUFSIZE];
    char *bufptr = buf;

    /* read from fd until eof, newline or our buffer is full */
    while ((read(fd, bufptr, 1) > 0) && (bufptr < &(buf[BUFSIZE - 1]))) {
        if (*bufptr == '\n')
            break;
        bufptr++;
    }

    /* null terminate and log */
    *bufptr = 0;
    if (strlen(buf))
        printf("(pid %d %s) %s\n", pid, fdname, buf);
}

static void *
ExecAndLogThread(void *cmd)
{
    int pid;
    int stdout_filedes[2];
    int stderr_filedes[2];
    int status;

    /* Create a pair of pipes */
    pipe(stdout_filedes);
    pipe(stderr_filedes);

    switch (pid = fork()) {
    case 0: /* child */
    {
        struct rlimit rl;
        unsigned int fd;

        /* dup write end of pipes onto stderr and stdout */
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        dup2(stdout_filedes[1], STDOUT_FILENO);
        dup2(stderr_filedes[1], STDERR_FILENO);

        /* Close any open descriptors except for STD* */
        getrlimit(RLIMIT_NOFILE, &rl);
        for (fd = STDERR_FILENO + 1; fd < rl.rlim_cur; fd++)
            close(fd);

        /* Disassociate any TTYs */
        setsid();

        execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
        perror("execl failed");
        exit(127);
    }
    break;

    default: /* parent */
    {
        close(stdout_filedes[1]);
        close(stderr_filedes[1]);

        printf("executing '%s', pid %d\n", (char *) cmd, pid);

        /* read from pipes, write to log, until both are closed */
        while (TRUE) {
            fd_set readfds, errorfds;
            int nfds = max(stdout_filedes[0], stderr_filedes[0]) + 1;

            FD_ZERO(&readfds);
            FD_SET(stdout_filedes[0], &readfds);
            FD_SET(stderr_filedes[0], &readfds);
            errorfds = readfds;

            if (select(nfds, &readfds, NULL, &errorfds, NULL) > 0) {
                if (FD_ISSET(stdout_filedes[0], &readfds))
                    LogLineFromFd(stdout_filedes[0], "stdout", pid);
                if (FD_ISSET(stderr_filedes[0], &readfds))
                    LogLineFromFd(stderr_filedes[0], "stderr", pid);

                if (FD_ISSET(stdout_filedes[0], &errorfds) &&
                    FD_ISSET(stderr_filedes[0], &errorfds))
                    break;
            }
            else {
                break;
            }
        }

        waitpid(pid, &status, 0);
        printf("pid %d exited with status %x\n", pid, status);;
    }
    break;

    case -1: /* error */
        printf("fork() to run command failed\n");
    }

    return (void *) (intptr_t) status;
}

void
menu_item_execute(int id)
{
  GDesktopAppInfo *appinfo = menu.appinfo[id-1];
  const char *cCmd = g_app_info_get_executable(G_APP_INFO(appinfo));

  pthread_t t;
  if (!pthread_create(&t, NULL, ExecAndLogThread, (void *)cCmd))
    pthread_detach(t);
  else
    printf("Creating command output logging thread failed\n");
}
