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
#include "editor/lib.h"
#include "history/lib.h"
#include "key/lib.h"
#include "question/lib.h"
#include "globals.h"
#include "msgcont.h"
#include "msgwin.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "protos.h"
#ifdef HAVE_ISWBLANK
#include <wctype.h>
#endif

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
  if (!ARRAY_EMPTY(&MacroEvents) && !OptForceRefresh)
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
 * mutt_edit_file - Let the user edit a file
 * @param editor User's editor config
 * @param file   File to edit
 */
void mutt_edit_file(const char *editor, const char *file)
{
  struct Buffer *cmd = buf_pool_get();

  mutt_endwin();
  buf_file_expand_fmt_quote(cmd, editor, file);
  if (mutt_system(buf_string(cmd)) != 0)
  {
    mutt_error(_("Error running \"%s\""), buf_string(cmd));
  }
  /* the terminal may have been resized while the editor owned it */
  mutt_resize_screen();
  keypad(stdscr, true);
  clearok(stdscr, true);

  buf_pool_release(&cmd);
}

/**
 * mutt_query_exit - Ask the user if they want to leave NeoMutt
 *
 * This function is called when the user presses the abort key.
 */
void mutt_query_exit(void)
{
  mutt_flushinp();
  if (query_yesorno(_("Exit NeoMutt without saving?"), MUTT_YES) == MUTT_YES)
  {
    mutt_exit(0); /* This call never returns */
  }
  mutt_clear_error();
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
  SigWinch = true;

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
  struct termios term = { 0 };
  struct termios old = { 0 };

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
 * mw_enter_fname - Ask the user to select a file - @ingroup gui_mw
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
 *
 * This function uses the message window.
 *
 * Allow the user to enter a filename.
 * If they hit '?' then the browser will be started.  See: dlg_browser()
 */
int mw_enter_fname(const char *prompt, struct Buffer *fname, bool mailbox,
                   struct Mailbox *m, bool multiple, char ***files,
                   int *numfiles, SelectFileFlags flags)
{
  struct MuttWindow *win = msgwin_new(true);
  if (!win)
    return -1;

  int rc = -1;

  struct Buffer *text = buf_pool_get();
  const struct AttrColor *ac_normal = simple_color_get(MT_COLOR_NORMAL);
  const struct AttrColor *ac_prompt = simple_color_get(MT_COLOR_PROMPT);

  msgwin_add_text(win, prompt, ac_prompt);
  msgwin_add_text(win, _(" ('?' for list): "), ac_prompt);
  if (!buf_is_empty(fname))
    msgwin_add_text(win, buf_string(fname), ac_normal);

  msgcont_push_window(win);
  struct MuttWindow *old_focus = window_set_focus(win);

  struct KeyEvent event = { 0, OP_NULL };
  do
  {
    window_redraw(NULL);
    event = mutt_getch(GETCH_NO_FLAGS);
  } while ((event.op == OP_TIMEOUT) || (event.op == OP_REPAINT));

  mutt_refresh();
  win = msgcont_pop_window();
  window_set_focus(old_focus);
  mutt_window_free(&win);

  if (event.ch < 0)
    goto done;

  if (event.ch == '?')
  {
    buf_reset(fname);

    if (flags == MUTT_SEL_NO_FLAGS)
      flags = MUTT_SEL_FOLDER;
    if (multiple)
      flags |= MUTT_SEL_MULTI;
    if (mailbox)
      flags |= MUTT_SEL_MAILBOX;
    dlg_browser(fname, flags, m, files, numfiles);
  }
  else
  {
    char *pc = NULL;
    mutt_str_asprintf(&pc, "%s: ", prompt);
    if (event.op == OP_NULL)
      mutt_unget_ch(event.ch);
    else
      mutt_unget_op(event.op);

    buf_alloc(fname, 1024);
    struct FileCompletionData cdata = { multiple, m, files, numfiles };
    enum HistoryClass hclass = mailbox ? HC_MAILBOX : HC_FILE;
    if (mw_get_field(pc, fname, MUTT_COMP_CLEAR, hclass, &CompleteMailboxOps, &cdata) != 0)
    {
      buf_reset(fname);
    }
    FREE(&pc);
  }

  rc = 0;

done:
  buf_pool_release(&text);
  return rc;
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

  if (((n1 = wcrtomb(buf, wc, &mbstate)) == ICONV_ILLEGAL_SEQ) ||
      ((n2 = wcrtomb(buf + n1, 0, &mbstate)) == ICONV_ILLEGAL_SEQ))
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
    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if ((k == ICONV_ILLEGAL_SEQ) && (errno == EILSEQ))
        memset(&mbstate1, 0, sizeof(mbstate1));

      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : n;
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
      if ((k2 == ICONV_ILLEGAL_SEQ) || (k2 > buflen))
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
  {
    *p = '\0';
  }
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
    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if (k == ICONV_ILLEGAL_SEQ)
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : len;
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
    if (cl == ICONV_ILLEGAL_SEQ)
    {
      memset(&mbstate, 0, sizeof(mbstate));
      cl = 1;
      wc = ReplacementChar;
    }
    else if (cl == ICONV_BUF_TOO_SMALL)
    {
      cl = n;
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
    {
      cw = 1;
    }
    else if (cw < 0)
    {
      cw = 0; /* unprintable wchar */
    }
    if (wc == '\n')
      break;
    if (((cl + l) > maxlen) || ((cw + w) > maxwid))
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

    if ((k == ICONV_ILLEGAL_SEQ) || (k == ICONV_BUF_TOO_SMALL))
    {
      if (k == ICONV_ILLEGAL_SEQ)
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == ICONV_ILLEGAL_SEQ) ? 1 : n;
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    w += wcwidth(wc);
  }
  return w;
}

/**
 * mw_what_key - Display the value of a key - @ingroup gui_mw
 *
 * This function uses the message window.
 *
 * Displays the octal value back to the user. e.g.
 * `Char = h, Octal = 150, Decimal = 104`
 *
 * Press the $abort_key (default Ctrl-G) to exit.
 */
void mw_what_key(void)
{
  struct MuttWindow *win = msgwin_new(true);
  if (!win)
    return;

  char prompt[256] = { 0 };
  snprintf(prompt, sizeof(prompt), _("Enter keys (%s to abort): "), km_keyname(AbortKey));
  msgwin_set_text(win, prompt, MT_COLOR_PROMPT);

  msgcont_push_window(win);
  struct MuttWindow *old_focus = window_set_focus(win);
  window_redraw(win);

  char keys[256] = { 0 };
  const struct AttrColor *ac_normal = simple_color_get(MT_COLOR_NORMAL);
  const struct AttrColor *ac_prompt = simple_color_get(MT_COLOR_PROMPT);

  // ---------------------------------------------------------------------------
  // Event Loop
  timeout(1000); // 1 second
  while (true)
  {
    int ch = getch();
    if (ch == AbortKey)
      break;

    if (ch == KEY_RESIZE)
    {
      timeout(0);
      while ((ch = getch()) == KEY_RESIZE)
      {
        // do nothing
      }
    }

    if (ch == ERR)
    {
      if (!isatty(0)) // terminal was lost
        mutt_exit(1);

      if (SigWinch)
      {
        SigWinch = false;
        notify_send(NeoMutt->notify_resize, NT_RESIZE, 0, NULL);
        window_redraw(NULL);
      }
      else
      {
        notify_send(NeoMutt->notify_timeout, NT_TIMEOUT, 0, NULL);
      }

      continue;
    }

    msgwin_clear_text(win);
    snprintf(keys, sizeof(keys), _("Char = %s, Octal = %o, Decimal = %d\n"),
             km_keyname(ch), ch, ch);
    msgwin_add_text(win, keys, ac_normal);
    msgwin_add_text(win, prompt, ac_prompt);
    msgwin_add_text(win, NULL, NULL);
    window_redraw(NULL);
  }
  // ---------------------------------------------------------------------------

  win = msgcont_pop_window();
  window_set_focus(old_focus);
  mutt_window_free(&win);
}
