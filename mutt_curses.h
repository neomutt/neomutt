/**
 * @file
 * Define wrapper functions around Curses/Slang
 *
 * @authors
 * Copyright (C) 1996-2000,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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

#ifndef _MUTT_CURSES_H
#define _MUTT_CURSES_H

#include <regex.h>
#include "mutt/mutt.h"
#include "options.h"
#include "mutt/queue.h"

#ifdef USE_SLANG_CURSES

#ifndef unix /* this symbol is not defined by the hp-ux compiler (sigh) */
#define unix
#endif /* unix */

#include <slang.h> /* in addition to slcurses.h, we need slang.h for the version
                      number to test for 2.x having UTF-8 support in main.c */
#ifdef bool
#undef bool
#endif

#include <slcurses.h>

#ifdef bool
#undef bool
#define bool _Bool
#endif

/* The prototypes for these four functions use "(char*)",
 * whereas the ncurses versions use "(const char*)" */
#undef addnstr
#undef addstr
#undef mvaddnstr
#undef mvaddstr

/* We redefine the helper macros to hide the compiler warnings */
#define addnstr(s, n)         waddnstr(stdscr, ((char *) s), (n))
#define addstr(x)             waddstr(stdscr, ((char *) x))
#define mvaddnstr(y, x, s, n) mvwaddnstr(stdscr, (y), (x), ((char *) s), (n))
#define mvaddstr(y, x, s)     mvwaddstr(stdscr, (y), (x), ((char *) s))

#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC

#else /* USE_SLANG_CURSES */

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#endif /* USE_SLANG_CURSES */

#define BEEP()                                                                 \
  do                                                                           \
  {                                                                            \
    if (Beep)                                                       \
      beep();                                                                  \
  } while (0)

#if !(defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
#define curs_set(x)
#endif

#if (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
void mutt_curs_set(int cursor);
#else
#define mutt_curs_set(x)
#endif

#define ctrl(c) ((c) - '@')

#ifdef KEY_ENTER
#define CI_is_return(c) ((c) == '\r' || (c) == '\n' || (c) == KEY_ENTER)
#else
#define CI_is_return(c) ((c) == '\r' || (c) == '\n')
#endif

/**
 * struct Event - An event such as a keypress
 */
struct Event
{
  int ch; /**< raw key pressed */
  int op; /**< function op */
};

struct Event mutt_getch(void);

void mutt_endwin(void);
void mutt_flushinp(void);
void mutt_refresh(void);
void mutt_resize_screen(void);
void mutt_unget_event(int ch, int op);
void mutt_unget_string(char *s);
void mutt_push_macro_event(int ch, int op);
void mutt_flush_macro_to_endcond(void);
void mutt_flush_unget_to_endcond(void);
void mutt_need_hard_redraw(void);

/* ----------------------------------------------------------------------------
 * Support for color
 */

/**
 * enum ColorId - List of all colored objects
 */
enum ColorId
{
  MT_COLOR_HDEFAULT = 0,
  MT_COLOR_QUOTED,
  MT_COLOR_SIGNATURE,
  MT_COLOR_INDICATOR,
  MT_COLOR_STATUS,
  MT_COLOR_TREE,
  MT_COLOR_NORMAL,
  MT_COLOR_ERROR,
  MT_COLOR_TILDE,
  MT_COLOR_MARKERS,
  MT_COLOR_BODY,
  MT_COLOR_HEADER,
  MT_COLOR_MESSAGE,
  MT_COLOR_ATTACHMENT,
  MT_COLOR_ATTACH_HEADERS,
  MT_COLOR_SEARCH,
  MT_COLOR_BOLD,
  MT_COLOR_UNDERLINE,
  MT_COLOR_PROMPT,
  MT_COLOR_PROGRESS,
#ifdef USE_SIDEBAR
  MT_COLOR_DIVIDER,
  MT_COLOR_FLAGGED,
  MT_COLOR_HIGHLIGHT,
  MT_COLOR_NEW,
  MT_COLOR_ORDINARY,
  MT_COLOR_SB_INDICATOR,
  MT_COLOR_SB_SPOOLFILE,
#endif
  MT_COLOR_MESSAGE_LOG,
  /* please no non-MT_COLOR_INDEX objects after this point */
  MT_COLOR_INDEX,
  MT_COLOR_INDEX_AUTHOR,
  MT_COLOR_INDEX_FLAGS,
  MT_COLOR_INDEX_TAG,
  MT_COLOR_INDEX_SUBJECT,
  /* below here - only index coloring stuff that doesn't have a pattern */
  MT_COLOR_INDEX_COLLAPSED,
  MT_COLOR_INDEX_DATE,
  MT_COLOR_INDEX_LABEL,
  MT_COLOR_INDEX_NUMBER,
  MT_COLOR_INDEX_SIZE,
  MT_COLOR_INDEX_TAGS,
  MT_COLOR_COMPOSE_HEADER,
  MT_COLOR_COMPOSE_SECURITY_ENCRYPT,
  MT_COLOR_COMPOSE_SECURITY_SIGN,
  MT_COLOR_COMPOSE_SECURITY_BOTH,
  MT_COLOR_COMPOSE_SECURITY_NONE,
  MT_COLOR_MAX
};

