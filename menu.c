/*
 * menu.c
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
// Use libgnome-menu to read an XDG desktop menu (.menu file) and all the XDG
// desktop entries (.desktop files) it contains, and then construct a
// corresponding Windows menu
//
// See util/test-menu-spec.c in gnome-menus for an example of using the gmenu API
//
// Loosely based on winprefs.c from xserver/hw/xwin/
//

#include "menu.h"

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>
#include <gtk/gtk.h>
#include <windows.h>
#include <resource.h>

extern HMENU hMenuTray;

typedef struct _xdgmenu
{
  // the GMenuTree
  GMenuTree* tree;

  // the windows menu structure
  HMENU hMenu;
  // size of the bitmaps
  int size;
  int size_id;

  // mapping between menu item IDs and the menu item data
  int count;
  GDesktopAppInfo **appinfo;

  // bitmaps for menu items
  HBITMAP *bitmaps;
} xdgmenu;

// singleton instance
static xdgmenu menu;

// '&' in menu text indicates a keyboard accelerator, so escape them with another '&'
static const char *
escape_ampersand(const char *text)
{
  char *result = strdup(text);
  int i = 0;
  while (result[i] != '\0')
    {
      if (result[i] == '&')
        {
          result = realloc(result, strlen(result) + 2);
          memmove(result + i + 1, result + i, strlen(result) + 1 - i);
          result[i + 1] = '&';
          i++;
        }

      i++;
    }
  return result;
}

static HBITMAP
resource_to_bitmap(int id, int size)
{
  HBITMAP hBitmap = NULL;
  BITMAPV4HEADER bmiV4Header;
  bmiV4Header.bV4Size = sizeof(BITMAPV4HEADER);
  bmiV4Header.bV4Width = size;
  bmiV4Header.bV4Height = size;
  bmiV4Header.bV4Planes = 1;
  bmiV4Header.bV4BitCount = 32;
  bmiV4Header.bV4V4Compression = BI_RGB;
  bmiV4Header.bV4SizeImage = 0;
  bmiV4Header.bV4XPelsPerMeter = 0;
  bmiV4Header.bV4YPelsPerMeter = 0;
  bmiV4Header.bV4ClrUsed = 0;
  bmiV4Header.bV4ClrImportant = 0;
  bmiV4Header.bV4AlphaMask = 0xff000000;
  bmiV4Header.bV4CSType = 0;

  HDC hScreenDC = GetDC(NULL);
  HDC hDC = CreateCompatibleDC(hScreenDC);

  void *pBits;
  hBitmap = CreateDIBSection(hDC, (BITMAPINFO *)&bmiV4Header,
                             DIB_RGB_COLORS, &pBits, NULL, 0);
  if (hBitmap)
    {
      HBITMAP stock = SelectObject(hDC, hBitmap);
      HICON hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(id),
                              IMAGE_ICON, size, size, LR_SHARED);
      DrawIconEx(hDC, 0, 0, hIcon, size, size, 0, NULL, DI_NORMAL);

      SelectObject(hDC, stock);
    }

  DeleteDC(hDC);
  ReleaseDC(NULL, hScreenDC);

  return hBitmap;
}

static HBITMAP
gicon_to_bitmap(GIcon *icon, int size)
{
  GtkIconInfo *iconInfo = NULL;
  HBITMAP hBitmap = NULL;

  // This is expensive, so definitely don't want to this more than once...
  static GtkIconTheme *theme = NULL;
  if (!theme) {
    theme = gtk_icon_theme_new();
    gtk_icon_theme_set_custom_theme(theme, "Adwaita");
  }

  if (icon)
    iconInfo = gtk_icon_theme_lookup_by_gicon(theme, icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
  // It seems that InsertMenuItem only uses the alpha channel of the bitmap if
  // it is a BI_RGB DIB, so we can't use BI_BITFIELDS
  if (iconInfo)
    {
      GdkPixbuf *pixbuf = gtk_icon_info_load_icon(iconInfo, NULL);
      if (pixbuf)
        {
          BITMAPV4HEADER bmiV4Header;
          bmiV4Header.bV4Size = sizeof(BITMAPV4HEADER);
          bmiV4Header.bV4Width = gdk_pixbuf_get_width(pixbuf);
          bmiV4Header.bV4Height = -gdk_pixbuf_get_height(pixbuf); // top-down bitmap
          bmiV4Header.bV4Planes = 1;
          bmiV4Header.bV4BitCount = gdk_pixbuf_get_bits_per_sample(pixbuf) * gdk_pixbuf_get_n_channels(pixbuf);
          bmiV4Header.bV4V4Compression = BI_RGB;
          bmiV4Header.bV4SizeImage = 0;
          bmiV4Header.bV4XPelsPerMeter = 0;
          bmiV4Header.bV4YPelsPerMeter = 0;
          bmiV4Header.bV4ClrUsed = 0;
          bmiV4Header.bV4ClrImportant = 0;
          bmiV4Header.bV4AlphaMask = gdk_pixbuf_get_has_alpha(pixbuf) ? 0xff000000 : 0;
          bmiV4Header.bV4CSType = 0;

          HDC hDC = GetDC(NULL);

          void *pBits;
          hBitmap = CreateDIBSection(hDC, (BITMAPINFO *)&bmiV4Header,
                                    DIB_RGB_COLORS, &pBits, NULL, 0);
          if (hBitmap)
            {
              // copy in the pixbuf data
              memcpy(pBits, gdk_pixbuf_get_pixels(pixbuf),
                     gdk_pixbuf_get_byte_length(pixbuf));

              if (gdk_pixbuf_get_has_alpha(pixbuf))
                {
                  // convert from RGBA to BGRA
                  // convert to premultiplied alpha
                  COLORREF *pargb = pBits;
                  ULONG y, x;
                  for (y = -bmiV4Header.bV4Height; y; --y)
                    {
                      for (x = bmiV4Header.bV4Width; x; --x)
                        {
                          COLORREF p = *pargb;
                          BYTE b = p & 0xFF;
                          BYTE g = (p >> 8) & 0xff;
                          BYTE r = (p >> 16) & 0xff;
                          BYTE a = p >> 24;
                          *pargb = (a << 24) | RGB(r * a / 255,
                                                   g * a / 255,
                                                   b * a / 255);

                          pargb++;
                        }
                    }
                }
              else
                {
                  BYTE *pRGB = pBits;
                  ULONG y, x;
                  for (y = -bmiV4Header.bV4Height; y; --y)
                    {
                      for (x = bmiV4Header.bV4Width; x; --x)
                        {
                          // convert from RGB to BGR
                          BYTE b = pRGB[2];
                          pRGB[2] = pRGB[0];
                          pRGB[0] = b;

                          pRGB = pRGB + 3;
                        }
                    }
                }
            }

          ReleaseDC(NULL, hDC);
          g_object_unref(pixbuf);
        }
    }

  // if no useable icon was found, use the X icon
  if (!hBitmap)
    {
      hBitmap = resource_to_bitmap(IDI_XWIN, size);
    }

  return hBitmap;
}

static void
store_id_info(xdgmenu *menu, GDesktopAppInfo *pAppInfo, HBITMAP hBitmap)
{
  // Store GDesktopAppInfo and HBITMAP to be later accessed via ID
  menu->count++;
  menu->appinfo = realloc(menu->appinfo, sizeof(GDesktopAppInfo *) * menu->count);
  menu->appinfo[menu->count-1] = pAppInfo;
  menu->bitmaps = realloc(menu->bitmaps, sizeof(HBITMAP) * menu->count);
  menu->bitmaps[menu->count-1] = hBitmap;
}

static void
menu_item_entry(xdgmenu *menu, HMENU hMenu, GMenuTreeEntry *entry)
{
  GDesktopAppInfo *pAppInfo = gmenu_tree_entry_get_app_info(entry);

  // The documentation seems to say that icon should be the same size as the
  // default check-mark bitmap, but it seems we can get away with using other
  // sizes...
  int size = menu->size;

  GIcon *pIcon = g_app_info_get_icon(G_APP_INFO(pAppInfo));
  HBITMAP hBitmap = gicon_to_bitmap(pIcon, size);
  store_id_info(menu, pAppInfo, hBitmap);

  //
  const gchar *cName = g_app_info_get_name(G_APP_INFO(pAppInfo));
  const char *text = escape_ampersand(cName);

  // Insert menu item
  MENUITEMINFO mii;
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fMask = MIIM_DATA | MIIM_STRING | MIIM_ID | MIIM_BITMAP;
  mii.fType = MFT_STRING;
  mii.dwTypeData = (LPTSTR)text;
  mii.fState = MFS_ENABLED;
  mii.wID = menu->count + ID_EXEC_BASE;
  mii.dwItemData = (uintptr_t)pAppInfo;
  mii.hbmpItem = hBitmap;

  InsertMenuItem(hMenu, -1, TRUE, &mii);

  free((char *)text);
}

static HMENU
menu_from_directory(xdgmenu *menu, GMenuTreeDirectory *directory)
{
  HMENU hMenu;
  GMenuTreeIter *iter;

  hMenu = CreatePopupMenu();
  if (!hMenu)
    {
      g_print("Unable to CreatePopupMenu()\n");
      return NULL;
    }

  iter = gmenu_tree_directory_iter(directory);

  while (1)
    {
      gpointer item = NULL;

      switch (gmenu_tree_iter_next (iter))
        {
        case GMENU_TREE_ITEM_INVALID:
          goto done;

        case GMENU_TREE_ITEM_ENTRY:
          item = gmenu_tree_iter_get_entry(iter);
          menu_item_entry(menu, hMenu, (GMenuTreeEntry *)item);
          break;

        case GMENU_TREE_ITEM_DIRECTORY:
          item = gmenu_tree_iter_get_directory(iter);
          HMENU hSubMenu = menu_from_directory(menu, (GMenuTreeDirectory *)item);
          GIcon *icon = gmenu_tree_directory_get_icon((GMenuTreeDirectory *)item);
          HBITMAP hBitmap = gicon_to_bitmap(icon, menu->size);
          store_id_info(menu, NULL, hBitmap);
          const char *text = gmenu_tree_directory_get_name((GMenuTreeDirectory *)item);
          text = escape_ampersand(text);

          if (hSubMenu) {
            // Insert menu item
            MENUITEMINFO mii;
            mii.cbSize = sizeof(MENUITEMINFO);
            mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID | MIIM_BITMAP;
            mii.fType = MFT_STRING;
            mii.dwTypeData = (LPTSTR)text;
            mii.fState = MFS_ENABLED;
            mii.wID = menu->count + ID_EXEC_BASE;
            mii.hSubMenu = hSubMenu;
            mii.hbmpItem = hBitmap;

            InsertMenuItem(hMenu, -1, TRUE, &mii);
          }
          free((char *)text);
          break;

        case GMENU_TREE_ITEM_HEADER:
          break;

        case GMENU_TREE_ITEM_SEPARATOR:
          InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
          break;

        case GMENU_TREE_ITEM_ALIAS:
          // ???
          item = gmenu_tree_iter_get_alias(iter);
          break;

        default:
          g_assert_not_reached();
          break;
        }
      gmenu_tree_item_unref(item);
      continue;
    done:
      break;
    }

  gmenu_tree_iter_unref(iter);
  return hMenu;
}

static void
menu_from_tree(void)
{
    GError *error = NULL;

    // Build the XDG desktop menu
    if (!gmenu_tree_load_sync (menu.tree, &error))
      {
        g_printerr ("Failed to load tree: %s\n", error->message);
        menu.hMenu = CreatePopupMenu();
      }
    else
      {
        GMenuTreeDirectory* root;
        root = gmenu_tree_get_root_directory(menu.tree);

        if (root == NULL)
          {
            g_warning ("The menu tree is empty.");
          }
        else
          {
            menu.hMenu = menu_from_directory(&menu, root);
          }

        gmenu_tree_item_unref (root);
      }

    // Add menu items specific to this application
    InsertMenu(menu.hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    HMENU hSettingsMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDM_SETTINGS_MENU));
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_BITMAP;
    mii.fType = MFT_STRING;
    mii.dwTypeData = (LPTSTR)"XDG Menu";
    mii.fState = MFS_ENABLED;
    mii.wID = -1;
    mii.hSubMenu = hSettingsMenu;
    mii.hbmpItem = resource_to_bitmap(IDI_TRAY, menu.size);
    InsertMenuItem(menu.hMenu, -1, TRUE, &mii);
    store_id_info(&menu, NULL, mii.hbmpItem);
    CheckMenuItem(hSettingsMenu, menu.size_id, MF_BYCOMMAND | MF_CHECKED);

    hMenuTray = menu.hMenu;
}

static void
menu_free(void)
{
  int i;
  for (i = 0; i < menu.count; i++)
    {
      DeleteObject(menu.bitmaps[i]);
    }
  menu.count = 0;

  free(menu.appinfo);
  menu.appinfo = NULL;

  free(menu.bitmaps);
  menu.bitmaps = NULL;

  DestroyMenu(menu.hMenu);
  menu.hMenu = NULL;
  hMenuTray = NULL;
}

static int
menu_size_id_to_size(int size_id)
{
  int size;

  switch (size_id)
    {
    case ID_SIZE_16:
      size = 16;
      break;
    case ID_SIZE_24:
      size = 24;
      break;
    case ID_SIZE_32:
      size = 32;
      break;
    case ID_SIZE_48:
      size = 48;
      break;
    case ID_SIZE_64:
      size = 64;
      break;
    default:
    case ID_SIZE_DEFAULT:
      size = MIN(GetSystemMetrics(SM_CXMENUCHECK), GetSystemMetrics(SM_CYMENUCHECK));
      break;
    }

  return size;
}

void
menu_set_icon_size(int size_id)
{
  int size = menu_size_id_to_size(size_id);

  if (menu.size != size)
    {
      g_key_file_set_integer(keyfile, "settings", "iconsize", size_id);

      menu_free();
      menu.size = size;
      menu.size_id = size_id;
      menu_from_tree();
    }
}

static void
menu_changed(GMenuTree *tree)
{
  g_print("Re-reading menu tree\n");
  menu_free();
  menu_from_tree();
}

void
menu_init(int size_id)
{
  menu.hMenu = NULL;
  menu.count = 0;
  menu.appinfo = NULL;
  menu.bitmaps = NULL;

  menu.size_id = size_id;
  menu.size = menu_size_id_to_size(size_id);

  // create the GMenuTree object
  menu.tree = gmenu_tree_new ("xwin-applications.menu", GMENU_TREE_FLAGS_NONE);
  g_assert (menu.tree != NULL);
  g_signal_connect(menu.tree, "changed", G_CALLBACK(menu_changed), NULL);

  menu_from_tree();
}

GDesktopAppInfo *
menu_get_appinfo(int id)
{
  return menu.appinfo[id-1];
}
