/**
 * @file
 * GUI miscellaneous curses (window drawing) routines
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2012-2013 Michael R. Elkins <me@mutt.org>
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

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "context.h"
#include "enter_state.h"
#include "globals.h"
#include "header.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "protos.h"
#ifdef HAVE_ISWBLANK
#include <wctype.h>
#endif
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

/* not possible to unget more than one char under some curses libs, and it
 * is impossible to unget function keys in SLang, so roll our own input
 * buffering routines.
 */

/* These are used for macros and exec/push commands.
 * They can be temporarily ignored by setting OPT_IGNORE_MACRO_EVENTS
 */
static size_t MacroBufferCount = 0;
static size_t MacroBufferLen = 0;
static struct Event *MacroEvents;

/* These are used in all other "normal" situations, and are not
 * ignored when setting OPT_IGNORE_MACRO_EVENTS
 */
static size_t UngetCount = 0;
static size_t UngetLen = 0;
static struct Event *UngetKeyEvents;

struct MuttWindow *MuttHelpWindow = NULL;
struct MuttWindow *MuttIndexWindow = NULL;
struct MuttWindow *MuttStatusWindow = NULL;
struct MuttWindow *MuttMessageWindow = NULL;
#ifdef USE_SIDEBAR
struct MuttWindow *MuttSidebarWindow = NULL;
#endif

static void reflow_message_window_rows(int mw_rows);

void mutt_refresh(void)
{
  /* don't refresh when we are waiting for a child. */
  if (OPT_KEEP_QUIET)
    return;

  /* don't refresh in the middle of macros unless necessary */
  if (MacroBufferCount && !OPT_FORCE_REFRESH && !OPT_IGNORE_MACRO_EVENTS)
    return;

  /* else */
  refresh();
}

/**
 * mutt_need_hard_redraw - Force a hard refresh
 *
 * Make sure that the next refresh does a full refresh.  This could be
 * optimized by not doing it at all if DISPLAY is set as this might indicate
 * that a GUI based pinentry was used.  Having an option to customize this is
 * of course the NeoMutt way.
 */
void mutt_need_hard_redraw(void)
{
  keypad(stdscr, true);
  clearok(stdscr, true);
  mutt_set_current_menu_redraw_full();
}

struct Event mutt_getch(void)
{
  int ch;
  struct Event err = { -1, OP_NULL }, ret;
  struct Event timeout = { -2, OP_NULL };

  if (UngetCount)
    return UngetKeyEvents[--UngetCount];

  if (!OPT_IGNORE_MACRO_EVENTS && MacroBufferCount)
    return MacroEvents[--MacroBufferCount];

  SigInt = 0;

  mutt_sig_allow_interrupt(1);
#ifdef KEY_RESIZE
  /* ncurses 4.2 sends this when the screen is resized */
  ch = KEY_RESIZE;
  while (ch == KEY_RESIZE)
#endif /* KEY_RESIZE */
    ch = getch();
  mutt_sig_allow_interrupt(0);

  if (SigInt)
  {
    mutt_query_exit();
    return err;
  }

  /* either timeout, a sigwinch (if timeout is set), or the terminal
   * has been lost */
  if (ch == ERR)
  {
    if (!isatty(0))
      mutt_exit(1);

    return timeout;
  }

  if ((ch & 0x80) && MetaKey)
  {
    /* send ALT-x as ESC-x */
    ch &= ~0x80;
    mutt_unget_event(ch, 0);
    ret.ch = '\033';
    ret.op = 0;
    return ret;
  }

  ret.ch = ch;
  ret.op = 0;
  return (ch == ctrl('G') ? err : ret);
}

int mutt_get_field_full(const char *field, char *buf, size_t buflen,
                        int complete, int multiple, char ***files, int *numfiles)
{
  int ret;
  int x;

  struct EnterState *es = mutt_new_enter_state();

  do
  {
    if (SigWinch)
    {
      SigWinch = 0;
      mutt_resize_screen();
      clearok(stdscr, TRUE);
      mutt_current_menu_redraw();
    }
    mutt_window_clearline(MuttMessageWindow, 0);
    SETCOLOR(MT_COLOR_PROMPT);
    addstr((char *) field); /* cast to get around bad prototypes */
    NORMAL_COLOR;
    mutt_refresh();
    mutt_window_getyx(MuttMessageWindow, NULL, &x);
    ret = mutt_enter_string_full(buf, buflen, x, complete, multiple, files, numfiles, es);
  } while (ret == 1);
  mutt_window_clearline(MuttMessageWindow, 0);
  mutt_free_enter_state(&es);

  return ret;
}

int mutt_get_field_unbuffered(char *msg, char *buf, size_t buflen, int flags)
{
  int rc;

  OPT_IGNORE_MACRO_EVENTS = true;
  rc = mutt_get_field(msg, buf, buflen, flags);
  OPT_IGNORE_MACRO_EVENTS = false;

  return rc;
}

