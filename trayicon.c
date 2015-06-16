/*
 * trayicon.c
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
// Notification area icon message handling
//
// Loosely based on wintrayicon.c from xserver/hw/xwin/
//

#include "trayicon.h"

#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include "resource.h"
#include "menu.h"
#include "execute.h"

HMENU hMenuTray;

/*
 * Return the HICON to use in the taskbar notification area
 */
HICON
taskbarIcon(void)
{
    HICON hIcon;
    hIcon = (HICON) LoadImage(GetModuleHandle(NULL),
                              MAKEINTRESOURCE(IDI_TRAY),
                              IMAGE_ICON,
                              GetSystemMetrics(SM_CXSMICON),
                              GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    return hIcon;
}

/*
 * Initialize the tray icon
 */
void
initNotifyIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { 0 };

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 0;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = taskbarIcon();

    /* Set tooltip text */
    snprintf(nid.szTip, sizeof(nid.szTip),
             "X applications menu on %s", getenv("DISPLAY"));

    /* Add the tray icon */
    if (!Shell_NotifyIcon(NIM_ADD, &nid))
        printf("initNotifyIcon - Shell_NotifyIcon Failed\n");
}

/*
 * Delete the tray icon
 */
void
deleteNotifyIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { 0 };

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 0;

    /* Delete the tray icon */
    if (!Shell_NotifyIcon(NIM_DELETE, &nid)) {
        printf("deleteNotifyIcon - Shell_NotifyIcon failed\n");
        return;
    }
}

static INT_PTR CALLBACK
aboutDlgProc(HWND hwndDialog, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
  case WM_INITDIALOG:
    {
      HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_XWIN));
      PostMessage(hwndDialog, WM_SETICON, ICON_BIG, (LPARAM) hIcon);

      /* Set the DISPLAY */
      char *display = NULL;
      if (asprintf(&display, "DISPLAY is %s", getenv("DISPLAY")) > 0)
        SetWindowText(GetDlgItem(hwndDialog, IDC_DISPLAY), display);
      free(display);

      return TRUE;
    }

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK)
      {
        EndDialog(hwndDialog, 0);
        return TRUE;
      }

  case WM_CLOSE:
    EndDialog(hwndDialog, 0);
    return TRUE;
  }

  return FALSE;
}

/*
 * Process messages intended for the tray icon
 */
LRESULT
handleIconMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (lParam) {
  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
    {
      POINT ptCursor;

      /* Get cursor position */
      GetCursorPos(&ptCursor);

      /*
       * NOTE: This three-step procedure is required for
       * proper popup menu operation.  Without the
       * call to SetForegroundWindow the
       * popup menu will often not disappear when you click
       * outside of it.  Without the PostMessage the second
       * time you display the popup menu it might immediately
       * disappear.
       */
      SetForegroundWindow(hwnd);
      int cmd = TrackPopupMenuEx(hMenuTray,
                                 TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                 ptCursor.x, ptCursor.y, hwnd, NULL);
      PostMessage(hwnd, WM_NULL, 0, 0);

      if (cmd > ID_EXEC_BASE)
        {
          menu_item_execute(cmd - ID_EXEC_BASE);
        }
      else
        {
          switch(cmd)
            {
            case ID_APP_ABOUT:
              DialogBox(NULL, MAKEINTRESOURCE(IDD_ABOUT), hwnd, aboutDlgProc);
              break;

            case ID_APP_REFRESH:
              menu_refresh();
              break;

            case ID_SIZE_DEFAULT:
            case ID_SIZE_16:
            case ID_SIZE_32:
            case ID_SIZE_48:
            case ID_SIZE_64:
              menu_set_icon_size(cmd);
              break;

            case ID_APP_EXIT:
              PostQuitMessage(0);  // XXX: needs confirmation dialog
              break;
            }
        }
    }
    break;
  }

  return 0;
}
