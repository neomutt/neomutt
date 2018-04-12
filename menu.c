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

#include "config.h"
#include <stddef.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "context.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "tags.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif

struct Header;

char *SearchBuffers[MENU_MAX];

/* These are used to track the active menus, for redraw operations. */
static size_t MenuStackCount = 0;
static size_t MenuStackLen = 0;
static struct Menu **MenuStack = NULL;

static int get_color(int index, unsigned char *s)
{
  struct ColorLineHead *color = NULL;
  struct ColorLine *np = NULL;
  struct Header *hdr = Context->hdrs[Context->v2r[index]];
  int type = *s;

  switch (type)
  {
    case MT_COLOR_INDEX_AUTHOR:
      color = &ColorIndexAuthorList;
      break;
    case MT_COLOR_INDEX_FLAGS:
      color = &ColorIndexFlagsList;
      break;
    case MT_COLOR_INDEX_SUBJECT:
      color = &ColorIndexSubjectList;
      break;
    case MT_COLOR_INDEX_TAG:
      STAILQ_FOREACH(np, &ColorIndexTagList, entries)
      {
        if (strncmp((const char *) (s + 1), np->pattern, strlen(np->pattern)) == 0)
          return np->pair;
        const char *transform = mutt_hash_find(TagTransforms, np->pattern);
        if (transform &&
            (strncmp((const char *) (s + 1), transform, strlen(transform)) == 0))
        {
          return np->pair;
        }
      }
      return 0;
    default:
      return ColorDefs[type];
  }

  STAILQ_FOREACH(np, color, entries)
  {
    if (mutt_pattern_exec(np->color_pattern, MUTT_MATCH_FULL_ADDRESS, Context, hdr, NULL))
      return np->pair;
  }

  return 0;
}

static void print_enriched_string(int index, int attr, unsigned char *s, int do_color)
{
  wchar_t wc;
  size_t k;
  size_t n = mutt_str_strlen((char *) s);
  mbstate_t mbstate;

  if (!stdscr)
    return;

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
        ATTRSET(mutt_combine_color(ColorDefs[MT_COLOR_TREE], attr));
#else
        SETCOLOR(MT_COLOR_TREE);
#endif

      while (*s && *s < MUTT_TREE_MAX)
      {
        switch (*s)
        {
          case MUTT_TREE_LLCORNER:
            if (AsciiChars)
              addch('`');
#ifdef WACS_LLCORNER
            else
              add_wch(WACS_LLCORNER);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\224"); /* WACS_LLCORNER */
            else
              addch(ACS_LLCORNER);
#endif
            break;
          case MUTT_TREE_ULCORNER:
            if (AsciiChars)
              addch(',');
#ifdef WACS_ULCORNER
            else
              add_wch(WACS_ULCORNER);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\214"); /* WACS_ULCORNER */
            else
              addch(ACS_ULCORNER);
#endif
            break;
          case MUTT_TREE_LTEE:
            if (AsciiChars)
              addch('|');
#ifdef WACS_LTEE
            else
              add_wch(WACS_LTEE);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\234"); /* WACS_LTEE */
            else
              addch(ACS_LTEE);
#endif
            break;
          case MUTT_TREE_HLINE:
            if (AsciiChars)
              addch('-');
#ifdef WACS_HLINE
            else
              add_wch(WACS_HLINE);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\200"); /* WACS_HLINE */
            else
              addch(ACS_HLINE);
#endif
            break;
          case MUTT_TREE_VLINE:
            if (AsciiChars)
              addch('|');
#ifdef WACS_VLINE
            else
              add_wch(WACS_VLINE);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\202"); /* WACS_VLINE */
            else
              addch(ACS_VLINE);
#endif
            break;
          case MUTT_TREE_TTEE:
            if (AsciiChars)
              addch('-');
#ifdef WACS_TTEE
            else
              add_wch(WACS_TTEE);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\254"); /* WACS_TTEE */
            else
              addch(ACS_TTEE);
#endif
            break;
          case MUTT_TREE_BTEE:
            if (AsciiChars)
              addch('-');
#ifdef WACS_BTEE
            else
              add_wch(WACS_BTEE);
#else
            else if (CharsetIsUtf8)
              addstr("\342\224\264"); /* WACS_BTEE */
            else
              addch(ACS_BTEE);
#endif
            break;
          case MUTT_TREE_SPACE:
            addch(' ');
            break;
          case MUTT_TREE_RARROW:
            addch('>');
            break;
          case MUTT_TREE_STAR:
            addch('*'); /* fake thread indicator */
            break;
          case MUTT_TREE_HIDDEN:
            addch('&');
            break;
          case MUTT_TREE_EQUALS:
            addch('=');
            break;
          case MUTT_TREE_MISSING:
            addch('?');
            break;
        }
        s++;
        n--;
      }
      if (do_color)
        ATTRSET(attr);
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
      addnstr((char *) s, k);
      s += k;
      n -= k;
    }
    else
      break;
  }
}