/**
 * struct ColorLine - A regular expression and a color to highlight a line
 */
struct ColorLine
{
  regex_t regex;
  int match; /**< which substringmap 0 for old behaviour */
  char *pattern;
  struct Pattern *color_pattern; /**< compiled pattern to speed up index color
                                      calculation */
  short fg;
  short bg;
  int pair;
  STAILQ_ENTRY(ColorLine) entries;
};
STAILQ_HEAD(ColorLineHead, ColorLine);

#define MUTT_PROGRESS_SIZE (1 << 0) /**< traffic-based progress */
#define MUTT_PROGRESS_MSG  (1 << 1) /**< message-based progress */

/**
 * struct Progress - A progress bar
 */
struct Progress
{
  unsigned short inc;
  unsigned short flags;
  const char *msg;
  long pos;
  size_t size;
  unsigned int timestamp;
  char sizestr[SHORT_STRING];
};

void mutt_progress_init(struct Progress *progress, const char *msg,
                        unsigned short flags, unsigned short inc, size_t size);
/* If percent is positive, it is displayed as percentage, otherwise
 * percentage is calculated from progress->size and pos if progress
 * was initialized with positive size, otherwise no percentage is shown */
void mutt_progress_update(struct Progress *progress, long pos, int percent);

/**
 * struct MuttWindow - A division of the screen
 *
 * Windows for different parts of the screen
 */
struct MuttWindow
{
  int rows;
  int cols;
  int row_offset;
  int col_offset;
};

extern struct MuttWindow *MuttHelpWindow;
extern struct MuttWindow *MuttIndexWindow;
extern struct MuttWindow *MuttStatusWindow;
extern struct MuttWindow *MuttMessageWindow;
#ifdef USE_SIDEBAR
extern struct MuttWindow *MuttSidebarWindow;
#endif

void mutt_init_windows(void);
void mutt_free_windows(void);
void mutt_reflow_windows(void);
int mutt_window_move(struct MuttWindow *win, int row, int col);
int mutt_window_mvaddch(struct MuttWindow *win, int row, int col, const chtype ch);
int mutt_window_mvaddstr(struct MuttWindow *win, int row, int col, const char *str);
int mutt_window_mvprintw(struct MuttWindow *win, int row, int col, const char *fmt, ...);
void mutt_window_clrtoeol(struct MuttWindow *win);
void mutt_window_clearline(struct MuttWindow *win, int row);
void mutt_window_getyx(struct MuttWindow *win, int *y, int *x);

static inline int mutt_window_wrap_cols(struct MuttWindow *win, short wrap)
{
  if (wrap < 0)
    return win->cols > -wrap ? win->cols + wrap : win->cols;
  else if (wrap)
    return wrap < win->cols ? wrap : win->cols;
  else
    return win->cols;
}

extern int *ColorQuote;
extern int ColorQuoteUsed;
extern int ColorDefs[];
extern struct ColorLineHead ColorHdrList;
extern struct ColorLineHead ColorBodyList;
extern struct ColorLineHead ColorAttachList;
extern struct ColorLineHead ColorStatusList;
extern struct ColorLineHead ColorIndexList;
extern struct ColorLineHead ColorIndexAuthorList;
extern struct ColorLineHead ColorIndexFlagsList;
extern struct ColorLineHead ColorIndexSubjectList;
extern struct ColorLineHead ColorIndexTagList;

void ci_start_color(void);

/* If the system has bkgdset() use it rather than attrset() so that the clr*()
 * functions will properly set the background attributes all the way to the
 * right column.
 */
#ifdef HAVE_BKGDSET
#define SETCOLOR(X)                                                            \
  do                                                                           \
  {                                                                            \
    if (ColorDefs[X] != 0)                                                     \
      bkgdset(ColorDefs[X] | ' ');                                             \
    else                                                                       \
      bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ');                               \
  } while (0)
#define ATTRSET(X) bkgdset(X | ' ')
#else
#define SETCOLOR(X)                                                            \
  do                                                                           \
  {                                                                            \
    if (ColorDefs[X] != 0)                                                     \
      attrset(ColorDefs[X]);                                                   \
    else                                                                       \
      attrset(ColorDefs[MT_COLOR_NORMAL]);                                     \
  } while (0)
#define ATTRSET attrset
#endif

/* reset the color to the normal terminal color as defined by 'color normal ...' */
#ifdef HAVE_BKGDSET
#define NORMAL_COLOR bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ')
#else
#define NORMAL_COLOR attrset(ColorDefs[MT_COLOR_NORMAL])
#endif

#endif /* _MUTT_CURSES_H */
