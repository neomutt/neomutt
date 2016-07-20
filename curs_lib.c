/*
 * Copyright (C) 1996-2002,2010,2012-2013 Michael R. Elkins <me@mutt.org>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "pager.h"
#include "mbyte.h"

#include <termios.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#ifdef HAVE_LANGINFO_YESEXPR
#include <langinfo.h>
#endif

/* not possible to unget more than one char under some curses libs, and it
 * is impossible to unget function keys in SLang, so roll our own input
 * buffering routines.
 */

/* These are used for macros and exec/push commands.
 * They can be temporarily ignored by setting OPTIGNOREMACROEVENTS
 */
static size_t MacroBufferCount = 0;
static size_t MacroBufferLen = 0;
static event_t *MacroEvents;

/* These are used in all other "normal" situations, and are not
 * ignored when setting OPTIGNOREMACROEVENTS
 */
static size_t UngetCount = 0;
static size_t UngetLen = 0;
static event_t *UngetKeyEvents;

mutt_window_t *MuttHelpWindow = NULL;
mutt_window_t *MuttIndexWindow = NULL;
mutt_window_t *MuttStatusWindow = NULL;
mutt_window_t *MuttMessageWindow = NULL;
#ifdef USE_SIDEBAR
mutt_window_t *MuttSidebarWindow = NULL;
#endif

void mutt_refresh (void)
{
  /* don't refresh when we are waiting for a child. */
  if (option (OPTKEEPQUIET))
    return;

  /* don't refresh in the middle of macros unless necessary */
  if (UngetCount && !option (OPTFORCEREFRESH))
    return;

  /* else */
  refresh ();
}

/* Make sure that the next refresh does a full refresh.  This could be
   optimized by not doing it at all if DISPLAY is set as this might
   indicate that a GUI based pinentry was used.  Having an option to
   customize this is of course the Mutt way.  */
void mutt_need_hard_redraw (void)
{
  keypad (stdscr, TRUE);
  clearok (stdscr, TRUE);
  set_option (OPTNEEDREDRAW);
}

event_t mutt_getch (void)
{
  int ch;
  event_t err = {-1, OP_NULL }, ret;
  event_t timeout = {-2, OP_NULL};

  if (UngetCount)
    return (UngetKeyEvents[--UngetCount]);

  if (!option(OPTIGNOREMACROEVENTS) && MacroBufferCount)
    return (MacroEvents[--MacroBufferCount]);

  SigInt = 0;

  mutt_allow_interrupt (1);
#ifdef KEY_RESIZE
  /* ncurses 4.2 sends this when the screen is resized */
  ch = KEY_RESIZE;
  while (ch == KEY_RESIZE)
#endif /* KEY_RESIZE */
    ch = getch ();
  mutt_allow_interrupt (0);

  if (SigInt)
  {
    mutt_query_exit ();
    return err;
  }

  if(ch == ERR)
  {
    /* either timeout or the terminal has been lost */
    if (!isatty (0))
    {
      endwin ();
      exit (1);
    }
    return timeout;
  }

  if ((ch & 0x80) && option (OPTMETAKEY))
  {
    /* send ALT-x as ESC-x */
    ch &= ~0x80;
    mutt_unget_event (ch, 0);
    ret.ch = '\033';
    ret.op = 0;
    return ret;
  }

  ret.ch = ch;
  ret.op = 0;
  return (ch == ctrl ('G') ? err : ret);
}

int _mutt_get_field (const char *field, char *buf, size_t buflen, int complete, int multiple, char ***files, int *numfiles)
{
  int ret;
  int x;

  ENTER_STATE *es = mutt_new_enter_state();
  
  do
  {
    mutt_window_clearline (MuttMessageWindow, 0);
    SETCOLOR (MT_COLOR_PROMPT);
    addstr ((char *)field); /* cast to get around bad prototypes */
    NORMAL_COLOR;
    mutt_refresh ();
    mutt_window_getyx (MuttMessageWindow, NULL, &x);
    ret = _mutt_enter_string (buf, buflen, x, complete, multiple, files, numfiles, es);
  }
  while (ret == 1);
  mutt_window_clearline (MuttMessageWindow, 0);
  mutt_free_enter_state (&es);
  
  return (ret);
}