void mutt_edit_file(const char *editor, const char *data)
{
  char cmd[LONG_STRING];

  mutt_endwin();
  mutt_expand_file_fmt(cmd, sizeof(cmd), editor, data);
  if (mutt_system(cmd) != 0)
  {
    mutt_error(_("Error running \"%s\"!"), cmd);
  }
  /* the terminal may have been resized while the editor owned it */
  mutt_resize_screen();
  keypad(stdscr, true);
  clearok(stdscr, true);
}

int mutt_yesorno(const char *msg, int def)
{
  struct Event ch;
  char *yes = _("yes");
  char *no = _("no");
  char *answer_string = NULL;
  int answer_string_wid, msg_wid;
  size_t trunc_msg_len;
  bool redraw = true;
  int prompt_lines = 1;

  char *expr = NULL;
  regex_t reyes;
  regex_t reno;
  int reyes_ok;
  int reno_ok;
  char answer[2];

  answer[1] = '\0';

  reyes_ok = (expr = nl_langinfo(YESEXPR)) && (expr[0] == '^') &&
             (REGCOMP(&reyes, expr, REG_NOSUB) == 0);
  reno_ok = (expr = nl_langinfo(NOEXPR)) && (expr[0] == '^') &&
            (REGCOMP(&reno, expr, REG_NOSUB) == 0);

  /*
   * In order to prevent the default answer to the question to wrapped
   * around the screen in the even the question is wider than the screen,
   * ensure there is enough room for the answer and truncate the question
   * to fit.
   */
  safe_asprintf(&answer_string, " ([%s]/%s): ", def == MUTT_YES ? yes : no,
                def == MUTT_YES ? no : yes);
  answer_string_wid = mutt_strwidth(answer_string);
  msg_wid = mutt_strwidth(msg);

  while (true)
  {
    if (redraw || SigWinch)
    {
      redraw = false;
      if (SigWinch)
      {
        SigWinch = 0;
        mutt_resize_screen();
        clearok(stdscr, TRUE);
        mutt_current_menu_redraw();
      }
      if (MuttMessageWindow->cols)
      {
        prompt_lines = (msg_wid + answer_string_wid + MuttMessageWindow->cols - 1) /
                       MuttMessageWindow->cols;
        prompt_lines = MAX(1, MIN(3, prompt_lines));
      }
      if (prompt_lines != MuttMessageWindow->rows)
      {
        reflow_message_window_rows(prompt_lines);
        mutt_current_menu_redraw();
      }

      /* maxlen here is sort of arbitrary, so pick a reasonable upper bound */
      trunc_msg_len = mutt_wstr_trunc(
          msg, 4 * prompt_lines * MuttMessageWindow->cols,
          prompt_lines * MuttMessageWindow->cols - answer_string_wid, NULL);

      mutt_window_move(MuttMessageWindow, 0, 0);
      SETCOLOR(MT_COLOR_PROMPT);
      addnstr(msg, trunc_msg_len);
      addstr(answer_string);
      NORMAL_COLOR;
      mutt_window_clrtoeol(MuttMessageWindow);
    }

    mutt_refresh();
    /* SigWinch is not processed unless timeout is set */
    timeout(30 * 1000);
    ch = mutt_getch();
    timeout(-1);
    if (ch.ch == -2)
      continue;
    if (CI_is_return(ch.ch))
      break;
    if (ch.ch < 0)
    {
      def = MUTT_ABORT;
      break;
    }

    answer[0] = ch.ch;
    if (reyes_ok ? (regexec(&reyes, answer, 0, 0, 0) == 0) : (tolower(ch.ch) == 'y'))
    {
      def = MUTT_YES;
      break;
    }
    else if (reno_ok ? (regexec(&reno, answer, 0, 0, 0) == 0) : (tolower(ch.ch) == 'n'))
    {
      def = MUTT_NO;
      break;
    }
    else
    {
      BEEP();
    }
  }

  FREE(&answer_string);

  if (reyes_ok)
    regfree(&reyes);
  if (reno_ok)
    regfree(&reno);

  if (MuttMessageWindow->rows != 1)
  {
    reflow_message_window_rows(1);
    mutt_current_menu_redraw();
  }
  else
    mutt_window_clearline(MuttMessageWindow, 0);

  if (def != MUTT_ABORT)
  {
    addstr((char *) (def == MUTT_YES ? yes : no));
    mutt_refresh();
  }
  else
  {
    /* when the users cancels with ^G, clear the message stored with
     * mutt_message() so it isn't displayed when the screen is refreshed. */
    mutt_clear_error();
  }
  return def;
}

/**
 * mutt_query_exit - Ask the user if they want to leave NeoMutt
 *
 * This function is called when the user presses the abort key.
 */
void mutt_query_exit(void)
{
  mutt_flushinp();
  curs_set(1);
  if (Timeout)
    timeout(-1); /* restore blocking operation */
  if (mutt_yesorno(_("Exit NeoMutt?"), MUTT_YES) == MUTT_YES)
  {
    mutt_exit(1);
  }
  mutt_clear_error();
  mutt_curs_set(-1);
  SigInt = 0;
}

