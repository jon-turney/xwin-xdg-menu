/*
 * main.c
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

#include "menu.h"
#include "msgwindow.h"
#include "trayicon.h"
#include "resource.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <fcntl.h>
#include <stdio.h>

GKeyFile *keyfile = NULL;

//
// GSourceWinMsgQueue
//
// A GSource to dispatch the Windows message queue
//
typedef struct
{
  GSource source;
  int fd;
  gpointer fdtag;
} GSourceWinMsgQueue;

static gboolean
winMsgQueueCheck(GSource *source)
{
  return (g_source_query_unix_fd(source, ((GSourceWinMsgQueue *)source)->fdtag) != 0);
}

static gboolean
winMsgQueueDispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
  MSG msg;

  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      DispatchMessage(&msg);

      // terminate the main loop on WM_QUIT
      if (msg.message == WM_QUIT)
        gtk_main_quit();
    }

  return G_SOURCE_CONTINUE; // keep this source alive
}

static void
winMsgQueueFinalize(GSource *source)
{
  g_source_remove_unix_fd(source, ((GSourceWinMsgQueue *)source)->fdtag);
  close(((GSourceWinMsgQueue *)source)->fd);
}

GSourceFuncs winMsgQueueSourceFuncs =
{
  .prepare = NULL,
  .check = winMsgQueueCheck,
  .dispatch = winMsgQueueDispatch,
  .finalize = winMsgQueueFinalize,
};

GSource *
winMsgQueueCreate(void)
{
  GSourceWinMsgQueue *msgQueueSource = (GSourceWinMsgQueue *)g_source_new(&winMsgQueueSourceFuncs, sizeof(GSourceWinMsgQueue));

  g_source_set_name((GSource *)msgQueueSource, "Win32 message queue");
  msgQueueSource->fd = open("/dev/windows", O_RDONLY);
  msgQueueSource->fdtag = g_source_add_unix_fd((GSource *)msgQueueSource, msgQueueSource->fd, G_IO_IN);

  return (GSource *)msgQueueSource;
}

//
// main
//
// we need to run the GLib main loop so gmenu can receive notifications that the
// directories it is watching have changed.  we also need to pump the windows
// message queue
//
int
main (int argc, char **argv)
{
  // make sure stdout is line-buffered
  setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

  gtk_init(&argc, &argv);

  // load settings
  int size_id = ID_SIZE_DEFAULT;
  keyfile = g_key_file_new();
  gchar *filename = g_strjoin(NULL, g_get_user_config_dir(), "/xwin-xdg-menu", NULL);
  if (g_key_file_load_from_file(keyfile, filename, G_KEY_FILE_KEEP_COMMENTS, NULL))
    {
      GError *err = NULL;
      int tmp = g_key_file_get_integer(keyfile, "settings", "iconsize", &err);
      if (!err)
        size_id = tmp;
    }

  // construct menu
  menu_init(size_id);

  // construct message window
  HWND hwndMsg = createMsgWindow();
  if (!hwndMsg)
    return -1;

  // main loop
  GSource *msgQueueSource = winMsgQueueCreate();
  g_source_attach(msgQueueSource, g_main_context_default());

  initNotifyIcon(hwndMsg);

  gtk_main();

  deleteNotifyIcon(hwndMsg);

  g_source_destroy(msgQueueSource);

  // save settings
  g_key_file_save_to_file(keyfile, filename, NULL);
  g_key_file_free(keyfile);
  g_free(filename);

  return 0;
}
