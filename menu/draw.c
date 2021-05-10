/**
 * @file
 * Paint the Menu
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page menu_draw Paint the Menu
 *
 * Paint the Menu
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "context.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"

/**
 * get_color - Choose a colour for a line of the index
 * @param index Index number
 * @param s     String of embedded colour codes
 * @retval num Colour pair in an integer
 *
 * Text is coloured by inserting special characters into the string, e.g.
 * #MT_COLOR_INDEX_AUTHOR
 */
static int get_color(int index, unsigned char *s)
{
  struct ColorLineList *color = NULL;
  struct ColorLine *np = NULL;
  struct Email *e = mutt_get_virt_email(Context->mailbox, index);
  int type = *s;

  switch (type)
  {
    case MT_COLOR_INDEX_AUTHOR:
      color = mutt_color_index_author();
      break;
    case MT_COLOR_INDEX_FLAGS:
      color = mutt_color_index_flags();
      break;
    case MT_COLOR_INDEX_SUBJECT:
      color = mutt_color_index_subject();
      break;
    case MT_COLOR_INDEX_TAG:
      STAILQ_FOREACH(np, mutt_color_index_tags(), entries)
      {
        if (mutt_strn_equal((const char *) (s + 1), np->pattern, strlen(np->pattern)))
          return np->pair;
        const char *transform = mutt_hash_find(TagTransforms, np->pattern);
        if (transform && mutt_strn_equal((const char *) (s + 1), transform, strlen(transform)))
        {
          return np->pair;
        }
      }
      return 0;
    default:
      return mutt_color(type);
  }

  STAILQ_FOREACH(np, color, entries)
  {
    if (mutt_pattern_exec(SLIST_FIRST(np->color_pattern),
                          MUTT_MATCH_FULL_ADDRESS, Context->mailbox, e, NULL))
    {
      return np->pair;
    }
  }

  return 0;
}

/**
 * print_enriched_string - Display a string with embedded colours and graphics
 * @param index    Index number
 * @param attr     Default colour for the line
 * @param s        String of embedded colour codes
 * @param do_color If true, apply colour
 */
