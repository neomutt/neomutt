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

/**
 * @page gui_curs_lib Window drawing code
 *
 * GUI miscellaneous curses (window drawing) routines
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "curs_lib.h"
#include "browser/lib.h"
#include "color/lib.h"
#include "enter/lib.h"
#include "question/lib.h"
#include "keymap.h"
#include "msgwin.h"
#include "mutt_curses.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#ifdef HAVE_ISWBLANK
#include <wctype.h>
#endif
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

/* not possible to unget more than one char under some curses libs, so roll our
 * own input buffering routines.  */

ARRAY_HEAD(KeyEventArray, struct KeyEvent);

/* These are used for macros and exec/push commands.
 * They can be temporarily ignored by setting OptIgnoreMacroEvents */
static struct KeyEventArray MacroEvents = ARRAY_HEAD_INITIALIZER;

/* These are used in all other "normal" situations, and are not
 * ignored when setting OptIgnoreMacroEvents */
static struct KeyEventArray UngetKeyEvents = ARRAY_HEAD_INITIALIZER;

/**
 * array_pop - Remove an event from the array
 * @param a Array
 * @retval ptr Event
 */
static struct KeyEvent *array_pop(struct KeyEventArray *a)
{
  if (ARRAY_EMPTY(a))
  {
    return NULL;
  }

  struct KeyEvent *event = ARRAY_LAST(a);
  ARRAY_SHRINK(a, 1);
  return event;
}

/**
 * array_add - Add an event to the end of the array
 * @param a  Array
 * @param ch Character
 * @param op Operation, e.g. OP_DELETE
 */
static void array_add(struct KeyEventArray *a, int ch, int op)
{
  struct KeyEvent event = { .ch = ch, .op = op };
  ARRAY_ADD(a, event);
}

/**
 * array_to_endcond - Clear the array until an OP_END_COND
 * @param a Array
 */
static void array_to_endcond(struct KeyEventArray *a)
{
  while (!ARRAY_EMPTY(a))
  {
    if (array_pop(a)->op == OP_END_COND)
    {
      return;
    }
  }
}

int MuttGetchTimeout = -1;

/**
 * mutt_beep - Irritate the user
 * @param force If true, ignore the "$beep" config variable
 */
void mutt_beep(bool force)
{
  const bool c_beep = cs_subset_bool(NeoMutt->sub, "beep");
  if (force || c_beep)
    beep();
}

/**
 * mutt_refresh - Force a refresh of the screen
 */
void mutt_refresh(void)
{
  /* don't refresh when we are waiting for a child. */
  if (OptKeepQuiet)
    return;

  /* don't refresh in the middle of macros unless necessary */
  if (!ARRAY_EMPTY(&MacroEvents) && !OptForceRefresh && !OptIgnoreMacroEvents)
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
  // Forcibly switch to the alternate screen.
  // Using encryption can leave ncurses confused about which mode it's in.
  fputs("\033[?1049h", stdout);
  fflush(stdout);
  keypad(stdscr, true);
  clearok(stdscr, true);
  window_redraw(NULL);
}

/**
 * set_timeout - Set the getch() timeout
 * @param delay Timeout delay in ms
 * delay is just like for timeout() or poll(): the number of milliseconds
 * mutt_getch() should block for input.
 * * delay == 0 means mutt_getch() is non-blocking.
 * * delay < 0 means mutt_getch is blocking.
 */
static void set_timeout(int delay)
{
  MuttGetchTimeout = delay;
  timeout(delay);
}

/**
 * mutt_getch_timeout - Get an event with a timeout
 * @param delay Timeout delay in ms
 * @retval Same as mutt_get_ch
 *
 * delay is just like for timeout() or poll(): the number of milliseconds
 * mutt_getch() should block for input.
 * * delay == 0 means mutt_getch() is non-blocking.
 * * delay < 0 means mutt_getch is blocking.
 */
struct KeyEvent mutt_getch_timeout(int delay)
{
  set_timeout(delay);
  struct KeyEvent event = mutt_getch();
  set_timeout(-1);
  return event;
}

#ifdef USE_INOTIFY
/**
 * mutt_monitor_getch - Get a character and poll the filesystem monitor
 * @retval num Character pressed
 * @retval ERR Timeout
 */
static int mutt_monitor_getch(void)
{
  /* ncurses has its own internal buffer, so before we perform a poll,
   * we need to make sure there isn't a character waiting */
  timeout(0);
  int ch = getch();
  timeout(MuttGetchTimeout);
  if (ch == ERR)
  {
    if (mutt_monitor_poll() != 0)
      ch = ERR;
    else
      ch = getch();
  }
  return ch;
}
#endif /* USE_INOTIFY */

