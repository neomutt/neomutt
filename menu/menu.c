/**
 * @file
 * GUI present the user with a selectable list
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page menu_menu GUI present the user with a selectable list
 *
 * GUI present the user with a selectable list
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "menu/lib.h"
#include "commands.h"
#include "context.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "opcodes.h"
#include "protos.h"
#include "type.h"

char *SearchBuffers[MENU_MAX];

#define MUTT_SEARCH_UP 1
#define MUTT_SEARCH_DOWN 2

/**
 * menu_jump - Jump to another item in the menu
 * @param menu Current Menu
 *
 * Ask the user for a message number to jump to.
 */
static void menu_jump(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  mutt_unget_event(LastKey, 0);
  char buf[128] = { 0 };
  if ((mutt_get_field(_("Jump to: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS,
                      false, NULL, NULL) == 0) &&
      (buf[0] != '\0'))
  {
    int n = 0;
    if ((mutt_str_atoi(buf, &n) == 0) && (n > 0) && (n < (menu->max + 1)))
    {
      menu_set_index(menu, n - 1); // msg numbers are 0-based
    }
    else
      mutt_error(_("Invalid index number"));
  }
}

/**
 * default_color - Get the default colour for a line of the menu - Implements Menu::color()
 */
static int default_color(struct Menu *menu, int line)
{
  return mutt_color(MT_COLOR_NORMAL);
}

/**
 * generic_search - Search a menu for a item matching a regex - Implements Menu::search()
 */
static int generic_search(struct Menu *menu, regex_t *rx, int line)
{
  char buf[1024];

  menu_make_entry(menu, buf, sizeof(buf), line);
  return regexec(rx, buf, 0, NULL, 0);
}

/**
 * menu_cleanup - Free the saved Menu searches
 */
void menu_cleanup(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    FREE(&SearchBuffers[i]);
}

/**
 * menu_init - Initialise all the Menus
 */
void menu_init(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    SearchBuffers[i] = NULL;
}

/**
 * menu_add_dialog_row - Add a row to a Menu
 * @param menu Menu to add to
 * @param row  Row of text to add
 */
void menu_add_dialog_row(struct Menu *menu, const char *row)
{
  ARRAY_SET(&menu->dialog, menu->max, mutt_str_dup(row));
  menu->max++;
}

/**
 * search - Search a menu
 * @param menu Menu to search
 * @param op   Search operation, e.g. OP_SEARCH_NEXT
 * @retval >=0 Index of matching item
 * @retval -1  Search failed, or was cancelled
 */
static int search(struct Menu *menu, int op)
{
  int rc = 0, wrap = 0;
  int search_dir;
  regex_t re;
  char buf[128];
  char *search_buf = ((menu->type < MENU_MAX)) ? SearchBuffers[menu->type] : NULL;

  if (!(search_buf && *search_buf) || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    mutt_str_copy(buf, search_buf && (search_buf[0] != '\0') ? search_buf : "",
                  sizeof(buf));
    if ((mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                            _("Search for: ") :
                            _("Reverse search for: "),
                        buf, sizeof(buf), MUTT_CLEAR, false, NULL, NULL) != 0) ||
        (buf[0] == '\0'))
    {
      return -1;
    }
    if (menu->type < MENU_MAX)
    {
      mutt_str_replace(&SearchBuffers[menu->type], buf);
      search_buf = SearchBuffers[menu->type];
    }
    menu->search_dir =
        ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? MUTT_SEARCH_DOWN : MUTT_SEARCH_UP;
  }

  search_dir = (menu->search_dir == MUTT_SEARCH_UP) ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    search_dir = -search_dir;

  if (search_buf)
  {
    uint16_t flags = mutt_mb_is_lower(search_buf) ? REG_ICASE : 0;
    rc = REG_COMP(&re, search_buf, REG_NOSUB | flags);
  }

  if (rc != 0)
  {
    regerror(rc, &re, buf, sizeof(buf));
    mutt_error("%s", buf);
    return -1;
  }

  rc = menu->current + search_dir;
search_next:
  if (wrap)
    mutt_message(_("Search wrapped to top"));
  while ((rc >= 0) && (rc < menu->max))
  {
    if (menu->search(menu, &re, rc) == 0)
    {
      regfree(&re);
      return rc;
    }

    rc += search_dir;
  }

  const bool c_wrap_search = cs_subset_bool(menu->sub, "wrap_search");
  if (c_wrap_search && (wrap++ == 0))
  {
    rc = (search_dir == 1) ? 0 : menu->max - 1;
    goto search_next;
  }
  regfree(&re);
  mutt_error(_("Not found"));
  return -1;
}