void mutt_progress_init(struct Progress *progress, const char *msg,
                        unsigned short flags, unsigned short inc, size_t size)
{
  struct timeval tv = { 0, 0 };

  if (!progress)
    return;
  if (OPT_NO_CURSES)
    return;

  memset(progress, 0, sizeof(struct Progress));
  progress->inc = inc;
  progress->flags = flags;
  progress->msg = msg;
  progress->size = size;
  if (progress->size)
  {
    if (progress->flags & MUTT_PROGRESS_SIZE)
      mutt_str_pretty_size(progress->sizestr, sizeof(progress->sizestr),
                           progress->size);
    else
      snprintf(progress->sizestr, sizeof(progress->sizestr), "%zu", progress->size);
  }
  if (!inc)
  {
    if (size)
      mutt_message("%s (%s)", msg, progress->sizestr);
    else
      mutt_message(msg);
    return;
  }
  if (gettimeofday(&tv, NULL) < 0)
    mutt_debug(1, "gettimeofday failed: %d\n", errno);
  /* if timestamp is 0 no time-based suppression is done */
  if (TimeInc)
    progress->timestamp =
        ((unsigned int) tv.tv_sec * 1000) + (unsigned int) (tv.tv_usec / 1000);
  mutt_progress_update(progress, 0, 0);
}

/**
 * message_bar - Draw a colourful progress bar
 * @param percent %age complete
 * @param fmt     printf(1)-like formatting string
 * @param ...     Arguments to formatting string
 */
static void message_bar(int percent, const char *fmt, ...)
{
  va_list ap;
  char buf[STRING], buf2[STRING];
  int w = percent * COLS / 100;
  size_t l;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  l = mutt_strwidth(buf);
  va_end(ap);

  mutt_simple_format(buf2, sizeof(buf2), 0, COLS - 2, FMT_LEFT, 0, buf, sizeof(buf), 0);

  move(LINES - 1, 0);

  if (ColorDefs[MT_COLOR_PROGRESS] == 0)
  {
    addstr(buf2);
  }
  else
  {
    if (l < w)
    {
      /* The string fits within the colour bar */
      SETCOLOR(MT_COLOR_PROGRESS);
      addstr(buf2);
      w -= l;
      while (w--)
      {
        addch(' ');
      }
      NORMAL_COLOR;
    }
    else
    {
      /* The string is too long for the colour bar */
      char ch;
      int off = mutt_wstr_trunc(buf2, sizeof(buf2), w, NULL);

      ch = buf2[off];
      buf2[off] = '\0';
      SETCOLOR(MT_COLOR_PROGRESS);
      addstr(buf2);
      buf2[off] = ch;
      NORMAL_COLOR;
      addstr(&buf2[off]);
    }
  }

  clrtoeol();
  mutt_refresh();
}

void mutt_progress_update(struct Progress *progress, long pos, int percent)
{
  char posstr[SHORT_STRING];
  bool update = false;
  struct timeval tv = { 0, 0 };
  unsigned int now = 0;

  if (OPT_NO_CURSES)
    return;

  if (!progress->inc)
    goto out;

  /* refresh if size > inc */
  if (progress->flags & MUTT_PROGRESS_SIZE && (pos >= progress->pos + (progress->inc << 10)))
    update = true;
  else if (pos >= progress->pos + progress->inc)
    update = true;

  /* skip refresh if not enough time has passed */
  if (update && progress->timestamp && !gettimeofday(&tv, NULL))
  {
    now = ((unsigned int) tv.tv_sec * 1000) + (unsigned int) (tv.tv_usec / 1000);
    if (now && now - progress->timestamp < TimeInc)
      update = false;
  }

  /* always show the first update */
  if (!pos)
    update = true;

  if (update)
  {
    if (progress->flags & MUTT_PROGRESS_SIZE)
    {
      pos = pos / (progress->inc << 10) * (progress->inc << 10);
      mutt_str_pretty_size(posstr, sizeof(posstr), pos);
    }
    else
      snprintf(posstr, sizeof(posstr), "%ld", pos);

    mutt_debug(5, "updating progress: %s\n", posstr);

    progress->pos = pos;
    if (now)
      progress->timestamp = now;

    if (progress->size > 0)
    {
      message_bar(
          (percent > 0) ? percent :
                          (int) (100.0 * (double) progress->pos / progress->size),
          "%s %s/%s (%d%%)", progress->msg, posstr, progress->sizestr,
          (percent > 0) ? percent :
                          (int) (100.0 * (double) progress->pos / progress->size));
    }
    else
    {
      if (percent > 0)
        message_bar(percent, "%s %s (%d%%)", progress->msg, posstr, percent);
      else
        mutt_message("%s %s", progress->msg, posstr);
    }
  }

out:
  if (pos >= progress->size)
    mutt_clear_error();
}