/**
 * mutt_getch - Read a character from the input buffer
 * @retval obj KeyEvent to process
 *
 * The priority for reading events is:
 * 1. UngetKeyEvents buffer
 * 2. MacroEvents buffer
 * 3. Keyboard
 *
 * This function can return:
 * - Error   `{ OP_ABORT,   OP_NULL }`
 * - Timeout `{ OP_TIMEOUT, OP_NULL }`
 */
struct KeyEvent mutt_getch(void)
{
  int ch;
  const struct KeyEvent err = { 0, OP_ABORT };
  const struct KeyEvent timeout = { 0, OP_TIMEOUT };

  struct KeyEvent *key = array_pop(&UngetKeyEvents);
  if (key)
  {
    return *key;
  }

  if (!OptIgnoreMacroEvents && (key = array_pop(&MacroEvents)))
  {
    return *key;
  }

  SigInt = false;

  mutt_sig_allow_interrupt(true);
#ifdef KEY_RESIZE
  /* ncurses 4.2 sends this when the screen is resized */
  ch = KEY_RESIZE;
  while (ch == KEY_RESIZE)
#endif
#ifdef USE_INOTIFY
    ch = mutt_monitor_getch();
#else
  ch = getch();
#endif
  mutt_sig_allow_interrupt(false);

  if (SigInt)
  {
    mutt_query_exit();
    return err;
  }

  /* either timeout, a sigwinch (if timeout is set), the terminal
   * has been lost, or curses was never initialized */
  if (ch == ERR)
  {
    if (!isatty(0))
    {
      mutt_exit(1);
    }

    return OptNoCurses ? err : timeout;
  }

  const bool c_meta_key = cs_subset_bool(NeoMutt->sub, "meta_key");
  if ((ch & 0x80) && c_meta_key)
  {
    /* send ALT-x as ESC-x */
    ch &= ~0x80;
    mutt_unget_ch(ch);
    return (struct KeyEvent){ .ch = '\033' /* Escape */, .op = OP_NULL };
  }

  if (ch == AbortKey)
    return err;

  return (struct KeyEvent){ .ch = ch, .op = OP_NULL };
}

/**
 * mutt_edit_file - Let the user edit a file
 * @param editor User's editor config
 * @param file   File to edit
 */
void mutt_edit_file(const char *editor, const char *file)
{
  struct Buffer *cmd = mutt_buffer_pool_get();

  mutt_endwin();
  mutt_buffer_file_expand_fmt_quote(cmd, editor, file);
  if (mutt_system(mutt_buffer_string(cmd)) != 0)
  {
    mutt_error(_("Error running \"%s\""), mutt_buffer_string(cmd));
  }
  /* the terminal may have been resized while the editor owned it */
  mutt_resize_screen();
  keypad(stdscr, true);
  clearok(stdscr, true);

  mutt_buffer_pool_release(&cmd);
}

/**
 * mutt_query_exit - Ask the user if they want to leave NeoMutt
 *
 * This function is called when the user presses the abort key.
 */
void mutt_query_exit(void)
{
  mutt_flushinp();
  enum MuttCursorState cursor = mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
  const short c_timeout = cs_subset_number(NeoMutt->sub, "timeout");
  if (c_timeout)
    set_timeout(-1); /* restore blocking operation */
  if (mutt_yesorno(_("Exit NeoMutt without saving?"), MUTT_YES) == MUTT_YES)
  {
    mutt_exit(0); /* This call never returns */
  }
  mutt_clear_error();
  mutt_curses_set_cursor(cursor);
  SigInt = false;
}

/**
 * mutt_endwin - Shutdown curses
 */
void mutt_endwin(void)
{
  if (OptNoCurses)
    return;

  int e = errno;

  /* at least in some situations (screen + xterm under SuSE11/12) endwin()
   * doesn't properly flush the screen without an explicit call.  */
  mutt_refresh();
  endwin();

  errno = e;
}

/**
 * mutt_perror_debug - Show the user an 'errno' message
 * @param s Additional text to show
 */
void mutt_perror_debug(const char *s)
{
  char *p = strerror(errno);

  mutt_debug(LL_DEBUG1, "%s: %s (errno = %d)\n", s, p ? p : "unknown error", errno);
  mutt_error("%s: %s (errno = %d)", s, p ? p : _("unknown error"), errno);
}

/**
 * mutt_any_key_to_continue - Prompt the user to 'press any key' and wait
 * @param s Message prompt
 * @retval num Key pressed
 * @retval EOF Error, or prompt aborted
 */
