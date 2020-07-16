/**
 * @file
 * GUI present the user with a selectable list
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2012 Michael R. Elkins <me@mutt.org>
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
 * @page menu GUI present the user with a selectable list
 *
 * GUI present the user with a selectable list
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
#include "mutt.h"
#include "commands.h"
#include "context.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "pattern/lib.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif

/* These Config Variables are only used in menu.c */
short C_MenuContext; ///< Config: Number of lines of overlap when changing pages in the index
bool C_MenuMoveOff; ///< Config: Allow the last menu item to move off the bottom of the screen
bool C_MenuScroll; ///< Config: Scroll the menu/index by one line, rather than a page

char *SearchBuffers[MENU_MAX];

/* These are used to track the active menus, for redraw operations. */
static size_t MenuStackCount = 0;
static size_t MenuStackLen = 0;
static struct Menu **MenuStack = NULL;

#define DIRECTION ((neg * 2) + 1)

#define MUTT_SEARCH_UP 1
#define MUTT_SEARCH_DOWN 2

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
      color = &Colors->index_author_list;
      break;
    case MT_COLOR_INDEX_FLAGS:
      color = &Colors->index_flags_list;
      break;
    case MT_COLOR_INDEX_SUBJECT:
      color = &Colors->index_subject_list;
      break;
    case MT_COLOR_INDEX_TAG:
      STAILQ_FOREACH(np, &Colors->index_tag_list, entries)
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
      return Colors->defs[type];
  }

  STAILQ_FOREACH(np, color, entries)
  {
    if (mutt_pattern_exec(SLIST_FIRST(np->color_pattern),
                          MUTT_MATCH_FULL_ADDRESS, Context->mailbox, e, NULL))
      return np->pair;
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
        /* Combining tree fg color and another bg color requires
         * having use_default_colors, because the other bg color
         * may be undefined. */
        mutt_curses_set_attr(mutt_color_combine(Colors, Colors->defs[MT_COLOR_TREE], attr));
#else
        mutt_curses_set_color(MT_COLOR_TREE);
#endif

      while (*s && (*s < MUTT_TREE_MAX))
      {
        switch (*s)
        {
          case MUTT_TREE_LLCORNER:
            if (C_AsciiChars)
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
            if (C_AsciiChars)
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
            if (C_AsciiChars)
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
            if (C_AsciiChars)
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
            if (C_AsciiChars)
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
            if (C_AsciiChars)
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
            if (C_AsciiChars)
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
 * make_entry - Create string to display in a Menu (the index)
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param menu Current Menu
 * @param i    Selected item
 */
static void make_entry(char *buf, size_t buflen, struct Menu *menu, int i)
{
  if (menu->dialog)
  {
    mutt_str_copy(buf, NONULL(menu->dialog[i]), buflen);
    menu->current = -1; /* hide menubar */
  }
  else
    menu->make_entry(buf, buflen, menu, i);
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
  int shift = C_ArrowCursor ? mutt_strwidth(C_ArrowString) + 1 : 0;
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
  /* clear() doesn't optimize screen redraws */
  mutt_window_move_abs(0, 0);
  mutt_window_clrtobot();

  if (C_Help)
  {
    mutt_curses_set_color(MT_COLOR_STATUS);
    mutt_window_move(MuttHelpWindow, 0, 0);
    mutt_paddstr(MuttHelpWindow->state.cols, menu->help);
    mutt_curses_set_color(MT_COLOR_NORMAL);
  }
  menu->pagelen = menu->win_index->state.rows;

  mutt_show_error();

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
#ifdef USE_SIDEBAR
  menu->redraw |= REDRAW_SIDEBAR;
#endif
}

/**
 * menu_redraw_status - Force the redraw of the status bar
 * @param menu Current Menu
 */
void menu_redraw_status(struct Menu *menu)
{
  char buf[256];

  snprintf(buf, sizeof(buf), "-- NeoMutt: %s", menu->title);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_move(menu->win_ibar, 0, 0);
  mutt_paddstr(menu->win_ibar->state.cols, buf);
  mutt_curses_set_color(MT_COLOR_NORMAL);
  menu->redraw &= ~REDRAW_STATUS;
}

#ifdef USE_SIDEBAR
/**
 * menu_redraw_sidebar - Force the redraw of the sidebar
 * @param menu Current Menu
 */
void menu_redraw_sidebar(struct Menu *menu)
{
  menu->redraw &= ~REDRAW_SIDEBAR;
  struct MuttWindow *dlg = mutt_window_dialog(menu->win_index);
  struct MuttWindow *sidebar = mutt_window_find(dlg, WT_SIDEBAR);
  sb_draw(sidebar);
}
#endif

/**
 * menu_redraw_index - Force the redraw of the index
 * @param menu Current Menu
 */
void menu_redraw_index(struct Menu *menu)
{
  char buf[1024];
  bool do_color;
  int attr;

  for (int i = menu->top; i < menu->top + menu->pagelen; i++)
  {
    if (i < menu->max)
    {
      attr = menu->color(i);

      make_entry(buf, sizeof(buf), menu, i);
      menu_pad_string(menu, buf, sizeof(buf));

      mutt_curses_set_attr(attr);
      mutt_window_move(menu->win_index, 0, i - menu->top);
      do_color = true;

      if (i == menu->current)
      {
        mutt_curses_set_color(MT_COLOR_INDICATOR);
        if (C_ArrowCursor)
        {
          mutt_window_addstr(C_ArrowString);
          mutt_curses_set_attr(attr);
          mutt_window_addch(' ');
        }
        else
          do_color = false;
      }
      else if (C_ArrowCursor)
        /* Print space chars to match the screen width of C_ArrowString */
        mutt_window_printf("%*s", mutt_strwidth(C_ArrowString) + 1, "");

      print_enriched_string(i, attr, (unsigned char *) buf, do_color);
    }
    else
    {
      mutt_curses_set_color(MT_COLOR_NORMAL);
      mutt_window_clearline(menu->win_index, i - menu->top);
    }
  }
  mutt_curses_set_color(MT_COLOR_NORMAL);
  menu->redraw = 0;
}

/**
 * menu_redraw_motion - Force the redraw of the list part of the menu
 * @param menu Current Menu
 */
void menu_redraw_motion(struct Menu *menu)
{
  char buf[1024];

  if (menu->dialog)
  {
    menu->redraw &= ~REDRAW_MOTION;
    return;
  }

  /* Note: menu->color() for the index can end up retrieving a message
   * over imap (if matching against ~h for instance).  This can
   * generate status messages.  So we want to call it *before* we
   * position the cursor for drawing. */
  const int old_color = menu->color(menu->oldcurrent);
  mutt_window_move(menu->win_index, 0, menu->oldcurrent - menu->top);
  mutt_curses_set_attr(old_color);

  if (C_ArrowCursor)
  {
    /* clear the arrow */
    /* Print space chars to match the screen width of C_ArrowString */
    mutt_window_printf("%*s", mutt_strwidth(C_ArrowString) + 1, "");

    if (menu->redraw & REDRAW_MOTION_RESYNC)
    {
      make_entry(buf, sizeof(buf), menu, menu->oldcurrent);
      menu_pad_string(menu, buf, sizeof(buf));
      mutt_window_move(menu->win_index, mutt_strwidth(C_ArrowString) + 1,
                       menu->oldcurrent - menu->top);
      print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, true);
    }

    /* now draw it in the new location */
    mutt_curses_set_color(MT_COLOR_INDICATOR);
    mutt_window_mvaddstr(menu->win_index, 0, menu->current - menu->top, C_ArrowString);
  }
  else
  {
    /* erase the current indicator */
    make_entry(buf, sizeof(buf), menu, menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, true);

    /* now draw the new one to reflect the change */
    const int cur_color = menu->color(menu->current);
    make_entry(buf, sizeof(buf), menu, menu->current);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_curses_set_color(MT_COLOR_INDICATOR);
    mutt_window_move(menu->win_index, 0, menu->current - menu->top);
    print_enriched_string(menu->current, cur_color, (unsigned char *) buf, false);
  }
  menu->redraw &= REDRAW_STATUS;
  mutt_curses_set_color(MT_COLOR_NORMAL);
}

/**
 * menu_redraw_current - Redraw the current menu
 * @param menu Current Menu
 */
void menu_redraw_current(struct Menu *menu)
{
  char buf[1024];
  int attr = menu->color(menu->current);

  mutt_window_move(menu->win_index, 0, menu->current - menu->top);
  make_entry(buf, sizeof(buf), menu, menu->current);
  menu_pad_string(menu, buf, sizeof(buf));

  mutt_curses_set_color(MT_COLOR_INDICATOR);
  if (C_ArrowCursor)
  {
    mutt_window_addstr(C_ArrowString);
    mutt_curses_set_attr(attr);
    mutt_window_addch(' ');
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->current, attr, (unsigned char *) buf, true);
  }
  else
    print_enriched_string(menu->current, attr, (unsigned char *) buf, false);
  menu->redraw &= REDRAW_STATUS;
  mutt_curses_set_color(MT_COLOR_NORMAL);
}

/**
 * menu_redraw_prompt - Force the redraw of the message window
 * @param menu Current Menu
 */
static void menu_redraw_prompt(struct Menu *menu)
{
  if (!menu || !menu->dialog)
    return;

  if (OptMsgErr)
  {
    mutt_sleep(1);
    OptMsgErr = false;
  }

  if (ErrorBufMessage)
    mutt_clear_error();

  mutt_window_mvaddstr(MuttMessageWindow, 0, 0, menu->prompt);
  mutt_window_clrtoeol(MuttMessageWindow);
}

/**
 * menu_check_recenter - Recentre the menu on screen
 * @param menu Current Menu
 */
void menu_check_recenter(struct Menu *menu)
{
  int c = MIN(C_MenuContext, menu->pagelen / 2);
  int old_top = menu->top;

  if (!C_MenuMoveOff && (menu->max <= menu->pagelen)) /* less entries than lines */
  {
    if (menu->top != 0)
    {
      menu->top = 0;
      menu->redraw |= REDRAW_INDEX;
    }
  }
  else
  {
    if (C_MenuScroll || (menu->pagelen <= 0) || (c < C_MenuContext))
    {
      if (menu->current < menu->top + c)
        menu->top = menu->current - c;
      else if (menu->current >= menu->top + menu->pagelen - c)
        menu->top = menu->current - menu->pagelen + c + 1;
    }
    else
    {
      if (menu->current < menu->top + c)
      {
        menu->top -= (menu->pagelen - c) * ((menu->top + menu->pagelen - 1 - menu->current) /
                                            (menu->pagelen - c)) -
                     c;
      }
      else if ((menu->current >= menu->top + menu->pagelen - c))
      {
        menu->top +=
            (menu->pagelen - c) * ((menu->current - menu->top) / (menu->pagelen - c)) - c;
      }
    }
  }

  if (!C_MenuMoveOff) /* make entries stick to bottom */
    menu->top = MIN(menu->top, menu->max - menu->pagelen);
  menu->top = MAX(menu->top, 0);

  if (menu->top != old_top)
    menu->redraw |= REDRAW_INDEX;
}

/**
 * menu_jump - Jump to another item in the menu
 * @param menu Current Menu
 *
 * Ask the user for a message number to jump to.
 */
static void menu_jump(struct Menu *menu)
{
  int n;

  if (menu->max)
  {
    mutt_unget_event(LastKey, 0);
    char buf[128];
    buf[0] = '\0';
    if ((mutt_get_field(_("Jump to: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) == 0) && buf[0])
    {
      if ((mutt_str_atoi(buf, &n) == 0) && (n > 0) && (n < menu->max + 1))
      {
        n--; /* msg numbers are 0-based */
        menu->current = n;
        menu->redraw = REDRAW_MOTION;
      }
      else
        mutt_error(_("Invalid index number"));
    }
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_next_line - Move the view down one line, keeping the selection the same
 * @param menu Current Menu
 */
void menu_next_line(struct Menu *menu)
{
  if (menu->max)
  {
    int c = MIN(C_MenuContext, menu->pagelen / 2);

    if ((menu->top + 1 < menu->max - c) &&
        (C_MenuMoveOff ||
         ((menu->max > menu->pagelen) && (menu->top < menu->max - menu->pagelen))))
    {
      menu->top++;
      if ((menu->current < menu->top + c) && (menu->current < menu->max - 1))
        menu->current++;
      menu->redraw = REDRAW_INDEX;
    }
    else
      mutt_message(_("You can't scroll down farther"));
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_prev_line - Move the view up one line, keeping the selection the same
 * @param menu Current Menu
 */
void menu_prev_line(struct Menu *menu)
{
  if (menu->top > 0)
  {
    int c = MIN(C_MenuContext, menu->pagelen / 2);

    menu->top--;
    if ((menu->current >= menu->top + menu->pagelen - c) && (menu->current > 1))
      menu->current--;
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_message(_("You can't scroll up farther"));
}

/**
 * menu_length_jump - Calculate the destination of a jump
 * @param menu    Current Menu
 * @param jumplen Number of lines to jump
 *
 * * pageup:   jumplen == -pagelen
 * * pagedown: jumplen == pagelen
 * * halfup:   jumplen == -pagelen/2
 * * halfdown: jumplen == pagelen/2
 */
static void menu_length_jump(struct Menu *menu, int jumplen)
{
  const int neg = (jumplen >= 0) ? 0 : -1;
  const int c = MIN(C_MenuContext, menu->pagelen / 2);

  if (menu->max)
  {
    /* possible to scroll? */
    int tmp;
    if (DIRECTION * menu->top <
        (tmp = (neg ? 0 : (menu->max /* -1 */) - (menu->pagelen /* -1 */))))
    {
      menu->top += jumplen;

      /* jumped too long? */
      if ((neg || !C_MenuMoveOff) && (DIRECTION * menu->top > tmp))
        menu->top = tmp;

      /* need to move the cursor? */
      if ((DIRECTION *
           (tmp = (menu->current - (menu->top + (neg ? (menu->pagelen - 1) - c : c))))) < 0)
      {
        menu->current -= tmp;
      }

      menu->redraw = REDRAW_INDEX;
    }
    else if ((menu->current != (neg ? 0 : menu->max - 1)) && !menu->dialog)
    {
      menu->current += jumplen;
      menu->redraw = REDRAW_MOTION;
    }
    else
    {
      mutt_message(neg ? _("You are on the first page") : _("You are on the last page"));
    }

    menu->current = MIN(menu->current, menu->max - 1);
    menu->current = MAX(menu->current, 0);
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_next_page - Move the focus to the next page in the menu
 * @param menu Current Menu
 */
void menu_next_page(struct Menu *menu)
{
  menu_length_jump(menu, MAX(menu->pagelen /* - MenuOverlap */, 0));
}

/**
 * menu_prev_page - Move the focus to the previous page in the menu
 * @param menu Current Menu
 */
void menu_prev_page(struct Menu *menu)
{
  menu_length_jump(menu, 0 - MAX(menu->pagelen /* - MenuOverlap */, 0));
}

/**
 * menu_half_down - Move the focus down half a page in the menu
 * @param menu Current Menu
 */
void menu_half_down(struct Menu *menu)
{
  menu_length_jump(menu, menu->pagelen / 2);
}

/**
 * menu_half_up - Move the focus up half a page in the menu
 * @param menu Current Menu
 */
void menu_half_up(struct Menu *menu)
{
  menu_length_jump(menu, 0 - menu->pagelen / 2);
}

/**
 * menu_top_page - Move the focus to the top of the page
 * @param menu Current Menu
 */
void menu_top_page(struct Menu *menu)
{
  if (menu->current == menu->top)
    return;

  menu->current = menu->top;
  menu->redraw = REDRAW_MOTION;
}

/**
 * menu_bottom_page - Move the focus to the bottom of the page
 * @param menu Current Menu
 */
void menu_bottom_page(struct Menu *menu)
{
  if (menu->max)
  {
    menu->current = menu->top + menu->pagelen - 1;
    if (menu->current > menu->max - 1)
      menu->current = menu->max - 1;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_middle_page - Move the focus to the centre of the page
 * @param menu Current Menu
 */
void menu_middle_page(struct Menu *menu)
{
  if (menu->max)
  {
    int i = menu->top + menu->pagelen;
    if (i > menu->max - 1)
      i = menu->max - 1;
    menu->current = menu->top + (i - menu->top) / 2;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_first_entry - Move the focus to the first entry in the menu
 * @param menu Current Menu
 */
void menu_first_entry(struct Menu *menu)
{
  if (menu->max)
  {
    menu->current = 0;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_last_entry - Move the focus to the last entry in the menu
 * @param menu Current Menu
 */
void menu_last_entry(struct Menu *menu)
{
  if (menu->max)
  {
    menu->current = menu->max - 1;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_current_top - Move the current selection to the top of the window
 * @param menu Current Menu
 */
void menu_current_top(struct Menu *menu)
{
  if (menu->max)
  {
    menu->top = menu->current;
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_current_middle - Move the current selection to the centre of the window
 * @param menu Current Menu
 */
void menu_current_middle(struct Menu *menu)
{
  if (menu->max)
  {
    menu->top = menu->current - menu->pagelen / 2;
    if (menu->top < 0)
      menu->top = 0;
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_current_bottom - Move the current selection to the bottom of the window
 * @param menu Current Menu
 */
void menu_current_bottom(struct Menu *menu)
{
  if (menu->max)
  {
    menu->top = menu->current - menu->pagelen + 1;
    if (menu->top < 0)
      menu->top = 0;
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_error(_("No entries"));
}

/**
 * menu_next_entry - Move the focus to the next item in the menu
 * @param menu Current Menu
 */
static void menu_next_entry(struct Menu *menu)
{
  if (menu->current < menu->max - 1)
  {
    menu->current++;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_message(_("You are on the last entry"));
}

/**
 * menu_prev_entry - Move the focus to the previous item in the menu
 * @param menu Current Menu
 */
static void menu_prev_entry(struct Menu *menu)
{
  if (menu->current)
  {
    menu->current--;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_message(_("You are on the first entry"));
}

/**
 * default_color - Get the default colour for a line of the menu - Implements Menu::color()
 */
static int default_color(int line)
{
  return Colors->defs[MT_COLOR_NORMAL];
}

/**
 * generic_search - Search a menu for a item matching a regex - Implements Menu::search()
 */
static int generic_search(struct Menu *menu, regex_t *rx, int line)
{
  char buf[1024];

  make_entry(buf, sizeof(buf), menu, line);
  return regexec(rx, buf, 0, NULL, 0);
}

/**
 * mutt_menu_init - Initialise all the Menus
 */
void mutt_menu_init(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    SearchBuffers[i] = NULL;
}

/**
 * mutt_menu_new - Create a new Menu
 * @param type Menu type, e.g. #MENU_PAGER
 * @retval ptr New Menu
 */
struct Menu *mutt_menu_new(enum MenuType type)
{
  struct Menu *menu = mutt_mem_calloc(1, sizeof(struct Menu));

  menu->type = type;
  menu->current = 0;
  menu->top = 0;
  menu->redraw = REDRAW_FULL;
  menu->color = default_color;
  menu->search = generic_search;

  return menu;
}

/**
 * mutt_menu_free - Destroy a menu
 * @param[out] ptr Menu to destroy
 */
void mutt_menu_free(struct Menu **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Menu *m = *ptr;
  if (m->dialog)
  {
    for (int i = 0; i < m->max; i++)
      FREE(&m->dialog[i]);

    FREE(&m->dialog);
  }

  FREE(ptr);
}

/**
 * mutt_menu_add_dialog_row - Add a row to a Menu
 * @param menu Menu to add to
 * @param row  Row of text to add
 */
void mutt_menu_add_dialog_row(struct Menu *menu, const char *row)
{
  if (menu->dsize <= menu->max)
  {
    menu->dsize += 10;
    mutt_mem_realloc(&menu->dialog, menu->dsize * sizeof(char *));
  }
  menu->dialog[menu->max++] = mutt_str_dup(row);
}

/**
 * get_current_menu - Get the current Menu
 * @retval ptr Current Menu
 */
static struct Menu *get_current_menu(void)
{
  return MenuStackCount ? MenuStack[MenuStackCount - 1] : NULL;
}

/**
 * mutt_menu_push_current - Add a new Menu to the stack
 * @param menu Menu to add
 *
 * The menus are stored in a LIFO.  The top-most is shown to the user.
 */
void mutt_menu_push_current(struct Menu *menu)
{
  if (MenuStackCount >= MenuStackLen)
  {
    MenuStackLen += 5;
    mutt_mem_realloc(&MenuStack, MenuStackLen * sizeof(struct Menu *));
  }

  MenuStack[MenuStackCount++] = menu;
  CurrentMenu = menu->type;
}

/**
 * mutt_menu_pop_current - Remove a Menu from the stack
 * @param menu Current Menu
 *
 * The menus are stored in a LIFO.  The top-most is shown to the user.
 */
void mutt_menu_pop_current(struct Menu *menu)
{
  struct Menu *prev_menu = NULL;

  if (!MenuStackCount || (MenuStack[MenuStackCount - 1] != menu))
  {
    mutt_debug(LL_DEBUG1, "called with inactive menu\n");
    return;
  }

  MenuStackCount--;
  prev_menu = get_current_menu();
  if (prev_menu)
  {
    CurrentMenu = prev_menu->type;
    prev_menu->redraw = REDRAW_FULL;
  }
  else
  {
    CurrentMenu = MENU_MAIN;
    /* Clearing when NeoMutt exits would be an annoying change in behavior for
     * those who have disabled alternative screens.  The option is currently
     * set by autocrypt initialization which mixes menus and prompts outside of
     * the normal menu system state.  */
    if (OptMenuPopClearScreen)
    {
      mutt_window_move_abs(0, 0);
      mutt_window_clrtobot();
    }
  }
}

/**
 * mutt_menu_set_current_redraw - Set redraw flags on the current menu
 * @param redraw Flags to set, see #MuttRedrawFlags
 */
void mutt_menu_set_current_redraw(MuttRedrawFlags redraw)
{
  struct Menu *current_menu = get_current_menu();
  if (current_menu)
    current_menu->redraw |= redraw;
}

/**
 * mutt_menu_set_current_redraw_full - Flag the current menu to be fully redrawn
 */
void mutt_menu_set_current_redraw_full(void)
{
  struct Menu *current_menu = get_current_menu();
  if (current_menu)
    current_menu->redraw = REDRAW_FULL;
}

/**
 * mutt_menu_set_redraw - Set redraw flags on a menu
 * @param menu   Menu type, e.g. #MENU_ALIAS
 * @param redraw Flags, e.g. #REDRAW_INDEX
 *
 * This is ignored if it's not the current menu.
 */
void mutt_menu_set_redraw(enum MenuType menu, MuttRedrawFlags redraw)
{
  if (CurrentMenu == menu)
    mutt_menu_set_current_redraw(redraw);
}

/**
 * mutt_menu_set_redraw_full - Flag a menu to be fully redrawn
 * @param menu Menu type, e.g. #MENU_ALIAS
 *
 * This is ignored if it's not the current menu.
 */
void mutt_menu_set_redraw_full(enum MenuType menu)
{
  if (CurrentMenu == menu)
    mutt_menu_set_current_redraw_full();
}

/**
 * mutt_menu_current_redraw - Redraw the current menu
 */
void mutt_menu_current_redraw(void)
{
  struct Menu *current_menu = get_current_menu();
  if (current_menu)
  {
    if (menu_redraw(current_menu) == OP_REDRAW)
    {
      /* On a REDRAW_FULL with a non-customized redraw, menu_redraw()
       * will return OP_REDRAW to give the calling menu-loop a chance to
       * customize output.  */
      menu_redraw(current_menu);
    }
  }
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
                        buf, sizeof(buf), MUTT_CLEAR) != 0) ||
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
    int flags = mutt_mb_is_lower(search_buf) ? REG_ICASE : 0;
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

  if (C_WrapSearch && (wrap++ == 0))
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
  } while (ch.ch == -2);

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

  if (!menu->dialog)
    menu_check_recenter(menu);

  if (menu->redraw & REDRAW_STATUS)
    menu_redraw_status(menu);
#ifdef USE_SIDEBAR
  if (menu->redraw & REDRAW_SIDEBAR)
    menu_redraw_sidebar(menu);
#endif
  if (menu->redraw & REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & (REDRAW_MOTION | REDRAW_MOTION_RESYNC))
    menu_redraw_motion(menu);
  else if (menu->redraw == REDRAW_CURRENT)
    menu_redraw_current(menu);

  if (menu->dialog)
    menu_redraw_prompt(menu);

  return OP_NULL;
}

/**
 * mutt_menu_loop - Menu event loop
 * @param menu Current Menu
 * @retval num An event id that the menu can't process
 */
int mutt_menu_loop(struct Menu *menu)
{
  static int last_position = -1;
  int i = OP_NULL;

  if (menu->max && menu->is_mailbox_list)
  {
    if (last_position > (menu->max - 1))
      last_position = -1;
    else if (last_position >= 0)
      menu->current = last_position;
  }

  while (true)
  {
    /* Clear the tag prefix unless we just started it.  Don't clear
     * the prefix on a timeout (i==-2), but do clear on an abort (i==-1) */
    if (menu->tagprefix && (i != OP_TAG_PREFIX) && (i != OP_TAG_PREFIX_COND) && (i != -2))
      menu->tagprefix = false;

    mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);

    if (menu_redraw(menu) == OP_REDRAW)
      return OP_REDRAW;

    /* give visual indication that the next command is a tag- command */
    if (menu->tagprefix)
    {
      mutt_window_mvaddstr(MuttMessageWindow, 0, 0, "tag-");
      mutt_window_clrtoeol(MuttMessageWindow);
    }

    menu->oldcurrent = menu->current;

    /* move the cursor out of the way */
    if (C_ArrowCursor)
      mutt_window_move(menu->win_index, 2, menu->current - menu->top);
    else if (C_BrailleFriendly)
      mutt_window_move(menu->win_index, 0, menu->current - menu->top);
    else
    {
      mutt_window_move(menu->win_index, menu->win_index->state.cols - 1,
                       menu->current - menu->top);
    }

    mutt_refresh();

    /* try to catch dialog keys before ops */
    if (menu->dialog && (menu_dialog_dokey(menu, &i) == 0))
      return i;

    i = km_dokey(menu->type);
    if ((i == OP_TAG_PREFIX) || (i == OP_TAG_PREFIX_COND))
    {
      if (menu->tagprefix)
      {
        menu->tagprefix = false;
        mutt_window_clearline(MuttMessageWindow, 0);
        continue;
      }

      if (menu->tagged)
      {
        menu->tagprefix = true;
        continue;
      }
      else if (i == OP_TAG_PREFIX)
      {
        mutt_error(_("No tagged entries"));
        i = -1;
      }
      else /* None tagged, OP_TAG_PREFIX_COND */
      {
        mutt_flush_macro_to_endcond();
        mutt_message(_("Nothing to do"));
        i = -1;
      }
    }
    else if (menu->tagged && C_AutoTag)
      menu->tagprefix = true;

    mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

    if (SigWinch)
    {
      SigWinch = 0;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
    }

    if (i < 0)
    {
      if (menu->tagprefix)
        mutt_window_clearline(MuttMessageWindow, 0);
      continue;
    }

    if (!menu->dialog)
      mutt_clear_error();

    /* Convert menubar movement to scrolling */
    if (menu->dialog)
      i = menu_dialog_translate_op(i);

    switch (i)
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
        if (menu->search && !menu->dialog) /* Searching dialogs won't work */
        {
          menu->oldcurrent = menu->current;
          menu->current = search(menu, i);
          if (menu->current != -1)
            menu->redraw = REDRAW_MOTION;
          else
            menu->current = menu->oldcurrent;
        }
        else
          mutt_error(_("Search is not implemented for this menu"));
        break;

      case OP_JUMP:
        if (menu->dialog)
          mutt_error(_("Jumping is not implemented for dialogs"));
        else
          menu_jump(menu);
        break;

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        break;

      case OP_TAG:
        if (menu->tag && !menu->dialog)
        {
          if (menu->tagprefix && !C_AutoTag)
          {
            for (i = 0; i < menu->max; i++)
              menu->tagged += menu->tag(menu, i, 0);
            menu->redraw |= REDRAW_INDEX;
          }
          else if (menu->max)
          {
            int j = menu->tag(menu, menu->current, -1);
            menu->tagged += j;
            if (j && C_Resolve && (menu->current < menu->max - 1))
            {
              menu->current++;
              menu->redraw |= REDRAW_MOTION_RESYNC;
            }
            else
              menu->redraw |= REDRAW_CURRENT;
          }
          else
            mutt_error(_("No entries"));
        }
        else
          mutt_error(_("Tagging is not supported"));
        break;

      case OP_SHELL_ESCAPE:
        mutt_shell_escape();
        break;

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

      case OP_CHECK_STATS:
        mutt_check_stats();
        break;

      case OP_REDRAW:
        clearok(stdscr, true);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_HELP:
        mutt_help(menu->type, menu->win_index->state.cols);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_NULL:
        km_error_key(menu->type);
        break;

      case OP_END_COND:
        break;

      default:
        if (menu->is_mailbox_list)
          last_position = menu->current;
        return i;
    }
  }
  /* not reached */
}

/**
 * mutt_menu_color_observer - Listen for colour changes affecting the menu - Implements ::observer_t
 */
int mutt_menu_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  int s = nc->event_subtype;

  bool simple = (s == MT_COLOR_INDEX_COLLAPSED) || (s == MT_COLOR_INDEX_DATE) ||
                (s == MT_COLOR_INDEX_LABEL) || (s == MT_COLOR_INDEX_NUMBER) ||
                (s == MT_COLOR_INDEX_SIZE) || (s == MT_COLOR_INDEX_TAGS);
  bool lists = (s == MT_COLOR_ATTACH_HEADERS) || (s == MT_COLOR_BODY) ||
               (s == MT_COLOR_HEADER) || (s == MT_COLOR_INDEX) ||
               (s == MT_COLOR_INDEX_AUTHOR) || (s == MT_COLOR_INDEX_FLAGS) ||
               (s == MT_COLOR_INDEX_SUBJECT) || (s == MT_COLOR_INDEX_TAG);

  // The changes aren't relevant to the index menu
  if (!simple && !lists)
    return 0;

  struct EventColor *ec = nc->event_data;

  // Colour deleted from a list
  if (!ec->set && lists && Context && Context->mailbox)
  {
    struct Mailbox *m = Context->mailbox;
    // Force re-caching of index colors
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      e->pair = 0;
    }
  }

  mutt_menu_set_redraw_full(MENU_MAIN);
  return 0;
}

/**
 * mutt_menu_config_observer - Listen for config changes affecting the menu - Implements ::observer_t
 */
int mutt_menu_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  const struct ConfigDef *cdef = ec->he->data;
  ConfigRedrawFlags flags = cdef->type & R_REDRAW_MASK;

  if (flags == R_REDRAW_NO_FLAGS)
    return 0;

  if (flags & R_INDEX)
    mutt_menu_set_redraw_full(MENU_MAIN);
  if (flags & R_PAGER)
    mutt_menu_set_redraw_full(MENU_PAGER);
  if (flags & R_PAGER_FLOW)
  {
    mutt_menu_set_redraw_full(MENU_PAGER);
    mutt_menu_set_redraw(MENU_PAGER, REDRAW_FLOW);
  }

  if (flags & R_RESORT_SUB)
    OptSortSubthreads = true;
  if (flags & R_RESORT)
    OptNeedResort = true;
  if (flags & R_RESORT_INIT)
    OptResortInit = true;
  if (flags & R_TREE)
    OptRedrawTree = true;

  if (flags & R_REFLOW)
    mutt_window_reflow(NULL);
#ifdef USE_SIDEBAR
  if (flags & R_SIDEBAR)
    mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
#endif
  if (flags & R_MENU)
    mutt_menu_set_current_redraw_full();

  return 0;
}