void mutt_init_windows(void)
{
  MuttHelpWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
  MuttIndexWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
  MuttStatusWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
  MuttMessageWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
#ifdef USE_SIDEBAR
  MuttSidebarWindow = mutt_mem_calloc(1, sizeof(struct MuttWindow));
#endif
}

void mutt_free_windows(void)
{
  FREE(&MuttHelpWindow);
  FREE(&MuttIndexWindow);
  FREE(&MuttStatusWindow);
  FREE(&MuttMessageWindow);
#ifdef USE_SIDEBAR
  FREE(&MuttSidebarWindow);
#endif
}

void mutt_reflow_windows(void)
{
  if (OPT_NO_CURSES)
    return;

  mutt_debug(2, "entering\n");

  MuttStatusWindow->rows = 1;
  MuttStatusWindow->cols = COLS;
  MuttStatusWindow->row_offset = StatusOnTop ? 0 : LINES - 2;
  MuttStatusWindow->col_offset = 0;

  memcpy(MuttHelpWindow, MuttStatusWindow, sizeof(struct MuttWindow));
  if (!Help)
    MuttHelpWindow->rows = 0;
  else
    MuttHelpWindow->row_offset = StatusOnTop ? LINES - 2 : 0;

  memcpy(MuttMessageWindow, MuttStatusWindow, sizeof(struct MuttWindow));
  MuttMessageWindow->row_offset = LINES - 1;

  memcpy(MuttIndexWindow, MuttStatusWindow, sizeof(struct MuttWindow));
  MuttIndexWindow->rows = MAX(
      LINES - MuttStatusWindow->rows - MuttHelpWindow->rows - MuttMessageWindow->rows, 0);
  MuttIndexWindow->row_offset =
      StatusOnTop ? MuttStatusWindow->rows : MuttHelpWindow->rows;

#ifdef USE_SIDEBAR
  if (SidebarVisible)
  {
    memcpy(MuttSidebarWindow, MuttIndexWindow, sizeof(struct MuttWindow));
    MuttSidebarWindow->cols = SidebarWidth;
    MuttIndexWindow->cols -= SidebarWidth;

    if (SidebarOnRight)
    {
      MuttSidebarWindow->col_offset = COLS - SidebarWidth;
    }
    else
    {
      MuttIndexWindow->col_offset += SidebarWidth;
    }
  }
#endif

  mutt_set_current_menu_redraw_full();
  /* the pager menu needs this flag set to recalc line_info */
  mutt_set_current_menu_redraw(REDRAW_FLOW);
}

static void reflow_message_window_rows(int mw_rows)
{
  MuttMessageWindow->rows = mw_rows;
  MuttMessageWindow->row_offset = LINES - mw_rows;

  MuttStatusWindow->row_offset = StatusOnTop ? 0 : LINES - mw_rows - 1;

  if (Help)
    MuttHelpWindow->row_offset = StatusOnTop ? LINES - mw_rows - 1 : 0;

  MuttIndexWindow->rows = MAX(
      LINES - MuttStatusWindow->rows - MuttHelpWindow->rows - MuttMessageWindow->rows, 0);

#ifdef USE_SIDEBAR
  if (SidebarVisible)
    MuttSidebarWindow->rows = MuttIndexWindow->rows;
#endif

  /* We don't also set REDRAW_FLOW because this function only
   * changes rows and is a temporary adjustment. */
  mutt_set_current_menu_redraw_full();
}

int mutt_window_move(struct MuttWindow *win, int row, int col)
{
  return move(win->row_offset + row, win->col_offset + col);
}

int mutt_window_mvaddch(struct MuttWindow *win, int row, int col, const chtype ch)
{
  return mvaddch(win->row_offset + row, win->col_offset + col, ch);
}

int mutt_window_mvaddstr(struct MuttWindow *win, int row, int col, const char *str)
{
  return mvaddstr(win->row_offset + row, win->col_offset + col, str);
}

#ifdef USE_SLANG_CURSES
static int vw_printw(SLcurses_Window_Type *win, const char *fmt, va_list ap)
{
  char buf[LONG_STRING];

  (void) SLvsnprintf(buf, sizeof(buf), (char *) fmt, ap);
  SLcurses_waddnstr(win, buf, -1);
  return 0;
}
#endif

int mutt_window_mvprintw(struct MuttWindow *win, int row, int col, const char *fmt, ...)
{
  int rc = mutt_window_move(win, row, col);
  if (rc != ERR)
  {
    va_list ap;
    va_start(ap, fmt);
    rc = vw_printw(stdscr, fmt, ap);
    va_end(ap);
  }

  return rc;
}

/**
 * mutt_window_clrtoeol - Clear to the end of the line
 *
 * Assumes the cursor has already been positioned within the window.
 */
