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

#ifndef MUTT_MUTT_CURSES_H
#define MUTT_MUTT_CURSES_H

#include <regex.h>
#include "mutt/mutt.h"
#include "pattern.h"

#ifdef USE_SLANG_CURSES

#ifndef unix /* this symbol is not defined by the hp-ux compiler */
#define unix
#endif

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
#define addnstr(str, n)         waddnstr(stdscr, ((char *) str), (n))
#define addstr(x)               waddstr(stdscr, ((char *) x))
#define mvaddnstr(y, x, str, n) mvwaddnstr(stdscr, (y), (x), ((char *) str), (n))
#define mvaddstr(y, x, str)     mvwaddstr(stdscr, (y), (x), ((char *) str))

#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC

#else /* USE_SLANG_CURSES */

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif

#endif /* USE_SLANG_CURSES */

#define BEEP()                                                                 \
  do                                                                           \
  {                                                                            \
    if (C_Beep)                                                                \
      beep();                                                                  \
  } while (false)

#if !(defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
#define curs_set(x)
#endif

#if (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
void mutt_curs_set(int cursor);
#else
#define mutt_curs_set(x)
#endif

#define ctrl(ch) ((ch) - '@')

#ifdef KEY_ENTER
#define CI_is_return(ch) (((ch) == '\r') || ((ch) == '\n') || ((ch) == KEY_ENTER))
#else
#define CI_is_return(ch) (((ch) == '\r') || ((ch) == '\n'))
#endif

/**
 * struct KeyEvent - An event such as a keypress
 */
struct KeyEvent
{
  int ch; /**< raw key pressed */
  int op; /**< function op */
};

void mutt_resize_screen(void);

/* ----------------------------------------------------------------------------
 * Support for color
 */

/**
 * enum ColorId - List of all colored objects
 */
enum ColorId
{
  MT_COLOR_HDEFAULT = 0, ///< Header default colour
  MT_COLOR_QUOTED,       ///< Pager: quoted text
  MT_COLOR_SIGNATURE,    ///< Pager: signature lines
  MT_COLOR_INDICATOR,    ///< Selected item in list
  MT_COLOR_STATUS,       ///< Status bar
  MT_COLOR_TREE,         ///< Index: tree-drawing characters
  MT_COLOR_NORMAL,       ///< Plain text
  MT_COLOR_ERROR,        ///< Error message
  MT_COLOR_TILDE,        ///< Pager: empty lines after message
  MT_COLOR_MARKERS,      ///< Pager: markers, line continuation
  MT_COLOR_BODY,         ///< Pager: highlight body of message (takes a pattern)
  MT_COLOR_HEADER,       ///< Message headers (takes a pattern)
  MT_COLOR_MESSAGE,      ///< Informational message
  MT_COLOR_ATTACHMENT,   ///< MIME attachments text (entire line)
  MT_COLOR_ATTACH_HEADERS, ///< MIME attachment test (takes a pattern)
  MT_COLOR_SEARCH,         ///< Pager: search matches
  MT_COLOR_BOLD,           ///< Bold text
  MT_COLOR_UNDERLINE,      ///< Underlined text
  MT_COLOR_PROMPT,         ///< Question/user input
  MT_COLOR_PROGRESS,       ///< Progress bar
#ifdef USE_SIDEBAR
  MT_COLOR_DIVIDER,      ///< Line dividing sidebar from the index/pager
  MT_COLOR_FLAGGED,      ///< Mailbox with flagged messages
  MT_COLOR_HIGHLIGHT,    ///< Select cursor
  MT_COLOR_NEW,          ///< Mailbox with new mail
  MT_COLOR_ORDINARY,     ///< Mailbox with no new or flagged messages
  MT_COLOR_SB_INDICATOR, ///< Current open mailbox
  MT_COLOR_SB_SPOOLFILE, ///< $spoolfile (Spool mailbox)
#endif
  MT_COLOR_MESSAGE_LOG, ///< Menu showing log messages
  /* please no non-MT_COLOR_INDEX objects after this point */
  MT_COLOR_INDEX,         ///< Index: default colour (takes a pattern)
  MT_COLOR_INDEX_AUTHOR,  ///< Index: author field (takes a pattern)
  MT_COLOR_INDEX_FLAGS,   ///< Index: flags field (takes a pattern)
  MT_COLOR_INDEX_TAG,     ///< Index: tag field (%g, takes a pattern)
  MT_COLOR_INDEX_SUBJECT, ///< Index: subject field (takes a pattern)
  /* below here - only index coloring stuff that doesn't have a pattern */
  MT_COLOR_INDEX_COLLAPSED, ///< Index: number of messages in collapsed thread
  MT_COLOR_INDEX_DATE,      ///< Index: date field
  MT_COLOR_INDEX_LABEL,     ///< Index: label field
  MT_COLOR_INDEX_NUMBER,    ///< Index: index number
  MT_COLOR_INDEX_SIZE,      ///< Index: size field
  MT_COLOR_INDEX_TAGS,      ///< Index: tags field (%g, %J)
  MT_COLOR_COMPOSE_HEADER,  ///< Header labels, e.g. From:
  MT_COLOR_COMPOSE_SECURITY_ENCRYPT, ///< Mail will be encrypted
  MT_COLOR_COMPOSE_SECURITY_SIGN,    ///< Mail will be signed
  MT_COLOR_COMPOSE_SECURITY_BOTH,    ///< Mail will be encrypted and signed
  MT_COLOR_COMPOSE_SECURITY_NONE,    ///< Mail will not be encrypted or signed
  MT_COLOR_OPTIONS,                  ///< Options in prompt
  MT_COLOR_MAX,
};