int mutt_get_field_unbuffered (char *msg, char *buf, size_t buflen, int flags)
{
  int rc;

  set_option (OPTIGNOREMACROEVENTS);
  rc = mutt_get_field (msg, buf, buflen, flags);
  unset_option (OPTIGNOREMACROEVENTS);

  return (rc);
}

void mutt_clear_error (void)
{
  Errorbuf[0] = 0;
  if (!option(OPTNOCURSES))
    mutt_window_clearline (MuttMessageWindow, 0);
}

void mutt_edit_file (const char *editor, const char *data)
{
  char cmd[LONG_STRING];
  
  mutt_endwin (NULL);
  mutt_expand_file_fmt (cmd, sizeof (cmd), editor, data);
  if (mutt_system (cmd))
  {
    mutt_error (_("Error running \"%s\"!"), cmd);
    mutt_sleep (2);
  }
#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
  /* the terminal may have been resized while the editor owned it */
  mutt_resize_screen ();
#endif
  keypad (stdscr, TRUE);
  clearok (stdscr, TRUE);
}

int mutt_yesorno (const char *msg, int def)
{
  event_t ch;
  char *yes = _("yes");
  char *no = _("no");
  char *answer_string;
  size_t answer_string_len;
  size_t msglen;

#ifdef HAVE_LANGINFO_YESEXPR
  char *expr;
  regex_t reyes;
  regex_t reno;
  int reyes_ok;
  int reno_ok;
  char answer[2];

  answer[1] = 0;
  
  reyes_ok = (expr = nl_langinfo (YESEXPR)) && expr[0] == '^' &&
	     !REGCOMP (&reyes, expr, REG_NOSUB);
  reno_ok = (expr = nl_langinfo (NOEXPR)) && expr[0] == '^' &&
            !REGCOMP (&reno, expr, REG_NOSUB);
#endif

  mutt_window_clearline (MuttMessageWindow, 0);

  /*
   * In order to prevent the default answer to the question to wrapped
   * around the screen in the even the question is wider than the screen,
   * ensure there is enough room for the answer and truncate the question
   * to fit.
   */
  safe_asprintf (&answer_string, " ([%s]/%s): ", def == MUTT_YES ? yes : no, def == MUTT_YES ? no : yes);
  answer_string_len = mutt_strwidth (answer_string);
  /* maxlen here is sort of arbitrary, so pick a reasonable upper bound */
  msglen = mutt_wstr_trunc (msg, 4*MuttMessageWindow->cols, MuttMessageWindow->cols - answer_string_len, NULL);
  SETCOLOR (MT_COLOR_PROMPT);
  addnstr (msg, msglen);
  addstr (answer_string);
  NORMAL_COLOR;
  FREE (&answer_string);

  FOREVER
  {
    mutt_refresh ();
    ch = mutt_getch ();
    if (CI_is_return (ch.ch))
      break;
    if (ch.ch < 0)
    {
      def = -1;
      break;
    }

#ifdef HAVE_LANGINFO_YESEXPR
    answer[0] = ch.ch;
    if (reyes_ok ? 
	(regexec (& reyes, answer, 0, 0, 0) == 0) :
#else
    if (
#endif
	(tolower (ch.ch) == 'y'))
    {
      def = MUTT_YES;
      break;
    }
    else if (
#ifdef HAVE_LANGINFO_YESEXPR
	     reno_ok ?
	     (regexec (& reno, answer, 0, 0, 0) == 0) :
#endif
	     (tolower (ch.ch) == 'n'))
    {
      def = MUTT_NO;
      break;
    }
    else
    {
      BEEP();
    }
  }

#ifdef HAVE_LANGINFO_YESEXPR    
  if (reyes_ok)
    regfree (& reyes);
  if (reno_ok)
    regfree (& reno);
#endif

  if (def != -1)
  {
    addstr ((char *) (def == MUTT_YES ? yes : no));
    mutt_refresh ();
  }
  else
  {
    /* when the users cancels with ^G, clear the message stored with
     * mutt_message() so it isn't displayed when the screen is refreshed. */
    mutt_clear_error();
  }
  return (def);
}

/* this function is called when the user presses the abort key */
void mutt_query_exit (void)
{
  mutt_flushinp ();
  curs_set (1);
  if (Timeout)
    timeout (-1); /* restore blocking operation */
  if (mutt_yesorno (_("Exit Mutt?"), MUTT_YES) == MUTT_YES)
  {
    endwin ();
    exit (1);
  }
  mutt_clear_error();
  mutt_curs_set (-1);
  SigInt = 0;
}

static void curses_message (int error, const char *fmt, va_list ap)
{
  char scratch[LONG_STRING];

  vsnprintf (scratch, sizeof (scratch), fmt, ap);

  dprint (1, (debugfile, "%s\n", scratch));
  mutt_format_string (Errorbuf, sizeof (Errorbuf),
		      0, MuttMessageWindow->cols, FMT_LEFT, 0, scratch, sizeof (scratch), 0);

  if (!option (OPTKEEPQUIET))
  {
    if (error)
      BEEP ();
    SETCOLOR (error ? MT_COLOR_ERROR : MT_COLOR_MESSAGE);
    mutt_window_mvaddstr (MuttMessageWindow, 0, 0, Errorbuf);
    NORMAL_COLOR;
    mutt_window_clrtoeol (MuttMessageWindow);
    mutt_refresh ();
  }

  if (error)
    set_option (OPTMSGERR);
  else
    unset_option (OPTMSGERR);
}

void mutt_curses_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  curses_message (1, fmt, ap);
  va_end (ap);
}