void mutt_window_clrtoeol(struct MuttWindow *win)
{
  if (!win || !stdscr)
    return;

  if (win->col_offset + win->cols == COLS)
    clrtoeol();
  else
  {
    int row, col;
    getyx(stdscr, row, col);
    int curcol = col;
    while (curcol < win->col_offset + win->cols)
    {
      addch(' ');
      curcol++;
    }
    move(row, col);
  }
}

void mutt_window_clearline(struct MuttWindow *win, int row)
{
  mutt_window_move(win, row, 0);
  mutt_window_clrtoeol(win);
}

/**
 * mutt_window_getyx - Get the cursor position in the window
 *
 * Assumes the current position is inside the window.  Otherwise it will
 * happily return negative or values outside the window boundaries
 */
void mutt_window_getyx(struct MuttWindow *win, int *y, int *x)
{
  int row, col;

  getyx(stdscr, row, col);
  if (y)
    *y = row - win->row_offset;
  if (x)
    *x = col - win->col_offset;
}

void mutt_show_error(void)
{
  if (OPT_KEEP_QUIET || !ErrorBufMessage)
    return;

  SETCOLOR(OPT_MSG_ERR ? MT_COLOR_ERROR : MT_COLOR_MESSAGE);
  mutt_window_mvaddstr(MuttMessageWindow, 0, 0, ErrorBuf);
  NORMAL_COLOR;
  mutt_window_clrtoeol(MuttMessageWindow);
}

void mutt_endwin(void)
{
  if (OPT_NO_CURSES)
    return;

  int e = errno;

  /* at least in some situations (screen + xterm under SuSE11/12) endwin()
   * doesn't properly flush the screen without an explicit call.  */
  mutt_refresh();
  endwin();

  errno = e;
}

void mutt_perror_debug(const char *s)
{
  char *p = strerror(errno);

  mutt_debug(1, "%s: %s (errno = %d)\n", s, p ? p : "unknown error", errno);
  mutt_error("%s: %s (errno = %d)", s, p ? p : _("unknown error"), errno);
}

int mutt_any_key_to_continue(const char *s)
{
  struct termios t;
  struct termios old;
  int f, ch;

  f = open("/dev/tty", O_RDONLY);
  if (f < 0)
    return EOF;
  tcgetattr(f, &t);
  memcpy((void *) &old, (void *) &t, sizeof(struct termios)); /* save original state */
  t.c_lflag &= ~(ICANON | ECHO);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  tcsetattr(f, TCSADRAIN, &t);
  fflush(stdout);
  if (s)
    fputs(s, stdout);
  else
    fputs(_("Press any key to continue..."), stdout);
  fflush(stdout);
  ch = fgetc(stdin);
  fflush(stdin);
  tcsetattr(f, TCSADRAIN, &old);
  close(f);
  fputs("\r\n", stdout);
  mutt_clear_error();
  return (ch >= 0) ? ch : EOF;
}

int mutt_do_pager(const char *banner, const char *tempfile, int do_color, struct Pager *info)
{
  int rc;

  if (!Pager || (mutt_str_strcmp(Pager, "builtin") == 0))
    rc = mutt_pager(banner, tempfile, do_color, info);
  else
  {
    char cmd[STRING];

    mutt_endwin();
    mutt_expand_file_fmt(cmd, sizeof(cmd), Pager, tempfile);
    if (mutt_system(cmd) == -1)
    {
      mutt_error(_("Error running \"%s\"!"), cmd);
      rc = -1;
    }
    else
      rc = 0;
    mutt_file_unlink(tempfile);
  }

  return rc;
}

int mutt_enter_fname_full(const char *prompt, char *buf, size_t blen, int buffy,
                          int multiple, char ***files, int *numfiles, int flags)
{
  struct Event ch;

  SETCOLOR(MT_COLOR_PROMPT);
  mutt_window_mvaddstr(MuttMessageWindow, 0, 0, (char *) prompt);
  addstr(_(" ('?' for list): "));
  NORMAL_COLOR;
  if (buf[0])
    addstr(buf);
  mutt_window_clrtoeol(MuttMessageWindow);
  mutt_refresh();

  ch = mutt_getch();
  if (ch.ch < 0)
  {
    mutt_window_clearline(MuttMessageWindow, 0);
    return -1;
  }
  else if (ch.ch == '?')
  {
    mutt_refresh();
    buf[0] = '\0';

    if (!flags)
      flags = MUTT_SEL_FOLDER;
    if (multiple)
      flags |= MUTT_SEL_MULTI;
    if (buffy)
      flags |= MUTT_SEL_BUFFY;
    mutt_select_file(buf, blen, flags, files, numfiles);
  }
  else
  {
    char *pc = mutt_mem_malloc(mutt_str_strlen(prompt) + 3);

    sprintf(pc, "%s: ", prompt);
    mutt_unget_event(ch.op ? 0 : ch.ch, ch.op ? ch.op : 0);
    if (mutt_get_field_full(pc, buf, blen, (buffy ? MUTT_EFILE : MUTT_FILE) | MUTT_CLEAR,
                            multiple, files, numfiles) != 0)
    {
      buf[0] = '\0';
    }
    FREE(&pc);
#ifdef USE_NOTMUCH
    if ((flags & MUTT_SEL_VFOLDER) && buf[0] && (strncmp(buf, "notmuch://", 10) != 0))
      nm_description_to_path(buf, buf, blen);
#endif
  }

  return 0;
}