/**
 * menu_dialog_translate_op - Convert menubar movement to scrolling
 * @param i Action requested, e.g. OP_NEXT_ENTRY
 * @retval num Action to perform, e.g. OP_NEXT_LINE
 */
static int menu_dialog_translate_op(int i)
{
  switch (i)
  {
    case OP_NEXT_ENTRY:
      return OP_NEXT_LINE;
    case OP_PREV_ENTRY:
      return OP_PREV_LINE;
    case OP_CURRENT_TOP:
    case OP_FIRST_ENTRY:
      return OP_TOP_PAGE;
    case OP_CURRENT_BOTTOM:
    case OP_LAST_ENTRY:
      return OP_BOTTOM_PAGE;
    case OP_CURRENT_MIDDLE:
      return OP_MIDDLE_PAGE;
  }

  return i;
}

/**
 * menu_dialog_dokey - Check if there are any menu key events to process
 * @param menu Current Menu
 * @param ip   KeyEvent ID
 * @retval  0 An event occurred for the menu, or a timeout
 * @retval -1 There was an event, but not for menu
 */
static int menu_dialog_dokey(struct Menu *menu, int *ip)
{
  struct KeyEvent ch;
  char *p = NULL;

  do
  {
    ch = mutt_getch();
  } while (ch.ch == -2); // Timeout

  if (ch.ch < 0)
  {
    *ip = -1;
    return 0;
  }

  if (ch.ch && (p = strchr(menu->keys, ch.ch)))
  {
    *ip = OP_MAX + (p - menu->keys + 1);
    return 0;
  }
  else
  {
    if (ch.op == OP_NULL)
      mutt_unget_event(ch.ch, 0);
    else
      mutt_unget_event(0, ch.op);
    return -1;
  }
}

/**
 * menu_loop - Menu event loop
 * @param menu Current Menu
 * @retval num An event id that the menu can't process
 */