void mutt_curses_message (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  curses_message (0, fmt, ap);
  va_end (ap);
}

void mutt_progress_init (progress_t* progress, const char *msg,
			 unsigned short flags, unsigned short inc,
			 long size)
{
  struct timeval tv = { 0, 0 };

  if (!progress)
    return;
  if (option(OPTNOCURSES))
    return;

  memset (progress, 0, sizeof (progress_t));
  progress->inc = inc;
  progress->flags = flags;
  progress->msg = msg;
  progress->size = size;
  if (progress->size) {
    if (progress->flags & MUTT_PROGRESS_SIZE)
      mutt_pretty_size (progress->sizestr, sizeof (progress->sizestr),
			progress->size);
    else
      snprintf (progress->sizestr, sizeof (progress->sizestr), "%ld",
		progress->size);
  }
  if (!inc)
  {
    if (size)
      mutt_message ("%s (%s)", msg, progress->sizestr);
    else
      mutt_message (msg);
    return;
  }
  if (gettimeofday (&tv, NULL) < 0)
    dprint (1, (debugfile, "gettimeofday failed: %d\n", errno));
  /* if timestamp is 0 no time-based suppression is done */
  if (TimeInc)
    progress->timestamp = ((unsigned int) tv.tv_sec * 1000)
        + (unsigned int) (tv.tv_usec / 1000);
  mutt_progress_update (progress, 0, 0);
}

void mutt_progress_update (progress_t* progress, long pos, int percent)
{
  char posstr[SHORT_STRING];
  short update = 0;
  struct timeval tv = { 0, 0 };
  unsigned int now = 0;

  if (option(OPTNOCURSES))
    return;

  if (!progress->inc)
    goto out;

  /* refresh if size > inc */
  if (progress->flags & MUTT_PROGRESS_SIZE &&
      (pos >= progress->pos + (progress->inc << 10)))
    update = 1;
  else if (pos >= progress->pos + progress->inc)
    update = 1;

  /* skip refresh if not enough time has passed */
  if (update && progress->timestamp && !gettimeofday (&tv, NULL)) {
    now = ((unsigned int) tv.tv_sec * 1000)
          + (unsigned int) (tv.tv_usec / 1000);
    if (now && now - progress->timestamp < TimeInc)
      update = 0;
  }

  /* always show the first update */
  if (!pos)
    update = 1;

  if (update)
  {
    if (progress->flags & MUTT_PROGRESS_SIZE)
    {
      pos = pos / (progress->inc << 10) * (progress->inc << 10);
      mutt_pretty_size (posstr, sizeof (posstr), pos);
    }
    else
      snprintf (posstr, sizeof (posstr), "%ld", pos);

    dprint (5, (debugfile, "updating progress: %s\n", posstr));

    progress->pos = pos;
    if (now)
      progress->timestamp = now;

    if (progress->size > 0)
    {
      mutt_message ("%s %s/%s (%d%%)", progress->msg, posstr, progress->sizestr,
		    percent > 0 ? percent :
		   	(int) (100.0 * (double) progress->pos / progress->size));
    }
    else
    {
      if (percent > 0)
	mutt_message ("%s %s (%d%%)", progress->msg, posstr, percent);
      else
	mutt_message ("%s %s", progress->msg, posstr);
    }
  }

out:
  if (pos >= progress->size)
    mutt_clear_error ();
}