void mutt_unget_event(int ch, int op)
{
  struct Event tmp;

  tmp.ch = ch;
  tmp.op = op;

  if (UngetCount >= UngetLen)
    mutt_mem_realloc(&UngetKeyEvents, (UngetLen += 16) * sizeof(struct Event));

  UngetKeyEvents[UngetCount++] = tmp;
}

void mutt_unget_string(char *s)
{
  char *p = s + mutt_str_strlen(s) - 1;

  while (p >= s)
  {
    mutt_unget_event((unsigned char) *p--, 0);
  }
}

/**
 * mutt_push_macro_event - Add the character/operation to the macro buffer
 *
 * Adds the ch/op to the macro buffer.
 * This should be used for macros, push, and exec commands only.
 */
void mutt_push_macro_event(int ch, int op)
{
  struct Event tmp;

  tmp.ch = ch;
  tmp.op = op;

  if (MacroBufferCount >= MacroBufferLen)
    mutt_mem_realloc(&MacroEvents, (MacroBufferLen += 128) * sizeof(struct Event));

  MacroEvents[MacroBufferCount++] = tmp;
}

void mutt_flush_macro_to_endcond(void)
{
  UngetCount = 0;
  while (MacroBufferCount > 0)
  {
    if (MacroEvents[--MacroBufferCount].op == OP_END_COND)
      return;
  }
}

/**
 * mutt_flush_unget_to_endcond - Clear entries from UngetKeyEvents
 *
 * Normally, OP_END_COND should only be in the MacroEvent buffer.
 * km_error_key() (ab)uses OP_END_COND as a barrier in the unget buffer, and
 * calls this function to flush.
 */
void mutt_flush_unget_to_endcond(void)
{
  while (UngetCount > 0)
  {
    if (UngetKeyEvents[--UngetCount].op == OP_END_COND)
      return;
  }
}

void mutt_flushinp(void)
{
  UngetCount = 0;
  MacroBufferCount = 0;
  flushinp();
}

#if (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
/**
 * mutt_curs_set - Set the cursor position
 * @param cursor
 * * -1: restore the value of the last call
 * *  0: make the cursor invisible
 * *  1: make the cursor visible
 */
void mutt_curs_set(int cursor)
{
  static int SavedCursor = 1;

  if (cursor < 0)
    cursor = SavedCursor;
  else
    SavedCursor = cursor;

  if (curs_set(cursor) == ERR)
  {
    if (cursor == 1) /* cnorm */
      curs_set(2);   /* cvvis */
  }
}
#endif

int mutt_multi_choice(char *prompt, char *letters)
{
  struct Event ch;
  int choice;
  bool redraw = true;
  int prompt_lines = 1;
  char *p = NULL;

  while (true)
  {
    if (redraw || SigWinch)
    {
      redraw = false;
      if (SigWinch)
      {
        SigWinch = 0;
        mutt_resize_screen();
        clearok(stdscr, TRUE);
        mutt_current_menu_redraw();
      }
      if (MuttMessageWindow->cols)
      {
        prompt_lines = (mutt_strwidth(prompt) + MuttMessageWindow->cols - 1) /
                       MuttMessageWindow->cols;
        prompt_lines = MAX(1, MIN(3, prompt_lines));
      }
      if (prompt_lines != MuttMessageWindow->rows)
      {
        reflow_message_window_rows(prompt_lines);
        mutt_current_menu_redraw();
      }

      SETCOLOR(MT_COLOR_PROMPT);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 0, prompt);
      NORMAL_COLOR;
      mutt_window_clrtoeol(MuttMessageWindow);
    }

    mutt_refresh();
    /* SigWinch is not processed unless timeout is set */
    timeout(30 * 1000);
    ch = mutt_getch();
    timeout(-1);
    if (ch.ch == -2)
      continue;
    /* (ch.ch == 0) is technically possible.  Treat the same as < 0 (abort) */
    if (ch.ch <= 0 || CI_is_return(ch.ch))
    {
      choice = -1;
      break;
    }
    else
    {
      p = strchr(letters, ch.ch);
      if (p)
      {
        choice = p - letters + 1;
        break;
      }
      else if (ch.ch <= '9' && ch.ch > '0')
      {
        choice = ch.ch - '0';
        if (choice <= mutt_str_strlen(letters))
          break;
      }
    }
    BEEP();
  }
  if (MuttMessageWindow->rows != 1)
  {
    reflow_message_window_rows(1);
    mutt_current_menu_redraw();
  }
  else
    mutt_window_clearline(MuttMessageWindow, 0);
  mutt_refresh();
  return choice;
}

