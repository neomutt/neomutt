/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 2000 Edmund Grimley Evans <edmundo@rano.org>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#include "mutt.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "keymap.h"
#include "history.h"

#include <string.h>

/* redraw flags for mutt_enter_string() */
enum
{
  M_REDRAW_INIT = 1,	/* go to end of line and redraw */
  M_REDRAW_LINE		/* redraw entire line */
};

/* FIXME: these functions should deal with unprintable characters */

static int my_wcwidth (wchar_t wc)
{
  return wcwidth (wc);
}

static int my_wcswidth (const wchar_t *s, size_t n)
{
  int w = 0;
  while (n--)
    w += my_wcwidth (*s++);
  return w;
}

static int my_addwch (wchar_t wc)
{
  return mutt_addwch (wc);
}

static size_t width_ceiling (const wchar_t *s, size_t n, int w1)
{
  const wchar_t *s0 = s;
  int w = 0;
  for (; n; s++, n--)
    if ((w += my_wcwidth (*s)) > w1)
      break;
  return s - s0;  
}

void my_wcstombs(char *dest, size_t dlen, const wchar_t *src, size_t slen)
{
  mbstate_t st;
  size_t  k;

  memset (&st, 0, sizeof (st));
  for (; slen && dlen >= 2 * MB_LEN_MAX; dest += k, dlen -= k, src++, slen--)
    if ((k = wcrtomb (dest, *src, &st)) == (size_t)(-1))
      break;
  wcrtomb (dest, 0, &st); /* FIXME */
}

size_t my_mbstowcs (wchar_t **pwbuf, size_t *pwbuflen, size_t i, char *buf)
{
  wchar_t wc;
  mbstate_t st;
  size_t k;
  wchar_t *wbuf;
  size_t wbuflen;

  wbuf = *pwbuf, wbuflen = *pwbuflen;
  memset (&st, 0, sizeof (st));
  for (; (k = mbrtowc (&wc, buf, MB_LEN_MAX, &st)) &&
	 k != (size_t)(-1) && k != (size_t)(-2); buf += k)
  {
    if (i >= wbuflen)
    {
      wbuflen = i + 20;
      safe_realloc ((void **) &wbuf, wbuflen * sizeof (*wbuf));
    }
    wbuf[i++] = wc;
  }
  *pwbuf = wbuf, *pwbuflen = wbuflen;
  return i;
}

/*
 * Replace part of the wchar_t buffer, from FROM to TO, by BUF.
 */

static void replace_part (wchar_t **pwbuf, size_t *pwbuflen,
			  size_t from, size_t *to, size_t *end, char *buf)
{
  /* Save the suffix */
  size_t savelen = *end - *to;
  wchar_t *savebuf = safe_malloc (savelen * sizeof (wchar_t));
  memcpy (savebuf, *pwbuf + *to, savelen * sizeof (wchar_t));

  /* Convert to wide characters */
  *to = my_mbstowcs (pwbuf, pwbuflen, from, buf);

  /* Make space for suffix */
  if (*to + savelen > *pwbuflen)
  {
    *pwbuflen = *to + savelen;
    safe_realloc ((void **) pwbuf, *pwbuflen * sizeof (wchar_t));
  }

  /* Restore suffix */
  memcpy (*pwbuf + *to, savebuf, savelen * sizeof (wchar_t));
  *end = *to + savelen;

  free (savebuf);
}

/*
 * Returns:
 *	1 need to redraw the screen and call me again
 *	0 if input was given
 * 	-1 if abort.
 */