void mutt_init_windows ()
{
  MuttHelpWindow = safe_calloc (sizeof (mutt_window_t), 1);
  MuttIndexWindow = safe_calloc (sizeof (mutt_window_t), 1);
  MuttStatusWindow = safe_calloc (sizeof (mutt_window_t), 1);
  MuttMessageWindow = safe_calloc (sizeof (mutt_window_t), 1);
#ifdef USE_SIDEBAR
  MuttSidebarWindow = safe_calloc (sizeof (mutt_window_t), 1);
#endif
}

void mutt_free_windows ()
{
  FREE (&MuttHelpWindow);
  FREE (&MuttIndexWindow);
  FREE (&MuttStatusWindow);
  FREE (&MuttMessageWindow);
#ifdef USE_SIDEBAR
  FREE (&MuttSidebarWindow);
#endif
}

void mutt_reflow_windows (void)
{
  if (option (OPTNOCURSES))
    return;

  dprint (2, (debugfile, "In mutt_reflow_windows\n"));

  MuttStatusWindow->rows = 1;
  MuttStatusWindow->cols = COLS;
  MuttStatusWindow->row_offset = option (OPTSTATUSONTOP) ? 0 : LINES - 2;
  MuttStatusWindow->col_offset = 0;

  memcpy (MuttHelpWindow, MuttStatusWindow, sizeof (mutt_window_t));
  if (! option (OPTHELP))
    MuttHelpWindow->rows = 0;
  else
    MuttHelpWindow->row_offset = option (OPTSTATUSONTOP) ? LINES - 2 : 0;

  memcpy (MuttMessageWindow, MuttStatusWindow, sizeof (mutt_window_t));
  MuttMessageWindow->row_offset = LINES - 1;

  memcpy (MuttIndexWindow, MuttStatusWindow, sizeof (mutt_window_t));
  MuttIndexWindow->rows = MAX(LINES - MuttStatusWindow->rows -
			      MuttHelpWindow->rows - MuttMessageWindow->rows, 0);
  MuttIndexWindow->row_offset = option (OPTSTATUSONTOP) ? MuttStatusWindow->rows :
                                                          MuttHelpWindow->rows;

#ifdef USE_SIDEBAR
  if (option (OPTSIDEBAR))
  {
    memcpy (MuttSidebarWindow, MuttIndexWindow, sizeof (mutt_window_t));
    MuttSidebarWindow->cols = SidebarWidth;

    MuttIndexWindow->cols -= SidebarWidth;
    MuttIndexWindow->col_offset += SidebarWidth;
  }
#endif
}

int mutt_window_move (mutt_window_t *win, int row, int col)
{
  return move (win->row_offset + row, win->col_offset + col);
}

int mutt_window_mvaddch (mutt_window_t *win, int row, int col, const chtype ch)
{
  return mvaddch (win->row_offset + row, win->col_offset + col, ch);
}

int mutt_window_mvaddstr (mutt_window_t *win, int row, int col, const char *str)
{
  return mvaddstr (win->row_offset + row, win->col_offset + col, str);
}

#ifdef USE_SLANG_CURSES
static int vw_printw (SLcurses_Window_Type *win, const char *fmt, va_list ap)
{
  char buf[LONG_STRING];

  (void) SLvsnprintf (buf, sizeof (buf), (char *)fmt, ap);
  SLcurses_waddnstr (win, buf, -1);
  return 0;
}
#endif