static void menu_make_entry(char *s, int l, struct Menu *menu, int i)
{
  if (menu->dialog)
  {
    strncpy(s, menu->dialog[i], l);
    menu->current = -1; /* hide menubar */
  }
  else
    menu->make_entry(s, l, menu, i);
}

static void menu_pad_string(struct Menu *menu, char *buf, size_t buflen)
{
  char *scratch = mutt_str_strdup(buf);
  int shift = ArrowCursor ? 3 : 0;
  int cols = menu->indexwin->cols - shift;

  mutt_simple_format(buf, buflen, cols, cols, FMT_LEFT, ' ', scratch,
                     mutt_str_strlen(scratch), 1);
  buf[buflen - 1] = '\0';
  FREE(&scratch);
}

void menu_redraw_full(struct Menu *menu)
{
  mutt_window_reflow();
  NORMAL_COLOR;
  /* clear() doesn't optimize screen redraws */
  move(0, 0);
  clrtobot();

  if (Help)
  {
    SETCOLOR(MT_COLOR_STATUS);
    mutt_window_move(menu->helpwin, 0, 0);
    mutt_paddstr(menu->helpwin->cols, menu->help);
    NORMAL_COLOR;
  }
  menu->offset = 0;
  menu->pagelen = menu->indexwin->rows;

  mutt_show_error();

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
#ifdef USE_SIDEBAR
  menu->redraw |= REDRAW_SIDEBAR;
#endif
}

void menu_redraw_status(struct Menu *menu)
{
  char buf[STRING];

  snprintf(buf, sizeof(buf), MUTT_MODEFMT, menu->title);
  SETCOLOR(MT_COLOR_STATUS);
  mutt_window_move(menu->statuswin, 0, 0);
  mutt_paddstr(menu->statuswin->cols, buf);
  NORMAL_COLOR;
  menu->redraw &= ~REDRAW_STATUS;
}

#ifdef USE_SIDEBAR
void menu_redraw_sidebar(struct Menu *menu)
{
  menu->redraw &= ~REDRAW_SIDEBAR;
  mutt_sb_draw();
}
#endif

void menu_redraw_index(struct Menu *menu)
{
  char buf[LONG_STRING];
  bool do_color;
  int attr;

  for (int i = menu->top; i < menu->top + menu->pagelen; i++)
  {
    if (i < menu->max)
    {
      attr = menu->color(i);

      menu_make_entry(buf, sizeof(buf), menu, i);
      menu_pad_string(menu, buf, sizeof(buf));

      ATTRSET(attr);
      mutt_window_move(menu->indexwin, i - menu->top + menu->offset, 0);
      do_color = true;

      if (i == menu->current)
      {
        SETCOLOR(MT_COLOR_INDICATOR);
        if (ArrowCursor)
        {
          addstr("->");
          ATTRSET(attr);
          addch(' ');
        }
        else
          do_color = false;
      }
      else if (ArrowCursor)
        addstr("   ");

      print_enriched_string(i, attr, (unsigned char *) buf, do_color);
    }
    else
    {
      NORMAL_COLOR;
      mutt_window_clearline(menu->indexwin, i - menu->top + menu->offset);
    }
  }
  NORMAL_COLOR;
  menu->redraw = 0;
}