/**
 * mutt_addwch - addwch would be provided by an up-to-date curses library
 */
int mutt_addwch(wchar_t wc)
{
  char buf[MB_LEN_MAX * 2];
  mbstate_t mbstate;
  size_t n1, n2;

  memset(&mbstate, 0, sizeof(mbstate));
  if ((n1 = wcrtomb(buf, wc, &mbstate)) == (size_t)(-1) ||
      (n2 = wcrtomb(buf + n1, 0, &mbstate)) == (size_t)(-1))
  {
    return -1; /* ERR */
  }
  else
  {
    return addstr(buf);
  }
}

/**
 * mutt_simple_format - Format a string, like snprintf()
 * @param[out] buf       Buffer in which to save string
 * @param[in]  buflen    Buffer length
 * @param[in]  min_width Minimum width
 * @param[in]  max_width Maximum width
 * @param[in]  justify   Justification, e.g. #FMT_RIGHT
 * @param[in]  pad_char  Padding character
 * @param[in]  s         String to format
 * @param[in]  n         Number of bytes of string to format
 * @param[in]  arboreal  If true, string contains graphical tree characters
 *
 * This formats a string, a bit like snprintf(buf, buflen, "%-*.*s",
 * min_width, max_width, s), except that the widths refer to the number of
 * character cells when printed.
 */
void mutt_simple_format(char *buf, size_t buflen, int min_width, int max_width,
                        int justify, char pad_char, const char *s, size_t n, int arboreal)
{
  char *p = NULL;
  wchar_t wc;
  int w;
  size_t k, k2;
  char scratch[MB_LEN_MAX];
  mbstate_t mbstate1, mbstate2;
  int escaped = 0;

  memset(&mbstate1, 0, sizeof(mbstate1));
  memset(&mbstate2, 0, sizeof(mbstate2));
  buflen--;
  p = buf;
  for (; n && (k = mbrtowc(&wc, s, n, &mbstate1)); s += k, n -= k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1) && errno == EILSEQ)
        memset(&mbstate1, 0, sizeof(mbstate1));

      k = (k == (size_t)(-1)) ? 1 : n;
      wc = ReplacementChar;
    }
    if (escaped)
    {
      escaped = 0;
      w = 0;
    }
    else if (arboreal && wc == MUTT_SPECIAL_INDEX)
    {
      escaped = 1;
      w = 0;
    }
    else if (arboreal && wc < MUTT_TREE_MAX)
    {
      w = 1; /* hack */
    }
    else
    {
#ifdef HAVE_ISWBLANK
      if (iswblank(wc))
        wc = ' ';
      else
#endif
          if (!IsWPrint(wc))
        wc = '?';
      w = wcwidth(wc);
    }
    if (w >= 0)
    {
      if (w > max_width || (k2 = wcrtomb(scratch, wc, &mbstate2)) > buflen)
        continue;
      min_width -= w;
      max_width -= w;
      strncpy(p, scratch, k2);
      p += k2;
      buflen -= k2;
    }
  }
  w = (int) buflen < min_width ? buflen : min_width;
  if (w <= 0)
    *p = '\0';
  else if (justify == FMT_RIGHT) /* right justify */
  {
    p[w] = '\0';
    while (--p >= buf)
      p[w] = *p;
    while (--w >= 0)
      buf[w] = pad_char;
  }
  else if (justify == FMT_CENTER) /* center */
  {
    char *savedp = p;
    int half = (w + 1) / 2; /* half of cushion space */

    p[w] = '\0';

    /* move str to center of buffer */
    while (--p >= buf)
      p[half] = *p;

    /* fill rhs */
    p = savedp + half;
    while (--w >= half)
      *p++ = pad_char;

    /* fill lhs */
    while (half--)
      buf[half] = pad_char;
  }
  else /* left justify */
  {
    while (--w >= 0)
      *p++ = pad_char;
    *p = '\0';
  }
}

/**
 * format_s_x - Format a string like snprintf()
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  s        String to format
 * @param[in]  arboreal  If true, string contains graphical tree characters
 *
 * This formats a string rather like
 *   snprintf (fmt, sizeof (fmt), "%%%ss", prec);
 *   snprintf (buf, buflen, fmt, s);
 * except that the numbers in the conversion specification refer to
 * the number of character cells when printed.
 */
static void format_s_x(char *buf, size_t buflen, const char *prec, const char *s, int arboreal)
{
  int justify = FMT_RIGHT;
  char *p = NULL;
  int min_width;
  int max_width = INT_MAX;

  if (*prec == '-')
  {
    prec++;
    justify = FMT_LEFT;
  }
  else if (*prec == '=')
  {
    prec++;
    justify = FMT_CENTER;
  }
  min_width = strtol(prec, &p, 10);
  if (*p == '.')
  {
    prec = p + 1;
    max_width = strtol(prec, &p, 10);
    if (p <= prec)
      max_width = INT_MAX;
  }

  mutt_simple_format(buf, buflen, min_width, max_width, justify, ' ', s,
                     mutt_str_strlen(s), arboreal);
}