int mutt_window_mvprintw (mutt_window_t *win, int row, int col, const char *fmt, ...)
{
  va_list ap;
  int rv;

  if ((rv = mutt_window_move (win, row, col) != ERR))
  {
    va_start (ap, fmt);
    rv = vw_printw (stdscr, fmt, ap);
    va_end (ap);
  }

  return rv;
}

/* Assumes the cursor has already been positioned within the
 * window.
 */
void mutt_window_clrtoeol (mutt_window_t *win)
{
  int row, col, curcol;

  if (win->col_offset + win->cols == COLS)
    clrtoeol ();
  else
  {
    getyx (stdscr, row, col);
    curcol = col;
    while (curcol < win->col_offset + win->cols)
    {
      addch (' ');
      curcol++;
    }
    move (row, col);
  }
}

void mutt_window_clearline (mutt_window_t *win, int row)
{
  mutt_window_move (win, row, 0);
  mutt_window_clrtoeol (win);
}

/* Assumes the current position is inside the window.
 * Otherwise it will happily return negative or values outside
 * the window boundaries
 */
void mutt_window_getyx (mutt_window_t *win, int *y, int *x)
{
  int row, col;

  getyx (stdscr, row, col);
  if (y)
    *y = row - win->row_offset;
  if (x)
    *x = col - win->col_offset;
}


void mutt_show_error (void)
{
  if (option (OPTKEEPQUIET))
    return;
  
  SETCOLOR (option (OPTMSGERR) ? MT_COLOR_ERROR : MT_COLOR_MESSAGE);
  mutt_window_mvaddstr (MuttMessageWindow, 0, 0, Errorbuf);
  NORMAL_COLOR;
  mutt_window_clrtoeol(MuttMessageWindow);
}

void mutt_endwin (const char *msg)
{
  int e = errno;

  if (!option (OPTNOCURSES))
  {
    /* at least in some situations (screen + xterm under SuSE11/12) endwin()
     * doesn't properly flush the screen without an explicit call.
     */
    mutt_refresh();
    endwin ();
  }
  
  if (msg && *msg)
  {
    puts (msg);
    fflush (stdout);
  }

  errno = e;
}

void mutt_perror (const char *s)
{
  char *p = strerror (errno);

  dprint (1, (debugfile, "%s: %s (errno = %d)\n", s, 
      p ? p : "unknown error", errno));
  mutt_error ("%s: %s (errno = %d)", s, p ? p : _("unknown error"), errno);
}

int mutt_any_key_to_continue (const char *s)
{
  struct termios t;
  struct termios old;
  int f, ch;

  f = open ("/dev/tty", O_RDONLY);
  tcgetattr (f, &t);
  memcpy ((void *)&old, (void *)&t, sizeof(struct termios)); /* save original state */
  t.c_lflag &= ~(ICANON | ECHO);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  tcsetattr (f, TCSADRAIN, &t);
  fflush (stdout);
  if (s)
    fputs (s, stdout);
  else
    fputs (_("Press any key to continue..."), stdout);
  fflush (stdout);
  ch = fgetc (stdin);
  fflush (stdin);
  tcsetattr (f, TCSADRAIN, &old);
  close (f);
  fputs ("\r\n", stdout);
  mutt_clear_error ();
  return (ch);
}

int mutt_do_pager (const char *banner,
		   const char *tempfile,
		   int do_color,
		   pager_t *info)
{
  int rc;
  
  if (!Pager || mutt_strcmp (Pager, "builtin") == 0)
    rc = mutt_pager (banner, tempfile, do_color, info);
  else
  {
    char cmd[STRING];
    
    mutt_endwin (NULL);
    mutt_expand_file_fmt (cmd, sizeof(cmd), Pager, tempfile);
    if (mutt_system (cmd) == -1)
    {
      mutt_error (_("Error running \"%s\"!"), cmd);
      rc = -1;
    }
    else
      rc = 0;
    mutt_unlink (tempfile);
  }

  return rc;
}

