/*
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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

#ifdef HAVE_LANGINFO_YESEXPR
#include <langinfo.h>
#endif

/* not possible to unget more than one char under some curses libs, and it
 * is impossible to unget function keys in SLang, so roll our own input
 * buffering routines.
 */
size_t UngetCount = 0;
static size_t UngetBufLen = 0;
static event_t *KeyEvent;

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
   optmized by not doing it at all if DISPLAY is set as this might
   indicate that a GUI based pinentry was used.  Having an option to
   customize this is of course the Mutt way.  */
void mutt_need_hard_redraw (void)
{
  if (!getenv ("DISPLAY"))
  {
    keypad (stdscr, TRUE);
    clearok (stdscr, TRUE);
    set_option (OPTNEEDREDRAW);
  }
}

event_t mutt_getch (void)
{
  int ch;
  event_t err = {-1, OP_NULL }, ret;

  if (!option(OPTUNBUFFEREDINPUT) && UngetCount)
    return (KeyEvent[--UngetCount]);

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
    mutt_query_exit ();

  if(ch == ERR)
    return err;
  
  if ((ch & 0x80) && option (OPTMETAKEY))
  {
    /* send ALT-x as ESC-x */
    ch &= ~0x80;
    mutt_ungetch (ch, 0);
    ret.ch = '\033';
    ret.op = 0;
    return ret;
  }

  ret.ch = ch;
  ret.op = 0;
  return (ch == ctrl ('G') ? err : ret);
}

int _mutt_get_field (/* const */ char *field, char *buf, size_t buflen, int complete, int multiple, char ***files, int *numfiles)
{
  int ret;
  int x, y;

  ENTER_STATE *es = mutt_new_enter_state();
  
  do
  {
    CLEARLINE (LINES-1);
    addstr (field);
    mutt_refresh ();
    getyx (stdscr, y, x);
    ret = _mutt_enter_string (buf, buflen, y, x, complete, multiple, files, numfiles, es);
  }
  while (ret == 1);
  CLEARLINE (LINES-1);
  mutt_free_enter_state (&es);
  
  return (ret);
}

int mutt_get_field_unbuffered (char *msg, char *buf, size_t buflen, int flags)
{
  int rc;

  set_option (OPTUNBUFFEREDINPUT);
  rc = mutt_get_field (msg, buf, buflen, flags);
  unset_option (OPTUNBUFFEREDINPUT);

  return (rc);
}

void mutt_clear_error (void)
{
  Errorbuf[0] = 0;
  if (!option(OPTNOCURSES))
    CLEARLINE (LINES-1);
}

