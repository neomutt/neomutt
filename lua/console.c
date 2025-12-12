/**
 * @file
 * Lua Console
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page lua_console Lua Console
 *
 * Lua Console
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "console.h"
#include "menu/lib.h"
#include "logging.h"
#include "module.h"

static struct MuttWindow *LuaConsole = NULL;

/**
 * lua_console_info_new - XXX
 */
struct LuaConsoleInfo *lua_console_info_new(void)
{
  return MUTT_MEM_CALLOC(1, struct LuaConsoleInfo);
}

/**
 * lua_console_info_free - XXX
 */
void lua_console_info_free(struct Menu *menu, void **pptr)
{
  if (!pptr || !*pptr)
    return;

  // struct LuaConsoleInfo *lci = *pptr;

  FREE(pptr);
}

/**
 * lua_console_make_entry - Format an Log Entry for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $luaconsole_format XXX?
 */
int lua_console_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  //XXX see cert_make_entry() for truncating long lines

  if (!menu || !menu->mdata)
    return 0;

  if (!NeoMutt || !NeoMutt->lua_module)
    return 0;

  struct LuaModule *lm = NeoMutt->lua_module;
  struct LuaLogFile *llf = lm->log_file;
  if (!llf)
    return 0;

  if (line >= ARRAY_SIZE(&llf->line_offsets))
    return 0;

  long offset1 = *ARRAY_GET(&llf->line_offsets, line);
  long *pl = ARRAY_GET(&llf->line_offsets, line + 1);
  long offset2 = pl ? *pl : offset1 + 999;

  buf_alloc(buf, offset2 - offset1);
  fseek(llf->fp, offset1, SEEK_SET);
  buf->dsize = fread(buf->data, 1, offset2 - offset1, llf->fp);

  buf->dsize = MIN(buf->dsize, max_cols);

  if (buf->dsize > 0)
  {
    if (buf->data[buf->dsize - 1] == '\n')
      buf->dsize--;
  }

  buf->data[buf->dsize] = '\0';
  buf->dptr = buf->data + buf->dsize;

  return buf->dsize;
}

/**
 * lua_console_init - XXX
 */
struct MuttWindow *lua_console_init(void)
{
  if (LuaConsole)
    return LuaConsole;

  struct MuttWindow *focus = window_get_focus();
  struct MuttWindow *dlg = dialog_find(focus);
  struct MuttWindow *cont = window_find_child(dlg, WT_CONTAINER);
  if (!cont)
    return NULL;

  struct MuttWindow *win = menu_window_new(MENU_LUA, NeoMutt->sub);

  struct Menu *menu = win->wdata;
  menu->mdata_free = lua_console_info_free;
  menu->make_entry = lua_console_make_entry;
  menu->max = 0;
  menu->show_indicator = false;

  struct LuaConsoleInfo *lci = lua_console_info_new();
  lci->menu = menu;
  NeoMutt->lua_module->console = lci;
  menu->mdata = lci;
  // XXX mdata_free nothing to do

  window_set_visible(win, false);
  mutt_window_add_child(cont, win);
  mutt_window_reflow(dlg);

  LuaConsole = win;
  return LuaConsole;
}

/**
 * lua_console_set_visibility - XXX
 */
void lua_console_set_visibility(enum LuaConsoleVisibilty vis)
{
  struct MuttWindow *win = lua_console_init();
  if (!win)
    return;

  bool visible;
  switch (vis)
  {
    case LCV_SHOW:
      visible = true;
      break;
    case LCV_HIDE:
      visible = false;
      break;
    case LCV_TOGGLE:
      visible = !win->state.visible;
      break;
  }

  window_set_visible(win, visible);

  struct MuttWindow *dlg = dialog_find(win);
  window_reflow(dlg);
  lua_console_update();
}

/**
 * lua_console_update - Update the Lua Console Display
 */
void lua_console_update(void)
{
  if (!NeoMutt || !NeoMutt->lua_module)
    return;

  struct LuaModule *lm = NeoMutt->lua_module;
  struct LuaConsoleInfo *lci = lm->console;
  if (!lci || !lci->menu)
    return;

  struct LuaLogFile *llf = lm->log_file;

  if (llf)
  {
    lci->menu->page_len = lci->menu->win->state.rows; //XXX

    int size = ARRAY_SIZE(&llf->line_offsets);
    lci->menu->max = MAX(0, size - 1);

    if (size > 1)
    {
      menu_last_entry(lci->menu);
    }

    menu_redraw_full(lci->menu);
  }
  else
  {
    lci->menu->max = 0;
  }
}