int _mutt_enter_fname (const char *prompt, char *buf, size_t blen, int *redraw, int buffy, int multiple, char ***files, int *numfiles)
{
  event_t ch;

  SETCOLOR (MT_COLOR_PROMPT);
  mutt_window_mvaddstr (MuttMessageWindow, 0, 0, (char *) prompt);
  addstr (_(" ('?' for list): "));
  NORMAL_COLOR;
  if (buf[0])
    addstr (buf);
  mutt_window_clrtoeol (MuttMessageWindow);
  mutt_refresh ();

  ch = mutt_getch();
  if (ch.ch < 0)
  {
    mutt_window_clearline (MuttMessageWindow, 0);
    return (-1);
  }
  else if (ch.ch == '?')
  {
    mutt_refresh ();
    buf[0] = 0;
    _mutt_select_file (buf, blen, MUTT_SEL_FOLDER | (multiple ? MUTT_SEL_MULTI : 0), 
		       files, numfiles);
    *redraw = REDRAW_FULL;
  }
  else
  {
    char *pc = safe_malloc (mutt_strlen (prompt) + 3);

    sprintf (pc, "%s: ", prompt);	/* __SPRINTF_CHECKED__ */
    mutt_unget_event (ch.op ? 0 : ch.ch, ch.op ? ch.op : 0);
    if (_mutt_get_field (pc, buf, blen, (buffy ? MUTT_EFILE : MUTT_FILE) | MUTT_CLEAR, multiple, files, numfiles)
	!= 0)
      buf[0] = 0;
    MAYBE_REDRAW (*redraw);
    FREE (&pc);
  }

  return 0;
}

void mutt_unget_event (int ch, int op)
{
  event_t tmp;

  tmp.ch = ch;
  tmp.op = op;

  if (UngetCount >= UngetLen)
    safe_realloc (&UngetKeyEvents, (UngetLen += 16) * sizeof(event_t));

  UngetKeyEvents[UngetCount++] = tmp;
}

void mutt_unget_string (char *s)
{
  char *p = s + mutt_strlen (s) - 1;

  while (p >= s)
  {
    mutt_unget_event ((unsigned char)*p--, 0);
  }
}

/*
 * Adds the ch/op to the macro buffer.
 * This should be used for macros, push, and exec commands only.
 */
void mutt_push_macro_event (int ch, int op)
{
  event_t tmp;

  tmp.ch = ch;
  tmp.op = op;

  if (MacroBufferCount >= MacroBufferLen)
    safe_realloc (&MacroEvents, (MacroBufferLen += 128) * sizeof(event_t));

  MacroEvents[MacroBufferCount++] = tmp;
}

void mutt_flush_macro_to_endcond (void)
{
  UngetCount = 0;
  while (MacroBufferCount > 0)
  {
    if (MacroEvents[--MacroBufferCount].op == OP_END_COND)
      return;
  }
}

void mutt_flushinp (void)
{
  UngetCount = 0;
  MacroBufferCount = 0;
  flushinp ();
}

#if (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
/* The argument can take 3 values:
 * -1: restore the value of the last call
 *  0: make the cursor invisible
 *  1: make the cursor visible
 */
void mutt_curs_set (int cursor)
{
  static int SavedCursor = 1;
  
  if (cursor < 0)
    cursor = SavedCursor;
  else
    SavedCursor = cursor;
  
  if (curs_set (cursor) == ERR) {
    if (cursor == 1)	/* cnorm */
      curs_set (2);	/* cvvis */
  }
}
#endif

int mutt_multi_choice (char *prompt, char *letters)
{
  event_t ch;
  int choice;
  char *p;

  SETCOLOR (MT_COLOR_PROMPT);
  mutt_window_mvaddstr (MuttMessageWindow, 0, 0, prompt);
  NORMAL_COLOR;
  mutt_window_clrtoeol (MuttMessageWindow);
  FOREVER
  {
    mutt_refresh ();
    ch  = mutt_getch ();
    /* (ch.ch == 0) is technically possible.  Treat the same as < 0 (abort) */
    if (ch.ch <= 0 || CI_is_return (ch.ch))
    {
      choice = -1;
      break;
    }
    else
    {
      p = strchr (letters, ch.ch);
      if (p)
      {
	choice = p - letters + 1;
	break;
      }
      else if (ch.ch <= '9' && ch.ch > '0')
      {
	choice = ch.ch - '0';
	if (choice <= mutt_strlen (letters))
	  break;
      }
    }
    BEEP ();
  }
  mutt_window_clearline (MuttMessageWindow, 0);
  mutt_refresh ();
  return choice;
}