void mutt_edit_file (const char *editor, const char *data)
{
  char cmd[LONG_STRING];
  
  mutt_endwin (NULL);
  mutt_expand_file_fmt (cmd, sizeof (cmd), editor, data);
  if (mutt_system (cmd) == -1)
    mutt_error (_("Error running \"%s\"!"), cmd);
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

#ifdef HAVE_LANGINFO_YESEXPR
  char *expr;
  regex_t reyes;
  regex_t reno;
  int reyes_ok;
  int reno_ok;
  char answer[2];

  answer[1] = 0;
  
  reyes_ok = (expr = nl_langinfo (YESEXPR)) && expr[0] == '^' &&
	     !regcomp (&reyes, expr, REG_NOSUB|REG_EXTENDED);
  reno_ok = (expr = nl_langinfo (NOEXPR)) && expr[0] == '^' &&
            !regcomp (&reno, expr, REG_NOSUB|REG_EXTENDED);
#endif

  CLEARLINE(LINES-1);

  /*
   * In order to prevent the default answer to the question to wrapped
   * around the screen in the even the question is wider than the screen,
   * ensure there is enough room for the answer and truncate the question
   * to fit.
   */
  answer_string = safe_malloc (COLS + 1);
  snprintf (answer_string, COLS + 1, " ([%s]/%s): ", def == M_YES ? yes : no, def == M_YES ? no : yes);
  answer_string_len = strlen (answer_string);
  printw ("%.*s%s", COLS - answer_string_len, msg, answer_string);
  FREE (&answer_string);

  FOREVER
  {
    mutt_refresh ();
    ch = mutt_getch ();
    if (CI_is_return (ch.ch))
      break;
    if (ch.ch == -1)
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
      def = M_YES;
      break;
    }
    else if (
#ifdef HAVE_LANGINFO_YESEXPR
	     reno_ok ?
	     (regexec (& reno, answer, 0, 0, 0) == 0) :
#endif
	     (tolower (ch.ch) == 'n'))
    {
      def = M_NO;
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
    addstr ((char *) (def == M_YES ? yes : no));
    mutt_refresh ();
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
  if (mutt_yesorno (_("Exit Mutt?"), M_YES) == M_YES)
  {
    endwin ();
    exit (1);
  }
  mutt_clear_error();
  mutt_curs_set (-1);
  SigInt = 0;
}

void mutt_curses_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vsnprintf (Errorbuf, sizeof (Errorbuf), fmt, ap);
  va_end (ap);
  
  dprint (1, (debugfile, "%s\n", Errorbuf));
  mutt_format_string (Errorbuf, sizeof (Errorbuf),
		      0, COLS-2, 0, 0, Errorbuf, sizeof (Errorbuf), 0);

  if (!option (OPTKEEPQUIET))
  {
    BEEP ();
    SETCOLOR (MT_COLOR_ERROR);
    mvaddstr (LINES-1, 0, Errorbuf);
    clrtoeol ();
    SETCOLOR (MT_COLOR_NORMAL);
    mutt_refresh ();
  }

  set_option (OPTMSGERR);
}

void mutt_curses_message (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vsnprintf (Errorbuf, sizeof (Errorbuf), fmt, ap);
  va_end (ap);

  mutt_format_string (Errorbuf, sizeof (Errorbuf),
		      0, COLS-2, 0, 0, Errorbuf, sizeof (Errorbuf), 0);

  if (!option (OPTKEEPQUIET))
  {
    SETCOLOR (MT_COLOR_MESSAGE);
    mvaddstr (LINES - 1, 0, Errorbuf);
    clrtoeol ();
    SETCOLOR (MT_COLOR_NORMAL);
    mutt_refresh ();
  }

  unset_option (OPTMSGERR);
}

void mutt_progress_init (progress_t* progress, const char *msg,
			 unsigned short flags, unsigned short inc,
			 long size)
{
  if (!progress)
    return;
  memset (progress, 0, sizeof (progress_t));
  progress->inc = inc;
  progress->flags = flags;
  progress->msg = msg;
  progress->size = size;
  mutt_progress_update (progress, 0);
}

void mutt_progress_update (progress_t* progress, long pos)
{
  char posstr[SHORT_STRING];
  short update = 0;

  if (!pos)
  {
    if (!progress->inc)
      mutt_message (progress->msg);
    else
    {
      if (progress->size)
      {
	if (progress->flags & M_PROGRESS_SIZE)
	  mutt_pretty_size (progress->sizestr, sizeof (progress->sizestr), progress->size);
	else
	  snprintf (progress->sizestr, sizeof (progress->sizestr), "%ld", progress->size);
      }
      progress->pos = 0;
    }
  }

  if (!progress->inc)
    return;

  if (progress->flags & M_PROGRESS_SIZE)
  {
    if (pos >= progress->pos + (progress->inc << 10))
    {
      pos = pos / (progress->inc << 10) * (progress->inc << 10);
      mutt_pretty_size (posstr, sizeof (posstr), pos);
      update = 1;
    }
  }
  else if (pos >= progress->pos + progress->inc)
  {
    snprintf (posstr, sizeof (posstr), "%ld", pos);
    update = 1;
  }

  if (update)
  {
    progress->pos = pos;
    if (progress->size)
      mutt_message ("%s %s/%s", progress->msg, posstr, progress->sizestr);
    else
      mutt_message ("%s %s", progress->msg, posstr);
  }

  if (pos >= progress->size)
    mutt_clear_error ();
}