int mutt_any_key_to_continue(const char *s)
{
  struct termios term;
  struct termios old;

  int fd = open("/dev/tty", O_RDONLY);
  if (fd < 0)
    return EOF;

  tcgetattr(fd, &old); // Save the current tty settings

  term = old;
  term.c_lflag &= ~(ICANON | ECHO); // Canonical (not line-buffered), don't echo the characters
  term.c_cc[VMIN] = 1;    // Wait for at least one character
  term.c_cc[VTIME] = 255; // Wait for 25.5s
  tcsetattr(fd, TCSANOW, &term);

  if (s)
    fputs(s, stdout);
  else
    fputs(_("Press any key to continue..."), stdout);
  fflush(stdout);

  char ch = '\0';
  // Wait for a character.  This might timeout, so loop.
  while (read(fd, &ch, 1) == 0)
    ; // do nothing

  // Change the tty settings to be non-blocking
  term.c_cc[VMIN] = 0;  // Returning with zero characters is acceptable
  term.c_cc[VTIME] = 0; // Don't wait
  tcsetattr(fd, TCSANOW, &term);

  char buf[64] = { 0 };
  while (read(fd, buf, sizeof(buf)) > 0)
    ; // Mop up any remaining chars

  tcsetattr(fd, TCSANOW, &old); // Restore the previous tty settings
  close(fd);

  fputs("\r\n", stdout);
  mutt_clear_error();
  return (ch >= 0) ? ch : EOF;
}

/**
 * mutt_buffer_enter_fname - Ask the user to select a file
 * @param[in]  prompt   Prompt
 * @param[in]  fname    Buffer for the result
 * @param[in]  mailbox  If true, select mailboxes
 * @param[in]  multiple Allow multiple selections
 * @param[in]  m        Mailbox
 * @param[out] files    List of files selected
 * @param[out] numfiles Number of files selected
 * @param[in]  flags    Flags, see #SelectFileFlags
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_buffer_enter_fname(const char *prompt, struct Buffer *fname,
                            bool mailbox, struct Mailbox *m, bool multiple,
                            char ***files, int *numfiles, SelectFileFlags flags)
{
  struct MuttWindow *win = msgwin_get_window();
  if (!win)
    return -1;

  struct KeyEvent ch = { OP_NULL, OP_NULL };
  struct MuttWindow *old_focus = window_set_focus(win);

  mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROMPT);
  mutt_window_mvaddstr(win, 0, 0, prompt);
  mutt_window_addstr(win, _(" ('?' for list): "));
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  if (!mutt_buffer_is_empty(fname))
    mutt_window_addstr(win, mutt_buffer_string(fname));
  mutt_window_clrtoeol(win);
  mutt_refresh();

  enum MuttCursorState cursor = mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
  do
  {
    ch = mutt_getch();
  } while (ch.op == OP_TIMEOUT);
  mutt_curses_set_cursor(cursor);

  mutt_window_move(win, 0, 0);
  mutt_window_clrtoeol(win);
  mutt_refresh();
  window_set_focus(old_focus);

  if (ch.ch < 0)
  {
    return -1;
  }
  else if (ch.ch == '?')
  {
    mutt_buffer_reset(fname);

    if (flags == MUTT_SEL_NO_FLAGS)
      flags = MUTT_SEL_FOLDER;
    if (multiple)
      flags |= MUTT_SEL_MULTI;
    if (mailbox)
      flags |= MUTT_SEL_MAILBOX;
    mutt_buffer_select_file(fname, flags, m, files, numfiles);
  }
  else
  {
    char *pc = mutt_mem_malloc(mutt_str_len(prompt) + 3);

    sprintf(pc, "%s: ", prompt);
    if (ch.op == OP_NULL)
      mutt_unget_ch(ch.ch);
    else
      mutt_unget_op(ch.op);

    mutt_buffer_alloc(fname, 1024);
    if (mutt_buffer_get_field(pc, fname, (mailbox ? MUTT_COMP_FILE_MBOX : MUTT_COMP_FILE) | MUTT_COMP_CLEAR,
                              multiple, m, files, numfiles) != 0)
    {
      mutt_buffer_reset(fname);
    }
    FREE(&pc);
  }

  return 0;
}

/**
 * mutt_unget_ch - Return a keystroke to the input buffer
 * @param ch Key press
 *
 * This puts events into the `UngetKeyEvents` buffer
 */
void mutt_unget_ch(int ch)
{
  array_add(&UngetKeyEvents, ch, OP_NULL);
}

/**
 * mutt_unget_op - Return an operation to the input buffer
 * @param op Operation, e.g. OP_DELETE
 *
 * This puts events into the `UngetKeyEvents` buffer
 */
