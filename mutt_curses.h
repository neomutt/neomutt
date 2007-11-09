/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#ifndef _MUTT_CURSES_H_
#define _MUTT_CURSES_H_ 1

#ifdef USE_SLANG_CURSES

#ifndef unix /* this symbol is not defined by the hp-ux compiler (sigh) */
#define unix
#endif /* unix */

#include <slang.h>	/* in addition to slcurses.h, we need slang.h for the version
			   number to test for 2.x having UTF-8 support in main.c */
#include <slcurses.h>

#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC

/*
 * ncurses and SLang seem to send different characters when the Enter key is
 * pressed, so define some macros to properly detect the Enter key.
 */
#define M_ENTER_C '\r'
#define M_ENTER_S "\r"

#else /* USE_SLANG_CURSES */

#if HAVE_NCURSESW_NCURSES_H
# include <ncursesw/ncurses.h>
#elif HAVE_NCURSES_NCURSES_H
# include <ncurses/ncurses.h>
#elif HAVE_NCURSES_H
# include <ncurses.h>
#else
# include <curses.h>
#endif

#define M_ENTER_C '\n'
#define M_ENTER_S "\n"

#endif /* USE_SLANG_CURSES */

/* AIX defines ``lines'' in <term.h>, but it's used as a var name in
 * various places in Mutt
 */
#ifdef lines
#undef lines
#endif /* lines */

#define CLEARLINE(x) move(x,0), clrtoeol()
#define CENTERLINE(x,y) move(y, (COLS-strlen(x))/2), addstr(x)
#define BEEP() do { if (option (OPTBEEP)) beep(); } while (0)

#if ! (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
#define curs_set(x)
#endif

#if !defined(USE_SLANG_CURSES) && defined(HAVE_BKGDSET)
#define BKGDSET(x) bkgdset (ColorDefs[x] | ' ')
#else
#define BKGDSET(x)
#endif

#if (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
void mutt_curs_set (int);
#else
#define mutt_curs_set(x)
#endif
#define PAGELEN (LINES-3)

#define ctrl(c) ((c)-'@')

#ifdef KEY_ENTER
#define CI_is_return(c) ((c) == '\r' || (c) == '\n' || (c) == KEY_ENTER)
#else
#define CI_is_return(c) ((c) == '\r' || (c) == '\n')
#endif

event_t mutt_getch (void);

void mutt_endwin (const char *);
void mutt_flushinp (void);
void mutt_refresh (void);
void mutt_resize_screen (void);
void mutt_ungetch (int, int);
void mutt_need_hard_redraw (void);

/* ----------------------------------------------------------------------------
 * Support for color
 */

enum
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
  MT_COLOR_SEARCH,
  MT_COLOR_BOLD,
  MT_COLOR_UNDERLINE,
  MT_COLOR_INDEX,
  MT_COLOR_MAX
};

typedef struct color_line
{
  regex_t rx;
  char *pattern;
  pattern_t *color_pattern; /* compiled pattern to speed up index color
                               calculation */
  short fg;
  short bg;
  int pair;
  struct color_line *next;
} COLOR_LINE;

#define M_PROGRESS_SIZE		(1<<0)	/* traffic-based progress */
#define M_PROGRESS_MSG		(1<<1)	/* message-based progress */

typedef struct
{
  unsigned short inc;
  unsigned short flags;
  const char* msg;
  long pos;
  long size;
  unsigned int timestamp;
  char sizestr[SHORT_STRING];
} progress_t;

void mutt_progress_init (progress_t* progress, const char *msg,
			 unsigned short flags, unsigned short inc,
			 long size);
/* If percent is positive, it is displayed as percentage, otherwise
 * percentage is calculated from progress->size and pos if progress
 * was initialized with positive size, otherwise no percentage is shown */
void mutt_progress_update (progress_t* progress, long pos, int percent);

static inline int mutt_term_width(short wrap)
{
  if (wrap < 0)
    return COLS > -wrap ? COLS + wrap : COLS;
  else if (wrap)
    return wrap < COLS ? wrap : COLS;
  else
    return COLS;
}

extern int *ColorQuote;
extern int ColorQuoteUsed;
extern int ColorDefs[];
extern COLOR_LINE *ColorHdrList;
extern COLOR_LINE *ColorBodyList;
extern COLOR_LINE *ColorIndexList;

void ci_init_color (void);
void ci_start_color (void);

#define SETCOLOR(X) attrset(ColorDefs[X])
#define ADDCOLOR(X) attron(ColorDefs[X])

#define MAYBE_REDRAW(x) if (option (OPTNEEDREDRAW)) { unset_option (OPTNEEDREDRAW); x = REDRAW_FULL; }

/* ----------------------------------------------------------------------------
 * These are here to avoid compiler warnings with -Wall under SunOS 4.1.x
 */

#if !defined(STDC_HEADERS) && !defined(NCURSES_VERSION) && !defined(USE_SLANG_CURSES)
extern int endwin();
extern int printw();
extern int beep();
extern int isendwin();
extern int w32addch();
extern int keypad();
extern int wclrtobot();
extern int mvprintw();
extern int getcurx();
extern int getcury();
extern int noecho();
extern int wdelch();
extern int wrefresh();
extern int wmove();
extern int wclear();
extern int waddstr();
extern int wclrtoeol();
#endif

#endif /* _MUTT_CURSES_H_ */