int menu_loop(struct Menu *menu)
{
  int op = OP_NULL;

  while (true)
  {
    /* Clear the tag prefix unless we just started it.  Don't clear
     * the prefix on a timeout (op==-2), but do clear on an abort (op==-1) */
    if (menu->tagprefix && (op != OP_TAG_PREFIX) &&
        (op != OP_TAG_PREFIX_COND) && (op != -2))
    {
      menu->tagprefix = false;
    }

    mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);

    if (menu_redraw(menu) == OP_REDRAW)
      return OP_REDRAW;

    /* give visual indication that the next command is a tag- command */
    if (menu->tagprefix)
      msgwin_set_text(MT_COLOR_NORMAL, "tag-");

    const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
    const bool c_braille_friendly =
        cs_subset_bool(menu->sub, "braille_friendly");

    /* move the cursor out of the way */
    if (c_arrow_cursor)
      mutt_window_move(menu->win_index, 2, menu->current - menu->top);
    else if (c_braille_friendly)
      mutt_window_move(menu->win_index, 0, menu->current - menu->top);
    else
    {
      mutt_window_move(menu->win_index, menu->win_index->state.cols - 1,
                       menu->current - menu->top);
    }

    mutt_refresh();

    /* try to catch dialog keys before ops */
    if (!ARRAY_EMPTY(&menu->dialog) && (menu_dialog_dokey(menu, &op) == 0))
      return op;

    const bool c_auto_tag = cs_subset_bool(menu->sub, "auto_tag");
    op = km_dokey(menu->type);
    if ((op == OP_TAG_PREFIX) || (op == OP_TAG_PREFIX_COND))
    {
      if (menu->tagprefix)
      {
        menu->tagprefix = false;
        msgwin_clear_text();
        continue;
      }

      if (menu->tagged)
      {
        menu->tagprefix = true;
        continue;
      }
      else if (op == OP_TAG_PREFIX)
      {
        mutt_error(_("No tagged entries"));
        op = -1;
      }
      else /* None tagged, OP_TAG_PREFIX_COND */
      {
        mutt_flush_macro_to_endcond();
        mutt_message(_("Nothing to do"));
        op = -1;
      }
    }
    else if (menu->tagged && c_auto_tag)
      menu->tagprefix = true;

    mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

    if (SigWinch)
    {
      SigWinch = false;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
    }

    if (op < 0)
    {
      if (menu->tagprefix)
        msgwin_clear_text();
      continue;
    }

    if (ARRAY_EMPTY(&menu->dialog))
      mutt_clear_error();

    /* Convert menubar movement to scrolling */
    if (!ARRAY_EMPTY(&menu->dialog))
      op = menu_dialog_translate_op(op);

    switch (op)
    {
      case OP_NEXT_ENTRY:
        menu_next_entry(menu);
        break;
      case OP_PREV_ENTRY:
        menu_prev_entry(menu);
        break;
      case OP_HALF_DOWN:
        menu_half_down(menu);
        break;
      case OP_HALF_UP:
        menu_half_up(menu);
        break;
      case OP_NEXT_PAGE:
        menu_next_page(menu);
        break;
      case OP_PREV_PAGE:
        menu_prev_page(menu);
        break;
      case OP_NEXT_LINE:
        menu_next_line(menu);
        break;
      case OP_PREV_LINE:
        menu_prev_line(menu);
        break;
      case OP_FIRST_ENTRY:
        menu_first_entry(menu);
        break;
      case OP_LAST_ENTRY:
        menu_last_entry(menu);
        break;
      case OP_TOP_PAGE:
        menu_top_page(menu);
        break;
      case OP_MIDDLE_PAGE:
        menu_middle_page(menu);
        break;
      case OP_BOTTOM_PAGE:
        menu_bottom_page(menu);
        break;
      case OP_CURRENT_TOP:
        menu_current_top(menu);
        break;
      case OP_CURRENT_MIDDLE:
        menu_current_middle(menu);
        break;
      case OP_CURRENT_BOTTOM:
        menu_current_bottom(menu);
        break;
      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
        if (menu->custom_search)
          return op;
        else if (menu->search && ARRAY_EMPTY(&menu->dialog)) /* Searching dialogs won't work */
        {
          int index = search(menu, op);
          if (index != -1)
            menu_set_index(menu, index);
        }
        else
          mutt_error(_("Search is not implemented for this menu"));
        break;

      case OP_JUMP:
        if (!ARRAY_EMPTY(&menu->dialog))
          mutt_error(_("Jumping is not implemented for dialogs"));
        else
          menu_jump(menu);
        break;

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        window_redraw(NULL);
        break;

      case OP_TAG:
        if (menu->tag && ARRAY_EMPTY(&menu->dialog))
        {
          const bool c_resolve = cs_subset_bool(menu->sub, "resolve");

          if (menu->tagprefix && !c_auto_tag)
          {
            for (int i = 0; i < menu->max; i++)
              menu->tagged += menu->tag(menu, i, 0);
            menu->redraw |= MENU_REDRAW_INDEX;
          }
          else if (menu->max != 0)
          {
            int j = menu->tag(menu, menu->current, -1);
            menu->tagged += j;
            if (j && c_resolve && (menu->current < (menu->max - 1)))
            {
              menu_set_index(menu, menu->current + 1);
            }
            else
              menu->redraw |= MENU_REDRAW_CURRENT;
          }
          else
            mutt_error(_("No entries"));
        }
        else
          mutt_error(_("Tagging is not supported"));
        break;

      case OP_SHELL_ESCAPE:
        if (mutt_shell_escape())
        {
          mutt_mailbox_check(ctx_mailbox(Context), MUTT_MAILBOX_CHECK_FORCE);
        }
        break;

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

      case OP_CHECK_STATS:
        mutt_check_stats(ctx_mailbox(Context));
        break;

      case OP_REDRAW:
        clearok(stdscr, true);
        menu->redraw = MENU_REDRAW_FULL;
        break;

      case OP_HELP:
        mutt_help(menu->type);
        menu->redraw = MENU_REDRAW_FULL;
        break;

      case OP_NULL:
        km_error_key(menu->type);
        break;

      case OP_END_COND:
        break;

      default:
        return op;
    }
  }
  /* not reached */
}