void mutt_unget_op(int op)
{
  array_add(&UngetKeyEvents, 0, op);
}

/**
 * mutt_unget_string - Return a string to the input buffer
 * @param s String to return
 *
 * This puts events into the `UngetKeyEvents` buffer
 */
void mutt_unget_string(const char *s)
{
  const char *p = s + mutt_str_len(s) - 1;

  while (p >= s)
  {
    mutt_unget_ch((unsigned char) *p--);
  }
}

/**
 * mutt_push_macro_event - Add the character/operation to the macro buffer
 * @param ch Character to add
 * @param op Operation to add
 *
 * Adds the ch/op to the macro buffer.
 * This should be used for macros, push, and exec commands only.
 */
void mutt_push_macro_event(int ch, int op)
{
  array_add(&MacroEvents, ch, op);
}

/**
 * mutt_flush_macro_to_endcond - Drop a macro from the input buffer
 *
 * All the macro text is deleted until an OP_END_COND command,
 * or the buffer is empty.
 */
void mutt_flush_macro_to_endcond(void)
{
  array_to_endcond(&MacroEvents);
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
  array_to_endcond(&UngetKeyEvents);
}

/**
 * mutt_flushinp - Empty all the keyboard buffers
 */
void mutt_flushinp(void)
{
  ARRAY_SHRINK(&UngetKeyEvents, ARRAY_SIZE(&UngetKeyEvents));
  ARRAY_SHRINK(&MacroEvents, ARRAY_SIZE(&MacroEvents));
  flushinp();
}

/**
 * mutt_addwch - Addwch would be provided by an up-to-date curses library
 * @param win Window
 * @param wc  Wide char to display
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_addwch(struct MuttWindow *win, wchar_t wc)
{
  char buf[MB_LEN_MAX * 2];
  mbstate_t mbstate = { 0 };
  size_t n1, n2;

  if (((n1 = wcrtomb(buf, wc, &mbstate)) == (size_t) (-1)) ||
      ((n2 = wcrtomb(buf + n1, 0, &mbstate)) == (size_t) (-1)))
  {
    return -1; /* ERR */
  }
  else
  {
    return mutt_window_addstr(win, buf);
  }
}

/**
 * mutt_simple_format - Format a string, like snprintf()
 * @param[out] buf       Buffer in which to save string
 * @param[in]  buflen    Buffer length
 * @param[in]  min_width Minimum width
 * @param[in]  max_width Maximum width
 * @param[in]  justify   Justification, e.g. #JUSTIFY_RIGHT
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
                        enum FormatJustify justify, char pad_char,
                        const char *s, size_t n, bool arboreal)
{
  wchar_t wc = 0;
  int w;
  size_t k;
  char scratch[MB_LEN_MAX] = { 0 };
  mbstate_t mbstate1 = { 0 };
  mbstate_t mbstate2 = { 0 };
  bool escaped = false;

  buflen--;
  char *p = buf;
  for (; n && (k = mbrtowc(&wc, s, n, &mbstate1)); s += k, n -= k)
  {
    if ((k == (size_t) (-1)) || (k == (size_t) (-2)))
    {
      if ((k == (size_t) (-1)) && (errno == EILSEQ))
        memset(&mbstate1, 0, sizeof(mbstate1));

      k = (k == (size_t) (-1)) ? 1 : n;
      wc = ReplacementChar;
    }
    if (escaped)
    {
      escaped = false;
      w = 0;
    }
    else if (arboreal && (wc == MUTT_SPECIAL_INDEX))
    {
      escaped = true;
      w = 0;
    }
    else if (arboreal && (wc < MUTT_TREE_MAX))
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
      if (w > max_width)
        continue;
      size_t k2 = wcrtomb(scratch, wc, &mbstate2);
      if ((k2 == (size_t) -1) || (k2 > buflen))
        continue;

      min_width -= w;
      max_width -= w;
      strncpy(p, scratch, k2);
      p += k2;
      buflen -= k2;
    }
  }
  w = ((int) buflen < min_width) ? buflen : min_width;
  if (w <= 0)
    *p = '\0';
  else if (justify == JUSTIFY_RIGHT) /* right justify */
  {
    p[w] = '\0';
    while (--p >= buf)
      p[w] = *p;
    while (--w >= 0)
      buf[w] = pad_char;
  }
  else if (justify == JUSTIFY_CENTER) /* center */
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
 * mutt_format_s_x - Format a string like snprintf()
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  s        String to format
 * @param[in]  arboreal  If true, string contains graphical tree characters
 *
 * This formats a string rather like:
 * - snprintf(fmt, sizeof(fmt), "%%%ss", prec);
 * - snprintf(buf, buflen, fmt, s);
 * except that the numbers in the conversion specification refer to
 * the number of character cells when printed.
 */
