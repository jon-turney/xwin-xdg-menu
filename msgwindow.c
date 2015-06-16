/*
 * msgwindow.cc
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

#include <stdio.h>
#include <windows.h>
#include <trayicon.h>
#include <glib.h>

#define WINDOW_CLASS "xwin-xdg-menu"
#define WINDOW_NAME "xwin-xdg-menu"

/*
 * This is the messaging window, a hidden top-level window. We never do anything
 * with it but listen to the messages sent to it.
 */

/*
 * msgWindowProc - Window procedure for msg window
 */

static
LRESULT CALLBACK
msgWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_TRAYICON:
      return handleIconMessage(hwnd, message, wParam, lParam);
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

static HWND
createMsgWindow(void)
{
    HWND hwndMsg;

    // register window class
    {
        WNDCLASSEX wcx;

        wcx.cbSize = sizeof(WNDCLASSEX);
        wcx.style = CS_HREDRAW | CS_VREDRAW;
        wcx.lpfnWndProc = msgWindowProc;
        wcx.cbClsExtra = 0;
        wcx.cbWndExtra = 0;
        wcx.hInstance = GetModuleHandle(NULL);
        wcx.hIcon = NULL;
        wcx.hCursor = 0;
        wcx.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wcx.lpszMenuName = NULL;
        wcx.lpszClassName = WINDOW_CLASS;
        wcx.hIconSm = NULL;
        RegisterClassEx(&wcx);
    }

    // Create the msg window.
    hwndMsg = CreateWindowEx(0, // no extended styles
                             WINDOW_CLASS,      // class name
                             WINDOW_NAME,       // window name
                             WS_OVERLAPPEDWINDOW,       // overlapped window
                             CW_USEDEFAULT,     // default horizontal position
                             CW_USEDEFAULT,     // default vertical position
                             CW_USEDEFAULT,     // default width
                             CW_USEDEFAULT,     // default height
                             (HWND) NULL,       // no parent or owner window
                             (HMENU) NULL,      // class menu used
                             GetModuleHandle(NULL),     // instance handle
                             NULL);     // no window creation data

    if (!hwndMsg) {
        printf("Create msg window failed\n");
        return NULL;
    }

    return hwndMsg;
}

void *
msgWindowThreadProc(void)
{
    GMainLoop *main_loop;
    HWND hwndMsg = createMsgWindow();
    BOOL bQuit = FALSE;

    main_loop = g_main_loop_new (NULL, FALSE);

    if (hwndMsg) {
        MSG msg;

        initNotifyIcon(hwndMsg);

        while (!bQuit)
          {
            MsgWaitForMultipleObjects(0, NULL, FALSE, 15*1000, QS_ALLINPUT);

            /* Pump the msg window message queue */
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
              DispatchMessage(&msg);

              if (msg.message == WM_QUIT)
                bQuit = TRUE;
            }

            /* Run Glib event processing */
            g_main_context_iteration(g_main_context_default(), FALSE);
        }
    }

    deleteNotifyIcon(hwndMsg);

    g_main_loop_unref (main_loop);

    return NULL;
}
