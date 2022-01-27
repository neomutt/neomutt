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
#include "gui/lib.h"
#include "color/lib.h"
#include "index/lib.h"
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
static struct AttrColor *get_color(int index, unsigned char *s)
{
  struct RegexColorList *rcl = NULL;
  struct RegexColor *np = NULL;
  struct Mailbox *m_cur = get_current_mailbox();
  struct Email *e = mutt_get_virt_email(m_cur, index);
  int type = *s;

  switch (type)
  {
    case MT_COLOR_INDEX_AUTHOR:
      rcl = regex_colors_get_list(MT_COLOR_INDEX_AUTHOR);
      break;
    case MT_COLOR_INDEX_FLAGS:
      rcl = regex_colors_get_list(MT_COLOR_INDEX_FLAGS);
      break;
    case MT_COLOR_INDEX_SUBJECT:
      rcl = regex_colors_get_list(MT_COLOR_INDEX_SUBJECT);
      break;
    case MT_COLOR_INDEX_TAG:
      STAILQ_FOREACH(np, regex_colors_get_list(MT_COLOR_INDEX_TAG), entries)
      {
        if (mutt_strn_equal((const char *) (s + 1), np->pattern, strlen(np->pattern)))
          return &np->attr_color;
        const char *transform = mutt_hash_find(TagTransforms, np->pattern);
        if (transform && mutt_strn_equal((const char *) (s + 1), transform, strlen(transform)))
        {
          return &np->attr_color;
        }
      }
      return NULL;
    default:
      return simple_color_get(type);
  }

  STAILQ_FOREACH(np, rcl, entries)
  {
    if (mutt_pattern_exec(SLIST_FIRST(np->color_pattern),
                          MUTT_MATCH_FULL_ADDRESS, m_cur, e, NULL))
    {
      return &np->attr_color;
    }
  }

  return NULL;
}

/**
 * print_enriched_string - Display a string with embedded colours and graphics
 * @param win      Window
 * @param index    Index number
 * @param ac_def   Default colour for the line
 * @param ac_ind   Indicator colour for the line
 * @param s        String of embedded colour codes
 * @param sub      Config items
 */