void mutt_format_s_x(char *buf, size_t buflen, const char *prec, const char *s, bool arboreal)
{
  enum FormatJustify justify = JUSTIFY_RIGHT;
  char *p = NULL;
  int min_width;
  int max_width = INT_MAX;

  if (*prec == '-')
  {
    prec++;
    justify = JUSTIFY_LEFT;
  }
  else if (*prec == '=')
  {
    prec++;
    justify = JUSTIFY_CENTER;
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
                     mutt_str_len(s), arboreal);
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
  mutt_format_s_x(buf, buflen, prec, s, false);
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
  mutt_format_s_x(buf, buflen, prec, s, true);
}

/**
 * mutt_paddstr - Display a string on screen, padded if necessary
 * @param win Window
 * @param n   Final width of field
 * @param s   String to display
 */
void mutt_paddstr(struct MuttWindow *win, int n, const char *s)
{
  wchar_t wc = 0;
  size_t k;
  size_t len = mutt_str_len(s);
  mbstate_t mbstate = { 0 };

  for (; len && (k = mbrtowc(&wc, s, len, &mbstate)); s += k, len -= k)
  {
    if ((k == (size_t) (-1)) || (k == (size_t) (-2)))
    {
      if (k == (size_t) (-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t) (-1)) ? 1 : len;
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    const int w = wcwidth(wc);
    if (w >= 0)
    {
      if (w > n)
        break;
      mutt_window_addnstr(win, (char *) s, k);
      n -= w;
    }
  }
  while (n-- > 0)
    mutt_window_addch(win, ' ');
}

/**
 * mutt_wstr_trunc - Work out how to truncate a widechar string
 * @param[in]  src    String to measure
 * @param[in]  maxlen Maximum length of string in bytes
 * @param[in]  maxwid Maximum width in screen columns
 * @param[out] width  Save the truncated screen column width
 * @retval num Bytes to use
 *
 * See how many bytes to copy from string so it's at most maxlen bytes long and
 * maxwid columns wide
 */
size_t mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width)
{
  wchar_t wc = 0;
  size_t n, w = 0, l = 0, cl;
  int cw;
  mbstate_t mbstate = { 0 };

  if (!src)
    goto out;

  n = mutt_str_len(src);

  for (w = 0; n && (cl = mbrtowc(&wc, src, n, &mbstate)); src += cl, n -= cl)
  {
    if ((cl == (size_t) (-1)) || (cl == (size_t) (-2)))
    {
      if (cl == (size_t) (-1))
        memset(&mbstate, 0, sizeof(mbstate));
      cl = (cl == (size_t) (-1)) ? 1 : n;
      wc = ReplacementChar;
    }
    cw = wcwidth(wc);
    /* hack because MUTT_TREE symbols aren't turned into characters
     * until rendered by print_enriched_string() */
    if ((cw < 0) && (src[0] == MUTT_SPECIAL_INDEX))
    {
      cl = 2; /* skip the index coloring sequence */
      cw = 0;
    }
    else if ((cw < 0) && (cl == 1) && (src[0] != '\0') && (src[0] < MUTT_TREE_MAX))
      cw = 1;
    else if (cw < 0)
      cw = 0; /* unprintable wchar */
    if ((cl + l > maxlen) || (cw + w > maxwid))
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
 * @retval num Screen cells string would use
 */
size_t mutt_strwidth(const char *s)
{
  if (!s)
    return 0;
  return mutt_strnwidth(s, mutt_str_len(s));
}

/**
 * mutt_strnwidth - Measure a string's width in screen cells
 * @param s String to be measured
 * @param n Length of string to be measured
 * @retval num Screen cells string would use
 */
size_t mutt_strnwidth(const char *s, size_t n)
{
  if (!s)
    return 0;

  wchar_t wc = 0;
  int w;
  size_t k;
  mbstate_t mbstate = { 0 };

  for (w = 0; n && (k = mbrtowc(&wc, s, n, &mbstate)); s += k, n -= k)
  {
    if (*s == MUTT_SPECIAL_INDEX)
    {
      s += 2; /* skip the index coloring sequence */
      k = 0;
      continue;
    }

    if ((k == (size_t) (-1)) || (k == (size_t) (-2)))
    {
      if (k == (size_t) (-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t) (-1)) ? 1 : n;
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    w += wcwidth(wc);
  }
  return w;
}