void mutt_show_error (void)
{
  if (option (OPTKEEPQUIET))
    return;
  
  SETCOLOR (option (OPTMSGERR) ? MT_COLOR_ERROR : MT_COLOR_MESSAGE);
  CLEARLINE (LINES-1);
  addstr (Errorbuf);
  SETCOLOR (MT_COLOR_NORMAL);
}

void mutt_endwin (const char *msg)
{
  if (!option (OPTNOCURSES))
  {
    CLEARLINE (LINES - 1);
    
    attrset (A_NORMAL);
    mutt_refresh ();
    endwin ();
  }
  
  if (msg && *msg)
  {
    puts (msg);
    fflush (stdout);
  }
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

  mvaddstr (LINES-1, 0, (char *) prompt);
  addstr (_(" ('?' for list): "));
  if (buf[0])
    addstr (buf);
  clrtoeol ();
  mutt_refresh ();

  ch = mutt_getch();
  if (ch.ch == -1)
  {
    CLEARLINE (LINES-1);
    return (-1);
  }
  else if (ch.ch == '?')
  {
    mutt_refresh ();
    buf[0] = 0;
    _mutt_select_file (buf, blen, M_SEL_FOLDER | (multiple ? M_SEL_MULTI : 0), 
		       files, numfiles);
    *redraw = REDRAW_FULL;
  }
  else
  {
    char *pc = safe_malloc (mutt_strlen (prompt) + 3);

    sprintf (pc, "%s: ", prompt);	/* __SPRINTF_CHECKED__ */
    mutt_ungetch (ch.op ? 0 : ch.ch, ch.op ? ch.op : 0);
    if (_mutt_get_field (pc, buf, blen, (buffy ? M_EFILE : M_FILE) | M_CLEAR, multiple, files, numfiles)
	!= 0)
      buf[0] = 0;
    MAYBE_REDRAW (*redraw);
    FREE (&pc);
  }

  return 0;
}

void mutt_ungetch (int ch, int op)
{
  event_t tmp;

  tmp.ch = ch;
  tmp.op = op;

  if (UngetCount >= UngetBufLen)
    safe_realloc (&KeyEvent, (UngetBufLen += 128) * sizeof(event_t));

  KeyEvent[UngetCount++] = tmp;
}

void mutt_flushinp (void)
{
  UngetCount = 0;
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

  mvaddstr (LINES - 1, 0, prompt);
  clrtoeol ();
  FOREVER
  {
    mutt_refresh ();
    ch  = mutt_getch ();
    if (ch.ch == -1 || CI_is_return (ch.ch))
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
  CLEARLINE (LINES - 1);
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
			 int right_justify, char m_pad_char,
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
      k = (k == (size_t)(-1)) ? 1 : n;
      wc = replacement_char ();
    }
    if (arboreal && wc < M_TREE_MAX)
      w = 1; /* hack */
    else
    {
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
  else if (right_justify)
  {
    p[w] = '\0';
    while (--p >= dest)
      p[w] = *p;
    while (--w >= 0)
      dest[w] = m_pad_char;
  }
  else
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
  int right_justify = 1;
  char *p;
  int min_width;
  int max_width = INT_MAX;

  if (*prefix == '-')
    ++prefix, right_justify = 0;
  min_width = strtol (prefix, &p, 10);
  if (*p == '.')
  {
    prefix = p + 1;
    max_width = strtol (prefix, &p, 10);
    if (p <= prefix)
      max_width = INT_MAX;
  }

  mutt_format_string (dest, destlen, min_width, max_width,
		      right_justify, ' ', s, mutt_strlen (s), arboreal);
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
 * mutt_format_string (bigbuf, big, n, n, 0, ' ', s, big, 0), addstr (bigbuf)
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

/*
 * mutt_strwidth is like mutt_strlen except that it returns the width
 * refering to the number of characters cells.
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
      k = (k == (size_t)(-1)) ? 1 : n;
      wc = replacement_char ();
    }
    if (!IsWPrint (wc))
      wc = '?';
    w += wcwidth (wc);
  }
  return w;
}