/*
 * addwch would be provided by an up-to-date curses library
 */

int mutt_addwch (wchar_t wc)
{
  char buf[MB_LEN_MAX*2];
  mbstate_t mbstate;
  size_t n1, n2;

  memset (&mbstate, 0, sizeof (mbstate));
  if ((n1 = wcrtomb (buf, wc, &mbstate)) == (size_t)(-1) ||
      (n2 = wcrtomb (buf + n1, 0, &mbstate)) == (size_t)(-1))
    return -1; /* ERR */
  else
    return addstr (buf);
}


/*
 * This formats a string, a bit like
 * snprintf (dest, destlen, "%-*.*s", min_width, max_width, s),
 * except that the widths refer to the number of character cells
 * when printed.
 */

void mutt_format_string (char *dest, size_t destlen,
			 int min_width, int max_width,
			 int justify, char m_pad_char,
			 const char *s, size_t n,
			 int arboreal)
{
  char *p;
  wchar_t wc;
  int w;
  size_t k, k2;
  char scratch[MB_LEN_MAX];
  mbstate_t mbstate1, mbstate2;

  memset(&mbstate1, 0, sizeof (mbstate1));
  memset(&mbstate2, 0, sizeof (mbstate2));
  --destlen;
  p = dest;
  for (; n && (k = mbrtowc (&wc, s, n, &mbstate1)); s += k, n -= k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1) && errno == EILSEQ)
	memset (&mbstate1, 0, sizeof (mbstate1));

      k = (k == (size_t)(-1)) ? 1 : n;
      wc = replacement_char ();
    }
    if (arboreal && wc < MUTT_TREE_MAX)
      w = 1; /* hack */
    else
    {
#ifdef HAVE_ISWBLANK
      if (iswblank (wc))
	wc = ' ';
      else
#endif
      if (!IsWPrint (wc))
	wc = '?';
      w = wcwidth (wc);
    }
    if (w >= 0)
    {
      if (w > max_width || (k2 = wcrtomb (scratch, wc, &mbstate2)) > destlen)
	break;
      min_width -= w;
      max_width -= w;
      strncpy (p, scratch, k2);
      p += k2;
      destlen -= k2;
    }
  }
  w = (int)destlen < min_width ? destlen : min_width;
  if (w <= 0)
    *p = '\0';
  else if (justify == FMT_RIGHT)	/* right justify */
  {
    p[w] = '\0';
    while (--p >= dest)
      p[w] = *p;
    while (--w >= 0)
      dest[w] = m_pad_char;
  }
  else if (justify == FMT_CENTER)	/* center */
  {
    char *savedp = p;
    int half = (w+1) / 2; /* half of cushion space */

    p[w] = '\0';

    /* move str to center of buffer */
    while (--p >= dest)
      p[half] = *p;

    /* fill rhs */
    p = savedp + half;
    while (--w >= half)
      *p++ = m_pad_char;

    /* fill lhs */
    while (half--)
      dest[half] = m_pad_char;
  }
  else					/* left justify */
  {
    while (--w >= 0)
      *p++ = m_pad_char;
    *p = '\0';
  }
}

/*
 * This formats a string rather like
 *   snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
 *   snprintf (dest, destlen, fmt, s);
 * except that the numbers in the conversion specification refer to
 * the number of character cells when printed.
 */

static void mutt_format_s_x (char *dest,
			     size_t destlen,
			     const char *prefix,
			     const char *s,
			     int arboreal)
{
  int justify = FMT_RIGHT;
  char *p;
  int min_width;
  int max_width = INT_MAX;

  if (*prefix == '-')
    ++prefix, justify = FMT_LEFT;
  else if (*prefix == '=')
    ++prefix, justify = FMT_CENTER;
  min_width = strtol (prefix, &p, 10);
  if (*p == '.')
  {
    prefix = p + 1;
    max_width = strtol (prefix, &p, 10);
    if (p <= prefix)
      max_width = INT_MAX;
  }

  mutt_format_string (dest, destlen, min_width, max_width,
		      justify, ' ', s, mutt_strlen (s), arboreal);
}