void menu_redraw_motion(struct Menu *menu)
{
  char buf[LONG_STRING];

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
  mutt_window_move(menu->indexwin, menu->oldcurrent + menu->offset - menu->top, 0);
  ATTRSET(old_color);

  if (ArrowCursor)
  {
    /* clear the pointer */
    addstr("  ");

    if (menu->redraw & REDRAW_MOTION_RESYNCH)
    {
      menu_make_entry(buf, sizeof(buf), menu, menu->oldcurrent);
      menu_pad_string(menu, buf, sizeof(buf));
      mutt_window_move(menu->indexwin, menu->oldcurrent + menu->offset - menu->top, 3);
      print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, 1);
    }

    /* now draw it in the new location */
    SETCOLOR(MT_COLOR_INDICATOR);
    mutt_window_mvaddstr(menu->indexwin, menu->current + menu->offset - menu->top, 0, "->");
  }
  else
  {
    /* erase the current indicator */
    menu_make_entry(buf, sizeof(buf), menu, menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, 1);

    /* now draw the new one to reflect the change */
    const int cur_color = menu->color(menu->current);
    menu_make_entry(buf, sizeof(buf), menu, menu->current);
    menu_pad_string(menu, buf, sizeof(buf));
    SETCOLOR(MT_COLOR_INDICATOR);
    mutt_window_move(menu->indexwin, menu->current + menu->offset - menu->top, 0);
    print_enriched_string(menu->current, cur_color, (unsigned char *) buf, 0);
  }
  menu->redraw &= REDRAW_STATUS;
  NORMAL_COLOR;
}

void menu_redraw_current(struct Menu *menu)
{
  char buf[LONG_STRING];
  int attr = menu->color(menu->current);

  mutt_window_move(menu->indexwin, menu->current + menu->offset - menu->top, 0);
  menu_make_entry(buf, sizeof(buf), menu, menu->current);
  menu_pad_string(menu, buf, sizeof(buf));

  SETCOLOR(MT_COLOR_INDICATOR);
  if (ArrowCursor)
  {
    addstr("->");
    ATTRSET(attr);
    addch(' ');
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->current, attr, (unsigned char *) buf, 1);
  }
  else
    print_enriched_string(menu->current, attr, (unsigned char *) buf, 0);
  menu->redraw &= REDRAW_STATUS;
  NORMAL_COLOR;
}

static void menu_redraw_prompt(struct Menu *menu)
{
  if (menu->dialog)
  {
    if (OptMsgErr)
    {
      mutt_sleep(1);
      OptMsgErr = false;
    }

    if (ErrorBufMessage)
      mutt_clear_error();

    mutt_window_mvaddstr(menu->messagewin, 0, 0, menu->prompt);
    mutt_window_clrtoeol(menu->messagewin);
  }
}

void menu_check_recenter(struct Menu *menu)
{
  int c = MIN(MenuContext, menu->pagelen / 2);
  int old_top = menu->top;

  if (!MenuMoveOff && menu->max <= menu->pagelen) /* less entries than lines */
  {
    if (menu->top != 0)
    {
      menu->top = 0;
      menu->redraw |= REDRAW_INDEX;
    }
  }
  else
  {
    if (MenuScroll || (menu->pagelen <= 0) || (c < MenuContext))
    {
      if (menu->current < menu->top + c)
        menu->top = menu->current - c;
      else if (menu->current >= menu->top + menu->pagelen - c)
        menu->top = menu->current - menu->pagelen + c + 1;
    }
    else
    {
      if (menu->current < menu->top + c)
        menu->top -= (menu->pagelen - c) * ((menu->top + menu->pagelen - 1 - menu->current) /
                                            (menu->pagelen - c)) -
                     c;
      else if ((menu->current >= menu->top + menu->pagelen - c))
        menu->top +=
            (menu->pagelen - c) * ((menu->current - menu->top) / (menu->pagelen - c)) - c;
    }
  }

  if (!MenuMoveOff) /* make entries stick to bottom */
    menu->top = MIN(menu->top, menu->max - menu->pagelen);
  menu->top = MAX(menu->top, 0);

  if (menu->top != old_top)
    menu->redraw |= REDRAW_INDEX;
}