int _mutt_enter_string (char *buf, size_t buflen, int y, int x,
			int flags, int multiple, char ***files, int *numfiles)
{
  static wchar_t *wbuf = 0;
  static size_t wbuflen;
  static size_t wbufn;
  static size_t curpos;
  static size_t begin;
  int width = COLS - x - 1;
  int redraw;
  int pass = (flags & M_PASS);
  int first = 1;
  int tabs = 0; /* number of *consecutive* TABs */
  int ch, w, r;
  size_t i;
  wchar_t *tempbuf = 0;
  size_t templen = 0;
  history_class_t hclass;

  if (wbuf)
  {
    /* Coming back after return 1 */
    redraw = M_REDRAW_LINE;
  }
  else
  {
    /* Initialise wbuf from buf */
    wbuflen = 0;
    wbufn = my_mbstowcs (&wbuf, &wbuflen, 0, buf);
    redraw = M_REDRAW_INIT;
  }

  if (flags & (M_FILE | M_EFILE))
    hclass = HC_FILE;
  else if (flags & M_CMD)
    hclass = HC_CMD;
  else if (flags & M_ALIAS)
    hclass = HC_ALIAS;
  else if (flags & M_COMMAND)
    hclass = HC_COMMAND;
  else if (flags & M_PATTERN)
    hclass = HC_PATTERN;
  else 
    hclass = HC_OTHER;
    
  for (;;)
  {
    if (redraw && !pass)
    {
      if (redraw == M_REDRAW_INIT)
      {
	/* Go to end of line */
	curpos = wbufn;
	begin = width_ceiling (wbuf, wbufn, my_wcswidth (wbuf, wbufn) - width + 1);
      } 
      if (curpos < begin ||
	  my_wcswidth (wbuf + begin, curpos - begin) >= width)
	begin = width_ceiling (wbuf, wbufn, my_wcswidth (wbuf, curpos) - width / 2);
      move (y, x);
      w = 0;
      for (i = begin; i < wbufn; i++)
      {
	w += my_wcwidth (wbuf[i]);
	if (w > width)
	  break;
	my_addwch (wbuf[i]);
      }
      clrtoeol ();
      move (y, x + my_wcswidth (wbuf + begin, curpos - begin));
    }
    mutt_refresh ();

    if ((ch = km_dokey (MENU_EDITOR)) == -1)
    {
      free (wbuf);
      wbuf = 0;
      return -1;
    }

    if (ch != OP_NULL)
    {
      first = 0;
      if (ch != OP_EDITOR_COMPLETE)
	tabs = 0;
      redraw = M_REDRAW_LINE;
      switch (ch)
      {
	case OP_EDITOR_HISTORY_UP:
	  curpos = wbufn;
	  replace_part (&wbuf, &wbuflen, 0, &curpos, &wbufn,
			mutt_history_prev (hclass));
	  redraw = M_REDRAW_INIT;
	  break;

	case OP_EDITOR_HISTORY_DOWN:
	  curpos = wbufn;
	  replace_part (&wbuf, &wbuflen, 0, &curpos, &wbufn,
			mutt_history_prev (hclass));
	  redraw = M_REDRAW_INIT;
	  break;

	case OP_EDITOR_BACKSPACE:
	  if (curpos == 0)
	    BEEP ();
	  else
	  {
	    i = curpos;
	    while (i && !wcwidth (wbuf[i - 1]))
	      --i;
	    if (i)
	      --i;
	    memmove(wbuf + i, wbuf + curpos, (wbufn - curpos) * sizeof (*wbuf));
	    wbufn -= curpos - i;
	    curpos = i;
	  }
	  break;

	case OP_EDITOR_BOL:
	  curpos = 0;
	  break;

	case OP_EDITOR_EOL:
	  redraw= M_REDRAW_INIT;
	  break;

	case OP_EDITOR_KILL_LINE:
	  curpos = wbufn = 0;
	  break;

	case OP_EDITOR_KILL_EOL:
	  wbufn = curpos;
	  break;

	case OP_EDITOR_BACKWARD_CHAR:
	  if (curpos == 0)
	    BEEP ();
	  else
	  {
	    while (curpos && !wcwidth (wbuf[curpos - 1]))
	      --curpos;
	    if (curpos)
	      --curpos;
	  }
	  break;

	case OP_EDITOR_FORWARD_CHAR:
	  if (curpos == wbufn)
	    BEEP ();
	  else
	  {
	    ++curpos;
	    while (curpos < wbufn && !wcwidth (wbuf[curpos]))
	      ++curpos;
	  }
	  break;

	case OP_EDITOR_BACKWARD_WORD:
	  if (curpos == 0)
	    BEEP ();
	  else
	  {
	    while (curpos && iswspace (wbuf[curpos - 1]))
	      --curpos;
	    while (curpos && !iswspace (wbuf[curpos - 1]))
	      --curpos;
	  }
	  break;

	case OP_EDITOR_FORWARD_WORD:
	  if (curpos == wbufn)
	    BEEP ();
	  else
	  {
	    while (curpos < wbufn && iswspace (wbuf[curpos]))
	      ++curpos;
	    while (curpos < wbufn && !iswspace (wbuf[curpos]))
	      ++curpos;
	  }
	  break;

	case OP_EDITOR_CAPITALIZE_WORD:
	case OP_EDITOR_UPCASE_WORD:
	case OP_EDITOR_DOWNCASE_WORD:
	  if (curpos == wbufn)
	  {
	    BEEP ();
	    break;
	  }
	  while (curpos && !iswspace (wbuf[curpos]))
	    --curpos;
	  while (curpos < wbufn && iswspace (wbuf[curpos]))
	    ++curpos;
	  while (curpos < wbufn && !iswspace (wbuf[curpos]))
	  {
	    if (ch == OP_EDITOR_DOWNCASE_WORD)
	      wbuf[curpos] = towlower (wbuf[curpos]);
	    else
	    {
	      wbuf[curpos] = towupper (wbuf[curpos]);
	      if (ch == OP_EDITOR_CAPITALIZE_WORD)
		ch = OP_EDITOR_DOWNCASE_WORD;
	    }
	    curpos++;
	  }
	  break;

	case OP_EDITOR_DELETE_CHAR:
	  if (curpos == wbufn)
	    BEEP ();
	  else
	  {
	    i = curpos;
	    while (i < wbufn && !wcwidth (wbuf[i]))
	      ++i;
	    if (i < wbufn)
	      ++i;
	    while (i < wbufn && !wcwidth (wbuf[i]))
	      ++i;
	    memmove(wbuf + curpos, wbuf + i, (wbufn - i) * sizeof (*wbuf));
	    wbufn -= i - curpos;
	  }
	  break;

	case OP_EDITOR_BUFFY_CYCLE:
	  if (flags & M_EFILE)
	  {
	    first = 1; /* clear input if user types a real key later */
	    my_wcstombs (buf, buflen, wbuf, curpos);
	    mutt_buffy (buf);
	    curpos = wbufn = my_mbstowcs (&wbuf, &wbuflen, 0, buf);
	    break;
	  }
	  else if (!(flags & M_FILE))
	    goto self_insert;
	  /* fall through to completion routine (M_FILE) */

	case OP_EDITOR_COMPLETE:
	  tabs++;
	  if (flags & M_CMD)
	  {
	    for (i = curpos; i && wbuf[i-1] != ' '; i--)
	      ;
	    my_wcstombs (buf, buflen, wbuf + i, curpos - i);
	    if (tempbuf && templen == wbufn - i &&
		!memcmp (tempbuf, wbuf + i, (wbufn - i) * sizeof (*wbuf)))
	    {
	      mutt_select_file (buf, buflen, 0);
	      set_option (OPTNEEDREDRAW);
	      if (*buf)
		replace_part (&wbuf, &wbuflen, i, &curpos, &wbufn, buf);
	      return 1;
	    }
	    if (!mutt_complete (buf, buflen))
	    {
	      free (tempbuf);
	      templen = wbufn - i;
	      tempbuf = safe_malloc (templen * sizeof (*wbuf));
	      memcpy (tempbuf, wbuf + i, templen * sizeof (*wbuf));
	    }
	    else
	      BEEP ();

	    replace_part (&wbuf, &wbuflen, i, &curpos, &wbufn, buf);
	  }
	  else if (flags & M_ALIAS)
	  {
	    /* invoke the alias-menu to get more addresses */
	    for (i = curpos; i && wbuf[i-1] != ','; i--)
	      ;
	    for (; i < wbufn && wbuf[i] == ' '; i++)
	      ;
	    my_wcstombs (buf, buflen, wbuf + i, curpos - i);
	    r = mutt_alias_complete (buf, buflen);
	    replace_part (&wbuf, &wbuflen, i, &curpos, &wbufn, buf);
	    if (!r)
	      return 1;
	    break;
	  }
	  else if (flags & M_COMMAND)
	  {
	    my_wcstombs (buf, buflen, wbuf, curpos);
	    i = strlen (buf);
	    if (buf[i - 1] == '=' &&
		mutt_var_value_complete (buf, buflen, i))
	      tabs = 0;
	    else if (!mutt_command_complete (buf, buflen, i, tabs))
	      BEEP ();
	    replace_part (&wbuf, &wbuflen, 0, &curpos, &wbufn, buf);
	  }
	  else if (flags & (M_FILE | M_EFILE))
	  {
	    my_wcstombs (buf, buflen, wbuf, curpos);

	    /* see if the path has changed from the last time */
	    if (tempbuf && templen == wbufn &&
		!memcmp (tempbuf, wbuf, wbufn * sizeof (*wbuf)))
	    {
	      _mutt_select_file (buf, buflen, 0, multiple, files, numfiles);
	      set_option (OPTNEEDREDRAW);
	      if (*buf)
	      {
		mutt_pretty_mailbox (buf);
		if (!pass)
		  mutt_history_add (hclass, buf);
		return 0;
	      }

	      /* file selection cancelled */
	      return 1;
	    }

	    if (!mutt_complete (buf, buflen))
	    {
	      free (tempbuf);
	      templen = wbufn;
	      tempbuf = safe_malloc (templen * sizeof (*wbuf));
	      memcpy (tempbuf, wbuf, templen * sizeof (*wbuf));
	    }
	    else
	      BEEP (); /* let the user know that nothing matched */
	    replace_part (&wbuf, &wbuflen, 0, &curpos, &wbufn, buf);
	  }
	  else
	    goto self_insert;
	  break;

	case OP_EDITOR_COMPLETE_QUERY:
	  if (flags & M_ALIAS)
	  {
	    /* invoke the query-menu to get more addresses */
	    if (curpos)
	    {
	      for (i = curpos; i && buf[i - 1] != ','; i--)
		;
	      for (; i < curpos && buf[i] == ' '; i++)
		;
	      my_wcstombs (buf, buflen, wbuf + i, curpos - i);
	      mutt_query_complete (buf, buflen);
	      replace_part (&wbuf, &wbuflen, i, &curpos, &wbufn, buf);
	    }
	    else
	    {
	      my_wcstombs (buf, buflen, wbuf, curpos);
	      mutt_query_menu (buf, buflen);
	      replace_part (&wbuf, &wbuflen, 0, &curpos, &wbufn, buf);
	    }
	    return 1;
	  }
	  else
	    goto self_insert;

	case OP_EDITOR_QUOTE_CHAR:
	  {
	    event_t event;
	    /*ADDCH (LastKey);*/
	    event = mutt_getch ();
	    if (event.ch != -1)
	    {
	      LastKey = event.ch;
	      goto self_insert;
	    }
	  }

	default:
	  BEEP ();
      }
    }
    else
    {
      
self_insert:

      tabs = 0;
      /* use the raw keypress */
      ch = LastKey;

      if (first && (flags & M_CLEAR))
      {
	first = 0;
	if (IsWPrint (ch)) /* why? */
	  curpos = wbufn = 0;
      }

      if (CI_is_return (ch))
      {
	/* Convert from wide characters */
	my_wcstombs (buf, buflen, wbuf, wbufn);
	if (!pass)
	  mutt_history_add (hclass, buf);

	if (multiple)
	{
	  char **tfiles;
	  *numfiles = 1;
	  tfiles = safe_malloc (*numfiles * sizeof (char *));
	  mutt_expand_path (buf, buflen);
	  tfiles[0] = safe_strdup (buf);
	  *files = tfiles;
	}
	free (wbuf);
	wbuf = 0;
	return 0;
      }
      else if ((ch < ' ' || IsWPrint (ch))) /* why? */
      {
	if (wbufn >= wbuflen)
	{
	  wbuflen = wbufn + 20;
	  safe_realloc ((void **) &wbuf, wbuflen * sizeof (*wbuf));
	}
	memmove (wbuf + curpos + 1, wbuf + curpos, (wbufn - curpos) * sizeof (*wbuf));
	wbuf[curpos++] = ch;
	++wbufn;
      }
      else
      {
	mutt_flushinp ();
	BEEP ();
      }
    }
  }
}

/*
 * TODO:
 * very narrow screen might crash it
 * sort out the input side
 * unprintable chars
 * config tests for iswspace, towupper, towlower
 * OP_EDITOR_KILL_WORD
 * OP_EDITOR_KILL_EOW
 * OP_EDITOR_TRANSPOSE_CHARS
 */