static void print_enriched_string(int index, int attr, unsigned char *s, bool do_color)
{
  wchar_t wc;
  size_t k;
  size_t n = mutt_str_len((char *) s);
  mbstate_t mbstate;

  memset(&mbstate, 0, sizeof(mbstate));
  while (*s)
  {
    if (*s < MUTT_TREE_MAX)
    {
      if (do_color)
#if defined(HAVE_COLOR) && defined(HAVE_USE_DEFAULT_COLORS)
        /* Combining tree fg color and another bg color requires having
         * use_default_colors, because the other bg color may be undefined. */
        mutt_curses_set_attr(mutt_color_combine(mutt_color(MT_COLOR_TREE), attr));
#else
        mutt_curses_set_color(MT_COLOR_TREE);
#endif

      const bool c_ascii_chars = cs_subset_bool(NeoMutt->sub, "ascii_chars");
      while (*s && (*s < MUTT_TREE_MAX))
      {
        switch (*s)
        {
          case MUTT_TREE_LLCORNER:
            if (c_ascii_chars)
              mutt_window_addch('`');
#ifdef WACS_LLCORNER
            else
              add_wch(WACS_LLCORNER);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\224"); /* WACS_LLCORNER */
            else
              mutt_window_addch(ACS_LLCORNER);
#endif
            break;
          case MUTT_TREE_ULCORNER:
            if (c_ascii_chars)
              mutt_window_addch(',');
#ifdef WACS_ULCORNER
            else
              add_wch(WACS_ULCORNER);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\214"); /* WACS_ULCORNER */
            else
              mutt_window_addch(ACS_ULCORNER);
#endif
            break;
          case MUTT_TREE_LTEE:
            if (c_ascii_chars)
              mutt_window_addch('|');
#ifdef WACS_LTEE
            else
              add_wch(WACS_LTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\234"); /* WACS_LTEE */
            else
              mutt_window_addch(ACS_LTEE);
#endif
            break;
          case MUTT_TREE_HLINE:
            if (c_ascii_chars)
              mutt_window_addch('-');
#ifdef WACS_HLINE
            else
              add_wch(WACS_HLINE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\200"); /* WACS_HLINE */
            else
              mutt_window_addch(ACS_HLINE);
#endif
            break;
          case MUTT_TREE_VLINE:
            if (c_ascii_chars)
              mutt_window_addch('|');
#ifdef WACS_VLINE
            else
              add_wch(WACS_VLINE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\202"); /* WACS_VLINE */
            else
              mutt_window_addch(ACS_VLINE);
#endif
            break;
          case MUTT_TREE_TTEE:
            if (c_ascii_chars)
              mutt_window_addch('-');
#ifdef WACS_TTEE
            else
              add_wch(WACS_TTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\254"); /* WACS_TTEE */
            else
              mutt_window_addch(ACS_TTEE);
#endif
            break;
          case MUTT_TREE_BTEE:
            if (c_ascii_chars)
              mutt_window_addch('-');
#ifdef WACS_BTEE
            else
              add_wch(WACS_BTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\264"); /* WACS_BTEE */
            else
              mutt_window_addch(ACS_BTEE);
#endif
            break;
          case MUTT_TREE_SPACE:
            mutt_window_addch(' ');
            break;
          case MUTT_TREE_RARROW:
            mutt_window_addch('>');
            break;
          case MUTT_TREE_STAR:
            mutt_window_addch('*'); /* fake thread indicator */
            break;
          case MUTT_TREE_HIDDEN:
            mutt_window_addch('&');
            break;
          case MUTT_TREE_EQUALS:
            mutt_window_addch('=');
            break;
          case MUTT_TREE_MISSING:
            mutt_window_addch('?');
            break;
        }
        s++;
        n--;
      }
      if (do_color)
        mutt_curses_set_attr(attr);
    }
    else if (*s == MUTT_SPECIAL_INDEX)
    {
      s++;
      if (do_color)
      {
        if (*s == MT_COLOR_INDEX)
        {
          attrset(attr);
        }
        else
        {
          if (get_color(index, s) == 0)
          {
            attron(attr);
          }
          else
          {
            attron(get_color(index, s));
          }
        }
      }
      s++;
      n -= 2;
    }
    else if ((k = mbrtowc(&wc, (char *) s, n, &mbstate)) > 0)
    {
      mutt_window_addnstr((char *) s, k);
      s += k;
      n -= k;
    }
    else
      break;
  }
}

/**
 * menu_make_entry - Create string to display in a Menu (the index)
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param menu Current Menu
 * @param i    Selected item
 */
void menu_make_entry(struct Menu *menu, char *buf, size_t buflen, int i)
{
  if (!ARRAY_EMPTY(&menu->dialog))
  {
    mutt_str_copy(buf, NONULL(*ARRAY_GET(&menu->dialog, i)), buflen);
    menu_set_index(menu, -1); /* hide menubar */
  }
  else
    menu->make_entry(menu, buf, buflen, i);
}

/**
 * menu_pad_string - Pad a string with spaces for display in the Menu
 * @param menu   Current Menu
 * @param buf    Buffer containing the string
 * @param buflen Length of the buffer
 *
 * @note The string is padded in-place.
 */
static void menu_pad_string(struct Menu *menu, char *buf, size_t buflen)
{
  char *scratch = mutt_str_dup(buf);
  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(NeoMutt->sub, "arrow_string");
  int shift = c_arrow_cursor ? mutt_strwidth(c_arrow_string) + 1 : 0;
  int cols = menu->win_index->state.cols - shift;

  mutt_simple_format(buf, buflen, cols, cols, JUSTIFY_LEFT, ' ', scratch,
                     mutt_str_len(scratch), true);
  buf[buflen - 1] = '\0';
  FREE(&scratch);
}

/**
 * menu_redraw_full - Force the redraw of the Menu
 * @param menu Current Menu
 */
void menu_redraw_full(struct Menu *menu)
{
  mutt_curses_set_color(MT_COLOR_NORMAL);
  mutt_window_clear(menu->win_index);

  window_redraw(RootWindow, true);
  menu->pagelen = menu->win_index->state.rows;

  mutt_show_error();

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * menu_redraw_status - Force the redraw of the status bar
 * @param menu Current Menu
 */
void menu_redraw_status(struct Menu *menu)
{
  if (!menu || !menu->win_ibar)
    return;

  char buf[256];

  snprintf(buf, sizeof(buf), "-- NeoMutt: %s", menu->title);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_move(menu->win_ibar, 0, 0);
  mutt_paddstr(menu->win_ibar->state.cols, buf);
  mutt_curses_set_color(MT_COLOR_NORMAL);
  menu->redraw &= ~REDRAW_STATUS;
}

/**
 * menu_redraw_index - Force the redraw of the index
 * @param menu Current Menu
 */
void menu_redraw_index(struct Menu *menu)
{
  char buf[1024];
  bool do_color;
  int attr;

  for (int i = menu->top; i < (menu->top + menu->pagelen); i++)
  {
    if (i < menu->max)
    {
      attr = menu->color(menu, i);

      menu_make_entry(menu, buf, sizeof(buf), i);
      menu_pad_string(menu, buf, sizeof(buf));

      mutt_curses_set_attr(attr);
      mutt_window_move(menu->win_index, 0, i - menu->top);
      do_color = true;

      const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
      const char *const c_arrow_string =
          cs_subset_string(NeoMutt->sub, "arrow_string");
      if (i == menu->current)
      {
        mutt_curses_set_color(MT_COLOR_INDICATOR);
        if (c_arrow_cursor)
        {
          mutt_window_addstr(c_arrow_string);
          mutt_curses_set_attr(attr);
          mutt_window_addch(' ');
        }
        else
          do_color = false;
      }
      else if (c_arrow_cursor)
      {
        /* Print space chars to match the screen width of `$arrow_string` */
        mutt_window_printf("%*s", mutt_strwidth(c_arrow_string) + 1, "");
      }

      print_enriched_string(i, attr, (unsigned char *) buf, do_color);
    }
    else
    {
      mutt_curses_set_color(MT_COLOR_NORMAL);
      mutt_window_clearline(menu->win_index, i - menu->top);
    }
  }
  mutt_curses_set_color(MT_COLOR_NORMAL);
  menu->redraw = REDRAW_NO_FLAGS;

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw_motion - Force the redraw of the list part of the menu
 * @param menu Current Menu
 */
void menu_redraw_motion(struct Menu *menu)
{
  char buf[1024];

  if (!ARRAY_EMPTY(&menu->dialog))
  {
    menu->redraw &= ~REDRAW_MOTION;
    return;
  }

  /* Note: menu->color() for the index can end up retrieving a message
   * over imap (if matching against ~h for instance).  This can
   * generate status messages.  So we want to call it *before* we
   * position the cursor for drawing. */
  const int old_color = menu->color(menu, menu->oldcurrent);
  mutt_window_move(menu->win_index, 0, menu->oldcurrent - menu->top);
  mutt_curses_set_attr(old_color);

  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(NeoMutt->sub, "arrow_string");
  if (c_arrow_cursor)
  {
    /* clear the arrow */
    /* Print space chars to match the screen width of `$arrow_string` */
    mutt_window_printf("%*s", mutt_strwidth(c_arrow_string) + 1, "");

    menu_make_entry(menu, buf, sizeof(buf), menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_window_move(menu->win_index, mutt_strwidth(c_arrow_string) + 1,
                     menu->oldcurrent - menu->top);
    print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, true);

    /* now draw it in the new location */
    mutt_curses_set_color(MT_COLOR_INDICATOR);
    mutt_window_mvaddstr(menu->win_index, 0, menu->current - menu->top, c_arrow_string);
  }
  else
  {
    /* erase the current indicator */
    menu_make_entry(menu, buf, sizeof(buf), menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, true);

    /* now draw the new one to reflect the change */
    const int cur_color = menu->color(menu, menu->current);
    menu_make_entry(menu, buf, sizeof(buf), menu->current);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_curses_set_color(MT_COLOR_INDICATOR);
    mutt_window_move(menu->win_index, 0, menu->current - menu->top);
    print_enriched_string(menu->current, cur_color, (unsigned char *) buf, false);
  }
  menu->redraw &= REDRAW_STATUS;
  mutt_curses_set_color(MT_COLOR_NORMAL);

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw_current - Redraw the current menu
 * @param menu Current Menu
 */
void menu_redraw_current(struct Menu *menu)
{
  char buf[1024];
  int attr = menu->color(menu, menu->current);

  mutt_window_move(menu->win_index, 0, menu->current - menu->top);
  menu_make_entry(menu, buf, sizeof(buf), menu->current);
  menu_pad_string(menu, buf, sizeof(buf));

  mutt_curses_set_color(MT_COLOR_INDICATOR);
  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(NeoMutt->sub, "arrow_string");
  if (c_arrow_cursor)
  {
    mutt_window_addstr(c_arrow_string);
    mutt_curses_set_attr(attr);
    mutt_window_addch(' ');
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->current, attr, (unsigned char *) buf, true);
  }
  else
    print_enriched_string(menu->current, attr, (unsigned char *) buf, false);
  menu->redraw &= REDRAW_STATUS;
  mutt_curses_set_color(MT_COLOR_NORMAL);

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw_prompt - Force the redraw of the message window
 * @param menu Current Menu
 */
static void menu_redraw_prompt(struct Menu *menu)
{
  if (!menu || ARRAY_EMPTY(&menu->dialog))
    return;

  if (OptMsgErr)
  {
    mutt_sleep(1);
    OptMsgErr = false;
  }

  if (ErrorBufMessage)
    mutt_clear_error();

  mutt_window_mvaddstr(MessageWindow, 0, 0, menu->prompt);
  mutt_window_clrtoeol(MessageWindow);

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw - Redraw the parts of the screen that have been flagged to be redrawn
 * @param menu Menu to redraw
 * @retval OP_NULL   Menu was redrawn
 * @retval OP_REDRAW Full redraw required
 */
int menu_redraw(struct Menu *menu)
{
  if (menu->custom_redraw)
  {
    menu->custom_redraw(menu);
    return OP_NULL;
  }

  /* See if all or part of the screen needs to be updated.  */
  if (menu->redraw & REDRAW_FULL)
  {
    menu_redraw_full(menu);
    /* allow the caller to do any local configuration */
    return OP_REDRAW;
  }

  if (ARRAY_EMPTY(&menu->dialog))
    menu_check_recenter(menu);

  if (menu->redraw & REDRAW_STATUS)
    menu_redraw_status(menu);
  if (menu->redraw & REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & REDRAW_MOTION)
    menu_redraw_motion(menu);
  else if (menu->redraw == REDRAW_CURRENT)
    menu_redraw_current(menu);

  if (!ARRAY_EMPTY(&menu->dialog))
    menu_redraw_prompt(menu);

  return OP_NULL;
}