static void menu_jump(struct Menu *menu)
{
  int n;

  if (menu->max)
  {
    mutt_unget_event(LastKey, 0);
    char buf[SHORT_STRING];
    buf[0] = '\0';
    if (mutt_get_field(_("Jump to: "), buf, sizeof(buf), 0) == 0 && buf[0])
    {
      if (mutt_str_atoi(buf, &n) == 0 && n > 0 && n < menu->max + 1)
      {
        n--; /* msg numbers are 0-based */
        menu->current = n;
        menu->redraw = REDRAW_MOTION;
      }
      else
        mutt_error(_("Invalid index number."));
    }
  }
  else
    mutt_error(_("No entries."));
}

void menu_next_line(struct Menu *menu)
{
  if (menu->max)
  {
    int c = MIN(MenuContext, menu->pagelen / 2);

    if (menu->top + 1 < menu->max - c &&
        (MenuMoveOff || (menu->max > menu->pagelen && menu->top < menu->max - menu->pagelen)))
    {
      menu->top++;
      if (menu->current < menu->top + c && menu->current < menu->max - 1)
        menu->current++;
      menu->redraw = REDRAW_INDEX;
    }
    else
      mutt_error(_("You cannot scroll down farther."));
  }
  else
    mutt_error(_("No entries."));
}

void menu_prev_line(struct Menu *menu)
{
  if (menu->top > 0)
  {
    int c = MIN(MenuContext, menu->pagelen / 2);

    menu->top--;
    if (menu->current >= menu->top + menu->pagelen - c && menu->current > 1)
      menu->current--;
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_error(_("You cannot scroll up farther."));
}

/**
 * menu_length_jump - Calculate the destination of a jump
 *
 * * pageup:   jumplen == -pagelen
 * * pagedown: jumplen == pagelen
 * * halfup:   jumplen == -pagelen/2
 * * halfdown: jumplen == pagelen/2
 */
#define DIRECTION ((neg * 2) + 1)
static void menu_length_jump(struct Menu *menu, int jumplen)
{
  const int neg = (jumplen >= 0) ? 0 : -1;
  const int c = MIN(MenuContext, menu->pagelen / 2);

  if (menu->max)
  {
    /* possible to scroll? */
    int tmp;
    if (DIRECTION * menu->top <
        (tmp = (neg ? 0 : (menu->max /* -1 */) - (menu->pagelen /* -1 */))))
    {
      menu->top += jumplen;

      /* jumped too long? */
      if ((neg || !MenuMoveOff) && DIRECTION * menu->top > tmp)
        menu->top = tmp;

      /* need to move the cursor? */
      if ((DIRECTION *
           (tmp = (menu->current - (menu->top + (neg ? (menu->pagelen - 1) - c : c))))) < 0)
      {
        menu->current -= tmp;
      }

      menu->redraw = REDRAW_INDEX;
    }
    else if (menu->current != (neg ? 0 : menu->max - 1) && !menu->dialog)
    {
      menu->current += jumplen;
      menu->redraw = REDRAW_MOTION;
    }
    else
    {
      mutt_error(neg ? _("You are on the first page.") : _("You are on the last page."));
    }

    menu->current = MIN(menu->current, menu->max - 1);
    menu->current = MAX(menu->current, 0);
  }
  else
    mutt_error(_("No entries."));
}
#undef DIRECTION

void menu_next_page(struct Menu *menu)
{
  menu_length_jump(menu, MAX(menu->pagelen /* - MenuOverlap */, 0));
}

void menu_prev_page(struct Menu *menu)
{
  menu_length_jump(menu, 0 - MAX(menu->pagelen /* - MenuOverlap */, 0));
}

void menu_half_down(struct Menu *menu)
{
  menu_length_jump(menu, menu->pagelen / 2);
}

void menu_half_up(struct Menu *menu)
{
  menu_length_jump(menu, 0 - menu->pagelen / 2);
}

void menu_top_page(struct Menu *menu)
{
  if (menu->current != menu->top)
  {
    menu->current = menu->top;
    menu->redraw = REDRAW_MOTION;
  }
}

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
    mutt_error(_("No entries."));
}

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
    mutt_error(_("No entries."));
}