/**
 * mutt_format_s - Format a simple string
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  s        String to format
 */
void mutt_format_s(char *buf, size_t buflen, const char *prec, const char *s)
{
  format_s_x(buf, buflen, prec, s, 0);
}

/**
 * mutt_format_s_tree - Format a simple string with tree characters
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  s        String to format
 */
void mutt_format_s_tree(char *buf, size_t buflen, const char *prec, const char *s)
{
  format_s_x(buf, buflen, prec, s, 1);
}

/**
 * mutt_paddstr - Display a string on screen, padded if necessary
 * @param n Final width of field
 * @param s String to display
 */
void mutt_paddstr(int n, const char *s)
{
  wchar_t wc;
  size_t k;
  size_t len = mutt_str_strlen(s);
  mbstate_t mbstate;

  memset(&mbstate, 0, sizeof(mbstate));
  for (; len && (k = mbrtowc(&wc, s, len, &mbstate)); s += k, len -= k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t)(-1)) ? 1 : len;
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    const int w = wcwidth(wc);
    if (w >= 0)
    {
      if (w > n)
        break;
      addnstr((char *) s, k);
      n -= w;
    }
  }
  while (n-- > 0)
    addch(' ');
}

/**
 * mutt_wstr_trunc - Work out how to truncate a widechar string
 * @param[in]  src    String to measure
 * @param[in]  maxlen Maximum length of string in bytes
 * @param[in]  maxwid Maximum width in screen columns
 * @param[out] width  Save the truncated screen column width
 * @retval n Number of bytes to use
 *
 * See how many bytes to copy from string so it's at most maxlen bytes long and
 * maxwid columns wide
 */
size_t mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width)
{
  wchar_t wc;
  size_t n, w = 0, l = 0, cl;
  int cw;
  mbstate_t mbstate;

  if (!src)
    goto out;

  n = mutt_str_strlen(src);

  memset(&mbstate, 0, sizeof(mbstate));
  for (w = 0; n && (cl = mbrtowc(&wc, src, n, &mbstate)); src += cl, n -= cl)
  {
    if (cl == (size_t)(-1) || cl == (size_t)(-2))
    {
      if (cl == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      cl = (cl == (size_t)(-1)) ? 1 : n;
      wc = ReplacementChar;
    }
    cw = wcwidth(wc);
    /* hack because MUTT_TREE symbols aren't turned into characters
     * until rendered by print_enriched_string (#3364) */
    if ((cw < 0) && (src[0] == MUTT_SPECIAL_INDEX))
    {
      cl = 2; /* skip the index coloring sequence */
      cw = 0;
    }
    else if (cw < 0 && cl == 1 && src[0] && src[0] < MUTT_TREE_MAX)
      cw = 1;
    else if (cw < 0)
      cw = 0; /* unprintable wchar */
    if (cl + l > maxlen || cw + w > maxwid)
      break;
    l += cl;
    w += cw;
  }
out:
  if (width)
    *width = w;
  return l;
}

/**
 * mutt_strwidth - Measure a string's width in screen cells
 * @param s String to be measured
 * @retval n Number of screen cells string would use
 */
int mutt_strwidth(const char *s)
{
  wchar_t wc;
  int w;
  size_t k, n;
  mbstate_t mbstate;

  if (!s)
    return 0;

  n = mutt_str_strlen(s);

  memset(&mbstate, 0, sizeof(mbstate));
  for (w = 0; n && (k = mbrtowc(&wc, s, n, &mbstate)); s += k, n -= k)
  {
    if (*s == MUTT_SPECIAL_INDEX)
    {
      s += 2; /* skip the index coloring sequence */
      k = 0;
      continue;
    }

    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t)(-1)) ? 1 : n;
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    w += wcwidth(wc);
  }
  return w;
}

/**
 * message_is_visible - Is a message in the index within limit
 * @param ctx   Open mailbox
 * @param index Message ID (index into `ctx->hdrs[]`
 * @retval bool True if the message is within limit
 *
 * If no limit is in effect, all the messages are visible.
 */
bool message_is_visible(struct Context *ctx, int index)
{
  if (!ctx || !ctx->hdrs || (index >= ctx->msgcount))
    return false;

  return !ctx->pattern || ctx->hdrs[index]->limited;
}

/**
 * message_is_tagged - Is a message in the index tagged (and within limit)
 * @param ctx   Open mailbox
 * @param index Message ID (index into `ctx->hdrs[]`
 * @retval bool True if the message is both tagged and within limit
 *
 * If a limit is in effect, the message must be visible within it.
 */
bool message_is_tagged(struct Context *ctx, int index)
{
  return message_is_visible(ctx, index) && ctx->hdrs[index]->tagged;
}
