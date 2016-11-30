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
        if (WIFEXITED(status))
          printf("pid %d exited with status %d\n", pid, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
          printf("pid %d terminated by signal %d\n", pid, WTERMSIG(status));
        else
          printf("pid %d status 0x%x\n", pid, status);
    }
    break;

    case -1: /* error */
        printf("fork() to run command failed\n");
    }

    free(cmd);

    return (void *) (intptr_t) status;
}

static void
execute_cmd(char *cmd)
{
  // note that free() will be applied to cmd after the command has exited
  pthread_t t;
  if (!pthread_create(&t, NULL, ExecAndLogThread, (void *)cmd))
    pthread_detach(t);
  else
    printf("Creating command output logging thread failed\n");
}

static void
menu_cmd_add_text(unsigned int *j, char **cmd, const char *add)
{
  if (add)
    {
      *cmd = realloc(*cmd, strlen(add) + *j);
      memcpy(&(*cmd)[*j], add, strlen(add));
      *j = *j + strlen(add);
    }
}

void
menu_item_execute(int id)
{
  GDesktopAppInfo *appinfo = menu_get_appinfo(id);
  const char *fmt = g_app_info_get_commandline(G_APP_INFO(appinfo));

  // process field codes
  char *cmd = malloc(1);
  unsigned int i, j = 0;

  if (g_desktop_app_info_get_boolean(appinfo, "DBusActivatable"))
    {
      const char *filename = g_desktop_app_info_get_filename(appinfo);
      char *bus_name = g_path_get_basename (filename);
      bus_name[strlen(bus_name) - strlen(".desktop")] = '\0';
      asprintf(&cmd, "gapplication launch %s", bus_name);
      execute_cmd(cmd);
      return;
    }

  if (g_desktop_app_info_get_boolean(appinfo, "Terminal"))
    menu_cmd_add_text(&j, &cmd, "xterm -e ");

  for (i = 0; i < strlen(fmt) + 1; i++)
    {
      if (fmt[i] == '%')
        {
          i++;
          if (i < strlen(fmt))
            {
              switch (fmt[i])
                {
                case '%':
                  // field code %% is an escaped %
                  cmd = realloc(cmd, j+1);
                  cmd[j] = '%';
                  j++;
                  break;
                case 'c':
                  // %c Name key from desktop entry
                  {
                    const char *name = g_app_info_get_display_name(G_APP_INFO(appinfo));
                    menu_cmd_add_text(&j, &cmd, "\"");
                    menu_cmd_add_text(&j, &cmd, name);
                    menu_cmd_add_text(&j, &cmd, "\"");
                  }
                  break;
                case 'i':
                  // %i Icon key following '--icon '
                  {
                    GIcon *icon = g_app_info_get_icon(G_APP_INFO(appinfo));
                    char *name = g_icon_to_string(icon);
                    if (name)
                      {
                        menu_cmd_add_text(&j, &cmd, "--icon ");
                        menu_cmd_add_text(&j, &cmd, name);
                      }
                    g_free(icon);
                    g_free(name);
                  }
                  break;
                case 'k':
                  // %k location of desktop entry file
                  {
                    const char *filename = g_desktop_app_info_get_filename(appinfo);
                    menu_cmd_add_text(&j, &cmd, filename);
                  }
                  break;
                case 'f':
                case 'u':
                case 'F':
                case 'U':
                default:
                  // We don't support launching with file(s) or URL(s) so we should
                  // remove any %f %u %F %U.
                  // Deprecated field codes can also be removed
                  ;
                }
            }
        }
      else
        {
          cmd = realloc(cmd, j+1);
          cmd[j] = fmt[i];
          j++;
        }
    }

  // XXX: unquoting ???

  execute_cmd(cmd);
}

void
view_logfile_execute(void)
{
  char logfile[PATH_MAX+1];
  ssize_t l = readlink("/proc/self/fd/1", logfile, PATH_MAX);
  if (l > 0)
    {
      logfile[l] = 0; // readlink does not null terminate it's result
      char *cmd = NULL;
      asprintf(&cmd, "xterm -title '%s' -e less +F %s", logfile, logfile);
      execute_cmd(cmd);
    }
}