void menu_first_entry(struct Menu *menu)
{
  if (menu->max)
  {
    menu->current = 0;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("No entries."));
}

void menu_last_entry(struct Menu *menu)
{
  if (menu->max)
  {
    menu->current = menu->max - 1;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("No entries."));
}

void menu_current_top(struct Menu *menu)
{
  if (menu->max)
  {
    menu->top = menu->current;
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_error(_("No entries."));
}

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
    mutt_error(_("No entries."));
}

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
    mutt_error(_("No entries."));
}

static void menu_next_entry(struct Menu *menu)
{
  if (menu->current < menu->max - 1)
  {
    menu->current++;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("You are on the last entry."));
}

static void menu_prev_entry(struct Menu *menu)
{
  if (menu->current)
  {
    menu->current--;
    menu->redraw = REDRAW_MOTION;
  }
  else
    mutt_error(_("You are on the first entry."));
}

static int default_color(int i)
{
  return ColorDefs[MT_COLOR_NORMAL];
}

static int menu_search_generic(struct Menu *m, regex_t *re, int n)
{
  char buf[LONG_STRING];

  menu_make_entry(buf, sizeof(buf), m, n);
  return (regexec(re, buf, 0, NULL, 0));
}

void mutt_menu_init(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    SearchBuffers[i] = NULL;
}

struct Menu *mutt_menu_new(int menu)
{
  struct Menu *p = mutt_mem_calloc(1, sizeof(struct Menu));

  if ((menu < 0) || (menu >= MENU_MAX))
    menu = MENU_GENERIC;

  p->menu = menu;
  p->current = 0;
  p->top = 0;
  p->offset = 0;
  p->redraw = REDRAW_FULL;
  p->pagelen = MuttIndexWindow->rows;
  p->indexwin = MuttIndexWindow;
  p->statuswin = MuttStatusWindow;
  p->helpwin = MuttHelpWindow;
  p->messagewin = MuttMessageWindow;
  p->color = default_color;
  p->search = menu_search_generic;

  return p;
}

void mutt_menu_destroy(struct Menu **p)
{
  if ((*p)->dialog)
  {
    for (int i = 0; i < (*p)->max; i++)
      FREE(&(*p)->dialog[i]);

    FREE(&(*p)->dialog);
  }

  FREE(p);
}

static struct Menu *get_current_menu(void)
{
  return MenuStackCount ? MenuStack[MenuStackCount - 1] : NULL;
}

void mutt_menu_push_current(struct Menu *menu)
{
  if (MenuStackCount >= MenuStackLen)
  {
    MenuStackLen += 5;
    mutt_mem_realloc(&MenuStack, MenuStackLen * sizeof(struct Menu *));
  }

  MenuStack[MenuStackCount++] = menu;
  CurrentMenu = menu->menu;
}

void mutt_menu_pop_current(struct Menu *menu)
{
  struct Menu *prev_menu = NULL;

  if (!MenuStackCount || (MenuStack[MenuStackCount - 1] != menu))
  {
    mutt_debug(1, "called with inactive menu\n");
    return;
  }

  MenuStackCount--;
  prev_menu = get_current_menu();
  if (prev_menu)
  {
    CurrentMenu = prev_menu->menu;
    prev_menu->redraw = REDRAW_FULL;
  }
  else
  {
    CurrentMenu = MENU_MAIN;
  }
}

void mutt_menu_set_current_redraw(int redraw)
{
  struct Menu *current_menu = get_current_menu();
  if (current_menu)
    current_menu->redraw |= redraw;
}