void mutt_format_s (char *dest,
		    size_t destlen,
		    const char *prefix,
		    const char *s)
{
  mutt_format_s_x (dest, destlen, prefix, s, 0);
}

void mutt_format_s_tree (char *dest,
			 size_t destlen,
			 const char *prefix,
			 const char *s)
{
  mutt_format_s_x (dest, destlen, prefix, s, 1);
}

/*
 * mutt_paddstr (n, s) is almost equivalent to
 * mutt_format_string (bigbuf, big, n, n, FMT_LEFT, ' ', s, big, 0), addstr (bigbuf)
 */

void mutt_paddstr (int n, const char *s)
{
  wchar_t wc;
  int w;
  size_t k;
  size_t len = mutt_strlen (s);
  mbstate_t mbstate;

  memset (&mbstate, 0, sizeof (mbstate));
  for (; len && (k = mbrtowc (&wc, s, len, &mbstate)); s += k, len -= k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t) (-1))
	memset (&mbstate, 0, sizeof (mbstate));
      k = (k == (size_t)(-1)) ? 1 : len;
      wc = replacement_char ();
    }
    if (!IsWPrint (wc))
      wc = '?';
    w = wcwidth (wc);
    if (w >= 0)
    {
      if (w > n)
	break;
      addnstr ((char *)s, k);
      n -= w;
    }
  }
  while (n-- > 0)
    addch (' ');
}

/* See how many bytes to copy from string so its at most maxlen bytes
 * long and maxwid columns wide */
size_t mutt_wstr_trunc (const char *src, size_t maxlen, size_t maxwid, size_t *width)
{
  wchar_t wc;
  size_t n, w = 0, l = 0, cl;
  int cw;
  mbstate_t mbstate;

  if (!src)
    goto out;

  n = mutt_strlen (src);

  memset (&mbstate, 0, sizeof (mbstate));
  for (w = 0; n && (cl = mbrtowc (&wc, src, n, &mbstate)); src += cl, n -= cl)
  {
    if (cl == (size_t)(-1) || cl == (size_t)(-2))
    {
      if (cl == (size_t)(-1))
        memset (&mbstate, 0, sizeof (mbstate));
      cl = (cl == (size_t)(-1)) ? 1 : n;
      wc = replacement_char ();
    }
    cw = wcwidth (wc);
    /* hack because MUTT_TREE symbols aren't turned into characters
     * until rendered by print_enriched_string (#3364) */
    if (cw < 0 && cl == 1 && src[0] && src[0] < MUTT_TREE_MAX)
      cw = 1;
    else if (cw < 0)
      cw = 0;			/* unprintable wchar */
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

/*
 * returns the number of bytes the first (multibyte) character
 * of input consumes:
 * 	< 0 ... conversion error
 * 	= 0 ... end of input
 * 	> 0 ... length (bytes)
 */
int mutt_charlen (const char *s, int *width)
{
  wchar_t wc;
  mbstate_t mbstate;
  size_t k, n;

  if (!s || !*s)
    return 0;

  n = mutt_strlen (s);
  memset (&mbstate, 0, sizeof (mbstate));
  k = mbrtowc (&wc, s, n, &mbstate);
  if (width)
    *width = wcwidth (wc);
  return (k == (size_t)(-1) || k == (size_t)(-2)) ? -1 : k;
}

/*
 * mutt_strwidth is like mutt_strlen except that it returns the width
 * referring to the number of character cells.
 */

int mutt_strwidth (const char *s)
{
  wchar_t wc;
  int w;
  size_t k, n;
  mbstate_t mbstate;

  if (!s) return 0;

  n = mutt_strlen (s);

  memset (&mbstate, 0, sizeof (mbstate));
  for (w=0; n && (k = mbrtowc (&wc, s, n, &mbstate)); s += k, n -= k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      if (k == (size_t)(-1))
        memset (&mbstate, 0, sizeof (mbstate));
      k = (k == (size_t)(-1)) ? 1 : n;
      wc = replacement_char ();
    }
    if (!IsWPrint (wc))
      wc = '?';
    w += wcwidth (wc);
  }
  return w;
}