/**
 * menu_get_current_type - Get the type of the current Window
 * @retval enum Menu Type, e.g. #MENU_PAGER
 */
enum MenuType menu_get_current_type(void)
{
  struct MuttWindow *win = alldialogs_get_current();
  while (win && win->focus)
    win = win->focus;

  if (!win)
    return MENU_GENERIC;

  if (win->type != WT_MENU)
    return MENU_GENERIC;

  struct Menu *menu = win->wdata;
  if (!menu)
    return MENU_GENERIC;

  return menu->type;
}

/**
 * menu_free - Free a Menu
 * @param ptr Menu to free
 */
void menu_free(struct Menu **ptr)
{
  struct Menu *menu = *ptr;

  notify_free(&menu->notify);

  if (menu->mdata && menu->mdata_free)
    menu->mdata_free(menu, &menu->mdata); // Custom function to free private data

  char **line = NULL;
  ARRAY_FOREACH(line, &menu->dialog)
  {
    FREE(line);
  }
  ARRAY_FREE(&menu->dialog);

  FREE(ptr);
}

/**
 * menu_new - Create a new Menu
 * @param type Menu type, e.g. #MENU_ALIAS
 * @param win  Parent Window
 * @param sub  Config items
 * @retval ptr New Menu
 */
struct Menu *menu_new(enum MenuType type, struct MuttWindow *win, struct ConfigSubset *sub)
{
  struct Menu *menu = mutt_mem_calloc(1, sizeof(struct Menu));

  menu->type = type;
  menu->redraw = MENU_REDRAW_FULL;
  menu->color = default_color;
  menu->search = generic_search;
  menu->notify = notify_new();
  menu->win_index = win;
  menu->pagelen = win->state.rows;
  menu->sub = sub;

  notify_set_parent(menu->notify, win->notify);
  menu_add_observers(menu);

  return menu;
}

/**
 * menu_get_index - Get the current selection in the Menu
 * @param menu Menu
 * @retval num Index of selection
 */
int menu_get_index(struct Menu *menu)
{
  if (!menu)
    return -1;

  return menu->current;
}

/**
 * menu_set_index - Set the current selection in the Menu
 * @param menu  Menu
 * @param index Item to select
 * @retval true Selection was changed
 */
bool menu_set_index(struct Menu *menu, int index)
{
  if (!menu)
    return false;

  if (index < -1)
    return false;
  if (index >= menu->max)
    return false;

  menu->oldcurrent = menu->current;
  menu->current = index;
  menu->redraw |= MENU_REDRAW_MOTION;
  return true;
}

/**
 * menu_queue_redraw - Queue a request for a redraw
 * @param menu  Menu
 * @param redraw Item to redraw, e.g. #MENU_REDRAW_CURRENT
 */
void menu_queue_redraw(struct Menu *menu, MuttRedrawFlags redraw)
{
  if (!menu)
    return;

  menu->redraw |= redraw;
  menu->win_index->actions |= WA_RECALC;
}