void mutt_menu_set_current_redraw_full(void)
{
  struct Menu *current_menu = get_current_menu();
  if (current_menu)
    current_menu->redraw = REDRAW_FULL;
}

void mutt_menu_set_redraw(int menu_type, int redraw)
{
  if (CurrentMenu == menu_type)
    mutt_menu_set_current_redraw(redraw);
}

void mutt_menu_set_redraw_full(int menu_type)
{
  if (CurrentMenu == menu_type)
    mutt_menu_set_current_redraw_full();
}

void mutt_menu_current_redraw()
{
  struct Menu *current_menu = get_current_menu();
  if (current_menu)
  {
    if (menu_redraw(current_menu) == OP_REDRAW)
      /* On a REDRAW_FULL with a non-customized redraw, menu_redraw()
     * will return OP_REDRAW to give the calling menu-loop a chance to
     * customize output.
     */
      menu_redraw(current_menu);
  }
}

#define MUTT_SEARCH_UP 1
#define MUTT_SEARCH_DOWN 2

static int menu_search(struct Menu *menu, int op)
{
  int r = 0, wrap = 0;
  int search_dir;
  regex_t re;
  char buf[SHORT_STRING];
  char *search_buf =
      menu->menu >= 0 && menu->menu < MENU_MAX ? SearchBuffers[menu->menu] : NULL;

  if (!(search_buf && *search_buf) || (op != OP_SEARCH_NEXT && op != OP_SEARCH_OPPOSITE))
  {
    mutt_str_strfcpy(buf, search_buf && *search_buf ? search_buf : "", sizeof(buf));
    if (mutt_get_field((op == OP_SEARCH || op == OP_SEARCH_NEXT) ?
                           _("Search for: ") :
                           _("Reverse search for: "),
                       buf, sizeof(buf), MUTT_CLEAR) != 0 ||
        !buf[0])
      return -1;
    if (menu->menu >= 0 && menu->menu < MENU_MAX)
    {
      mutt_str_replace(&SearchBuffers[menu->menu], buf);
      search_buf = SearchBuffers[menu->menu];
    }
    menu->search_dir =
        (op == OP_SEARCH || op == OP_SEARCH_NEXT) ? MUTT_SEARCH_DOWN : MUTT_SEARCH_UP;
  }

  search_dir = (menu->search_dir == MUTT_SEARCH_UP) ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    search_dir = -search_dir;

  if (search_buf)
  {
    int flags = mutt_mb_is_lower(search_buf) ? REG_ICASE : 0;
    r = REGCOMP(&re, search_buf, REG_NOSUB | flags);
  }

  if (r != 0)
  {
    regerror(r, &re, buf, sizeof(buf));
    mutt_error("%s", buf);
    return -1;
  }

  r = menu->current + search_dir;
search_next:
  if (wrap)
    mutt_message(_("Search wrapped to top."));
  while (r >= 0 && r < menu->max)
  {
    if (menu->search(menu, &re, r) == 0)
    {
      regfree(&re);
      return r;
    }

    r += search_dir;
  }

  if (WrapSearch && wrap++ == 0)
  {
    r = search_dir == 1 ? 0 : menu->max - 1;
    goto search_next;
  }
  regfree(&re);
  mutt_error(_("Not found."));
  return -1;
}

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

static int menu_dialog_dokey(struct Menu *menu, int *ip)
{
  struct Event ch;
  char *p = NULL;

  ch = mutt_getch();

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
    mutt_unget_event(ch.op ? 0 : ch.ch, ch.op ? ch.op : 0);
    return -1;
  }
}

