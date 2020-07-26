/**
 * @file
 * Help Bar
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page helpbar_helpbar Help Bar
 *
 * Help Bar
 */

#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "keymap.h"
#include "mutt_globals.h"

/**
 * make_help - Create one entry for the help bar
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param txt    Text part, e.g. "delete"
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param op     Operation, e.g. OP_DELETE
 *
 * This will return something like: "d:delete"
 */
void make_help(char *buf, size_t buflen, const char *txt, enum MenuType menu, int op)
{
  char tmp[128];

  if (km_expand_key(tmp, sizeof(tmp), km_find_func(menu, op)) ||
      km_expand_key(tmp, sizeof(tmp), km_find_func(MENU_GENERIC, op)))
  {
    snprintf(buf, buflen, "%s:%s", tmp, txt);
  }
  else
  {
    buf[0] = '\0';
  }
}

/**
 * mutt_compile_help - Create the text for the help menu
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param items  Map of functions to display in the help bar
 * @retval ptr Buffer containing result
 */
char *mutt_compile_help(char *buf, size_t buflen, enum MenuType menu,
                        const struct Mapping *items)
{
  char *pbuf = buf;

  for (int i = 0; items[i].name && buflen > 2; i++)
  {
    if (i)
    {
      *pbuf++ = ' ';
      *pbuf++ = ' ';
      buflen -= 2;
    }
    make_help(pbuf, buflen, _(items[i].name), menu, items[i].value);
    const size_t len = mutt_str_len(pbuf);
    pbuf += len;
    buflen -= len;
  }
  return buf;
}

/**
 * helpbar_create - Create the Help Bar Window
 * @retval ptr New Window
 */
struct MuttWindow *helpbar_create(void)
{
  struct MuttWindow *win =
      mutt_window_new(WT_HELP_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);
  win->state.visible = C_Help;

  return win;
}