/**
 * struct ColorLine - A regular expression and a color to highlight a line
 */
struct ColorLine
{
  regex_t regex;                     ///< Compiled regex
  int match;                         ///< Substring to match, 0 for old behaviour
  char *pattern;                     ///< Pattern to match
  struct PatternList *color_pattern; ///< compiled pattern to speed up index color calculation
  uint32_t fg;                       ///< Foreground colour
  uint32_t bg;                       ///< Background colour
  int pair;                          ///< Colour pair index

  bool stop_matching : 1;            ///< used by the pager for body patterns, to prevent the color from being retried once it fails

  STAILQ_ENTRY(ColorLine) entries;   ///< Linked list
};
STAILQ_HEAD(ColorLineList, ColorLine);

extern int *ColorQuote;                            ///< Array of colours for quoted email text
extern int ColorQuoteUsed;                         ///< Number of colours for quoted email text
extern int ColorDefs[];                            ///< Array of all fixed colours, see enum ColorId
extern struct ColorLineList ColorHdrList;          ///< List of colours applied to the email headers
extern struct ColorLineList ColorBodyList;         ///< List of colours applied to the email body
extern struct ColorLineList ColorAttachList;       ///< List of colours applied to the attachment headers
extern struct ColorLineList ColorStatusList;       ///< List of colours applied to the status bar
extern struct ColorLineList ColorIndexList;        ///< List of default colours applied to the index
extern struct ColorLineList ColorIndexAuthorList;  ///< List of colours applied to the author in the index
extern struct ColorLineList ColorIndexFlagsList;   ///< List of colours applied to the flags in the index
extern struct ColorLineList ColorIndexSubjectList; ///< List of colours applied to the subject in the index
extern struct ColorLineList ColorIndexTagList;     ///< List of colours applied to tags in the index

/* If the system has bkgdset() use it rather than attrset() so that the clr*()
 * functions will properly set the background attributes all the way to the
 * right column.
 */
#ifdef HAVE_BKGDSET
#define SET_COLOR(X)                                                            \
  do                                                                           \
  {                                                                            \
    if (ColorDefs[X] != 0)                                                     \
      bkgdset(ColorDefs[X] | ' ');                                             \
    else                                                                       \
      bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ');                               \
  } while (false)
#define ATTR_SET(X) bkgdset(X | ' ')
#else
#define SET_COLOR(X)                                                            \
  do                                                                           \
  {                                                                            \
    if (ColorDefs[X] != 0)                                                     \
      attrset(ColorDefs[X]);                                                   \
    else                                                                       \
      attrset(ColorDefs[MT_COLOR_NORMAL]);                                     \
  } while (false)
#define ATTR_SET attrset
#endif

/* reset the color to the normal terminal color as defined by 'color normal ...' */
#ifdef HAVE_BKGDSET
#define NORMAL_COLOR bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ')
#else
#define NORMAL_COLOR attrset(ColorDefs[MT_COLOR_NORMAL])
#endif

#endif /* MUTT_MUTT_CURSES_H */