int menu_redraw(struct Menu *menu)
{
  if (menu->custom_menu_redraw)
  {
    menu->custom_menu_redraw(menu);
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
  else if (menu->redraw & (REDRAW_MOTION | REDRAW_MOTION_RESYNCH))
    menu_redraw_motion(menu);
  else if (menu->redraw == REDRAW_CURRENT)
    menu_redraw_current(menu);

  if (menu->dialog)
    menu_redraw_prompt(menu);

  return OP_NULL;
}

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
    if (OptMenuCaller)
    {
      OptMenuCaller = false;
      return OP_NULL;
    }

    /* Clear the tag prefix unless we just started it.  Don't clear
     * the prefix on a timeout (i==-2), but do clear on an abort (i==-1)
     */
    if (menu->tagprefix && i != OP_TAG_PREFIX && i != OP_TAG_PREFIX_COND && i != -2)
      menu->tagprefix = false;

    mutt_curs_set(0);

    if (menu_redraw(menu) == OP_REDRAW)
      return OP_REDRAW;

    /* give visual indication that the next command is a tag- command */
    if (menu->tagprefix)
    {
      mutt_window_mvaddstr(menu->messagewin, 0, 0, "tag-");
      mutt_window_clrtoeol(menu->messagewin);
    }

    menu->oldcurrent = menu->current;

    /* move the cursor out of the way */
    if (ArrowCursor)
      mutt_window_move(menu->indexwin, menu->current - menu->top + menu->offset, 2);
    else if (BrailleFriendly)
      mutt_window_move(menu->indexwin, menu->current - menu->top + menu->offset, 0);
    else
      mutt_window_move(menu->indexwin, menu->current - menu->top + menu->offset,
                       menu->indexwin->cols - 1);

    mutt_refresh();

    /* try to catch dialog keys before ops */
    if (menu->dialog && menu_dialog_dokey(menu, &i) == 0)
      return i;

    i = km_dokey(menu->menu);
    if (i == OP_TAG_PREFIX || i == OP_TAG_PREFIX_COND)
    {
      if (menu->tagprefix)
      {
        menu->tagprefix = false;
        mutt_window_clearline(menu->messagewin, 0);
        continue;
      }

      if (menu->tagged)
      {
        menu->tagprefix = true;
        continue;
      }
      else if (i == OP_TAG_PREFIX)
      {
        mutt_error(_("No tagged entries."));
        i = -1;
      }
      else /* None tagged, OP_TAG_PREFIX_COND */
      {
        mutt_flush_macro_to_endcond();
        mutt_message(_("Nothing to do."));
        i = -1;
      }
    }
    else if (menu->tagged && AutoTag)
      menu->tagprefix = true;

    mutt_curs_set(1);

    if (SigWinch)
    {
      mutt_resize_screen();
      SigWinch = 0;
      clearok(stdscr, true); /* force complete redraw */
    }

    if (i < 0)
    {
      if (menu->tagprefix)
        mutt_window_clearline(menu->messagewin, 0);
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
          menu->current = menu_search(menu, i);
          if (menu->current != -1)
            menu->redraw = REDRAW_MOTION;
          else
            menu->current = menu->oldcurrent;
        }
        else
          mutt_error(_("Search is not implemented for this menu."));
        break;

      case OP_JUMP:
        if (menu->dialog)
          mutt_error(_("Jumping is not implemented for dialogs."));
        else
          menu_jump(menu);
        break;

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        break;

      case OP_TAG:
        if (menu->tag && !menu->dialog)
        {
          if (menu->tagprefix && !AutoTag)
          {
            for (i = 0; i < menu->max; i++)
              menu->tagged += menu->tag(menu, i, 0);
            menu->redraw |= REDRAW_INDEX;
          }
          else if (menu->max)
          {
            int j = menu->tag(menu, menu->current, -1);
            menu->tagged += j;
            if (j && Resolve && menu->current < menu->max - 1)
            {
              menu->current++;
              menu->redraw |= REDRAW_MOTION_RESYNCH;
            }
            else
              menu->redraw |= REDRAW_CURRENT;
          }
          else
            mutt_error(_("No entries."));
        }
        else
          mutt_error(_("Tagging is not supported."));
        break;

      case OP_SHELL_ESCAPE:
        mutt_shell_escape();
        break;

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

      case OP_REDRAW:
        clearok(stdscr, true);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_HELP:
        mutt_help(menu->menu);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_NULL:
        km_error_key(menu->menu);
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