static void print_enriched_string(struct MuttWindow *win, int index,
                                  struct AttrColor *ac_def, struct AttrColor *ac_ind,
                                  unsigned char *s, struct ConfigSubset *sub)
{
  wchar_t wc = 0;
  size_t k;
  size_t n = mutt_str_len((char *) s);
  mbstate_t mbstate = { 0 };

  const bool c_ascii_chars = cs_subset_bool(sub, "ascii_chars");
  while (*s)
  {
    if (*s < MUTT_TREE_MAX)
    {
      struct AttrColor *ac_merge =
          merged_color_overlay(ac_def, simple_color_get(MT_COLOR_TREE));
      ac_merge = merged_color_overlay(ac_merge, ac_ind);

      /* Combining tree fg color and another bg color requires having
       * use_default_colors, because the other bg color may be undefined. */
      mutt_curses_set_color(ac_merge);

      while (*s && (*s < MUTT_TREE_MAX))
      {
        switch (*s)
        {
          case MUTT_TREE_LLCORNER:
            if (c_ascii_chars)
              mutt_window_addch(win, '`');
#ifdef WACS_LLCORNER
            else
              add_wch(WACS_LLCORNER);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\224"); /* WACS_LLCORNER */
            else
              mutt_window_addch(win, ACS_LLCORNER);
#endif
            break;
          case MUTT_TREE_ULCORNER:
            if (c_ascii_chars)
              mutt_window_addch(win, ',');
#ifdef WACS_ULCORNER
            else
              add_wch(WACS_ULCORNER);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\214"); /* WACS_ULCORNER */
            else
              mutt_window_addch(win, ACS_ULCORNER);
#endif
            break;
          case MUTT_TREE_LTEE:
            if (c_ascii_chars)
              mutt_window_addch(win, '|');
#ifdef WACS_LTEE
            else
              add_wch(WACS_LTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\234"); /* WACS_LTEE */
            else
              mutt_window_addch(win, ACS_LTEE);
#endif
            break;
          case MUTT_TREE_HLINE:
            if (c_ascii_chars)
              mutt_window_addch(win, '-');
#ifdef WACS_HLINE
            else
              add_wch(WACS_HLINE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\200"); /* WACS_HLINE */
            else
              mutt_window_addch(win, ACS_HLINE);
#endif
            break;
          case MUTT_TREE_VLINE:
            if (c_ascii_chars)
              mutt_window_addch(win, '|');
#ifdef WACS_VLINE
            else
              add_wch(WACS_VLINE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\202"); /* WACS_VLINE */
            else
              mutt_window_addch(win, ACS_VLINE);
#endif
            break;
          case MUTT_TREE_TTEE:
            if (c_ascii_chars)
              mutt_window_addch(win, '-');
#ifdef WACS_TTEE
            else
              add_wch(WACS_TTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\254"); /* WACS_TTEE */
            else
              mutt_window_addch(win, ACS_TTEE);
#endif
            break;
          case MUTT_TREE_BTEE:
            if (c_ascii_chars)
              mutt_window_addch(win, '-');
#ifdef WACS_BTEE
            else
              add_wch(WACS_BTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr(win, "\342\224\264"); /* WACS_BTEE */
            else
              mutt_window_addch(win, ACS_BTEE);
#endif
            break;
          case MUTT_TREE_SPACE:
            mutt_window_addch(win, ' ');
            break;
          case MUTT_TREE_RARROW:
            mutt_window_addch(win, '>');
            break;
          case MUTT_TREE_STAR:
            mutt_window_addch(win, '*'); /* fake thread indicator */
            break;
          case MUTT_TREE_HIDDEN:
            mutt_window_addch(win, '&');
            break;
          case MUTT_TREE_EQUALS:
            mutt_window_addch(win, '=');
            break;
          case MUTT_TREE_MISSING:
            mutt_window_addch(win, '?');
            break;
        }
        s++;
        n--;
      }
      ac_merge = merged_color_overlay(ac_def, ac_ind);
      mutt_curses_set_color(ac_merge);
    }
    else if (*s == MUTT_SPECIAL_INDEX)
    {
      s++;
      if (*s == MT_COLOR_INDEX)
      {
        struct AttrColor *ac_merge = merged_color_overlay(ac_def, ac_ind);
        mutt_curses_set_color(ac_merge);
      }
      else
      {
        struct AttrColor *color = get_color(index, s);
        struct AttrColor *ac_merge = merged_color_overlay(ac_def, color);
        ac_merge = merged_color_overlay(ac_merge, ac_ind);

        mutt_curses_set_color(ac_merge);
      }
      s++;
      n -= 2;
    }
    else if ((k = mbrtowc(&wc, (char *) s, n, &mbstate)) > 0)
    {
      mutt_window_addnstr(win, (char *) s, k);
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
    menu->current = -1; /* hide menubar */
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
  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(menu->sub, "arrow_string");
  int shift = c_arrow_cursor ? mutt_strwidth(c_arrow_string) + 1 : 0;
  int cols = menu->win->state.cols - shift;

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
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  mutt_window_clear(menu->win);

  menu->pagelen = menu->win->state.rows;

  menu->redraw = MENU_REDRAW_INDEX;
}

/**
 * menu_redraw_index - Force the redraw of the index
 * @param menu Current Menu
 */
void menu_redraw_index(struct Menu *menu)
{
  char buf[1024];
  struct AttrColor *ac = NULL;

  for (int i = menu->top; i < (menu->top + menu->pagelen); i++)
  {
    if (i < menu->max)
    {
      struct AttrColor *ac_ind = NULL;
      if (i == menu->current)
        ac_ind = simple_color_get(MT_COLOR_INDICATOR);

      ac = menu->color(menu, i);

      menu_make_entry(menu, buf, sizeof(buf), i);
      menu_pad_string(menu, buf, sizeof(buf));

      mutt_curses_set_color(ac);
      mutt_window_move(menu->win, 0, i - menu->top);

      const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
      const char *const c_arrow_string =
          cs_subset_string(menu->sub, "arrow_string");
      if (i == menu->current)
      {
        mutt_curses_set_color(ac_ind);
        if (c_arrow_cursor)
        {
          mutt_window_addstr(menu->win, c_arrow_string);
          mutt_curses_set_color(ac);
          mutt_window_addch(menu->win, ' ');
        }
      }
      else if (c_arrow_cursor)
      {
        /* Print space chars to match the screen width of `$arrow_string` */
        mutt_window_printf(menu->win, "%*s", mutt_strwidth(c_arrow_string) + 1, "");
      }

      print_enriched_string(menu->win, i, ac, ac_ind, (unsigned char *) buf, menu->sub);
    }
    else
    {
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
      mutt_window_clearline(menu->win, i - menu->top);
    }
  }
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  menu->redraw = MENU_REDRAW_NO_FLAGS;
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
    menu->redraw &= ~MENU_REDRAW_MOTION;
    return;
  }

  /* Note: menu->color() for the index can end up retrieving a message
   * over imap (if matching against ~h for instance).  This can
   * generate status messages.  So we want to call it *before* we
   * position the cursor for drawing. */
  struct AttrColor *old_color = menu->color(menu, menu->oldcurrent);
  mutt_window_move(menu->win, 0, menu->oldcurrent - menu->top);
  mutt_curses_set_color(old_color);

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(menu->sub, "arrow_string");
  struct AttrColor *ac_ind = simple_color_get(MT_COLOR_INDICATOR);
  if (c_arrow_cursor)
  {
    /* clear the arrow */
    /* Print space chars to match the screen width of `$arrow_string` */
    mutt_window_printf(menu->win, "%*s", mutt_strwidth(c_arrow_string) + 1, "");

    menu_make_entry(menu, buf, sizeof(buf), menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_window_move(menu->win, mutt_strwidth(c_arrow_string) + 1,
                     menu->oldcurrent - menu->top);
    print_enriched_string(menu->win, menu->oldcurrent, old_color, NULL,
                          (unsigned char *) buf, menu->sub);

    /* now draw it in the new location */
    mutt_curses_set_color(ac_ind);
    mutt_window_mvaddstr(menu->win, 0, menu->current - menu->top, c_arrow_string);
  }
  else
  {
    /* erase the current indicator */
    menu_make_entry(menu, buf, sizeof(buf), menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->win, menu->oldcurrent, old_color, NULL,
                          (unsigned char *) buf, menu->sub);

    /* now draw the new one to reflect the change */
    struct AttrColor *cur_color = menu->color(menu, menu->current);
    cur_color = merged_color_overlay(cur_color, ac_ind);
    menu_make_entry(menu, buf, sizeof(buf), menu->current);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_window_move(menu->win, 0, menu->current - menu->top);
    mutt_curses_set_color(cur_color);
    print_enriched_string(menu->win, menu->current, cur_color, ac_ind,
                          (unsigned char *) buf, menu->sub);
  }
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
}

/**
 * menu_redraw_current - Redraw the current menu
 * @param menu Current Menu
 */
void menu_redraw_current(struct Menu *menu)
{
  char buf[1024];
  struct AttrColor *ac = menu->color(menu, menu->current);

  mutt_window_move(menu->win, 0, menu->current - menu->top);
  menu_make_entry(menu, buf, sizeof(buf), menu->current);
  menu_pad_string(menu, buf, sizeof(buf));

  struct AttrColor *ac_ind = simple_color_get(MT_COLOR_INDICATOR);
  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(menu->sub, "arrow_string");
  if (c_arrow_cursor)
  {
    mutt_curses_set_color(ac_ind);
    mutt_window_addstr(menu->win, c_arrow_string);
    mutt_curses_set_color(ac);
    mutt_window_addch(menu->win, ' ');
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->win, menu->current, ac, NULL,
                          (unsigned char *) buf, menu->sub);
  }
  else
  {
    print_enriched_string(menu->win, menu->current, ac, ac_ind,
                          (unsigned char *) buf, menu->sub);
  }
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
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

  msgwin_set_text(MT_COLOR_NORMAL, menu->prompt);
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
  if (menu->redraw & MENU_REDRAW_FULL)
  {
    menu_redraw_full(menu);
    /* allow the caller to do any local configuration */
    return OP_REDRAW;
  }

  if (menu->redraw & MENU_REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & MENU_REDRAW_MOTION)
    menu_redraw_motion(menu);
  else if (menu->redraw == MENU_REDRAW_CURRENT)
    menu_redraw_current(menu);

  if (!ARRAY_EMPTY(&menu->dialog))
    menu_redraw_prompt(menu);

  return OP_NULL;
}
