
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

static int my_wcwidth (wchar_t wc)
{
  int n = wcwidth (wc);
  if (IsWPrint (wc) && n > 0)
    return n;
  if (!(wc & ~0x7f))
    return 2;
  if (!(wc & ~0xffff))
    return 6;
  return 10;
}

/* combining mark / non-spacing character */
#define COMB_CHAR(wc) (IsWPrint (wc) && !wcwidth (wc))

static int my_wcswidth (const wchar_t *s, size_t n)
{
  int w = 0;
  while (n--)
    w += my_wcwidth (*s++);
  return w;
}

static int my_addwch (wchar_t wc)
{
  int n = wcwidth (wc);
  if (IsWPrint (wc) && n > 0)
    return mutt_addwch (wc);
  if (!(wc & ~0x7f))
    return printw ("^%c", ((int)wc + 0x40) & 0x7f);
  if (!(wc & ~0xffff))
    return printw ("\\u%04x", (int)wc);
  return printw ("\\u%08x", (int)wc);
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

static void my_wcstombs (char *dest, size_t dlen, const wchar_t *src, size_t slen)
{
  mbstate_t st;
  size_t k;

  /* First convert directly into the destination buffer */
  memset (&st, 0, sizeof (st));
  for (; slen && dlen >= MB_LEN_MAX; dest += k, dlen -= k, src++, slen--)
    if ((k = wcrtomb (dest, *src, &st)) == (size_t)(-1))
      break;

  /* If this works, we can stop now */
  if (dlen >= MB_LEN_MAX) {
    wcrtomb (dest, 0, &st);
    return;
  }

  /* Otherwise convert any remaining data into a local buffer */
  {
    char buf[3 * MB_LEN_MAX];
    char *p = buf;

    for (; slen && p - buf < dlen; p += k, src++, slen--)
      if ((k = wcrtomb (p, *src, &st)) == (size_t)(-1))
	break;
    p += wcrtomb (p, 0, &st);

    /* If it fits into the destination buffer, we can stop now */
    if (p - buf <= dlen) {
      memcpy (dest, buf, p - buf);
      return;
    }

    /* Otherwise we truncate the string in an ugly fashion */
    memcpy (dest, buf, dlen);
    dest[dlen - 1] = '\0'; /* assume original dlen > 0 */
  }
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
 * Replace part of the wchar_t buffer, from FROM to CURPOS, by BUF.
 */

static void replace_part (ENTER_STATE *state, size_t from, char *buf)
{
  /* Save the suffix */
  size_t savelen = state->lastchar - state->curpos;
  wchar_t *savebuf = safe_malloc (savelen * sizeof (wchar_t));
  memcpy (savebuf, state->wbuf + state->curpos, savelen * sizeof (wchar_t));

  /* Convert to wide characters */
  state->curpos = my_mbstowcs (&state->wbuf, &state->wbuflen, from, buf);

  /* Make space for suffix */
  if (state->curpos + savelen > state->wbuflen)
  {
    state->wbuflen = state->curpos + savelen;
    safe_realloc ((void **) &state->wbuf, state->wbuflen * sizeof (wchar_t));
  }

  /* Restore suffix */
  memcpy (state->wbuf + state->curpos, savebuf, savelen * sizeof (wchar_t));
  state->lastchar = state->curpos + savelen;

  safe_free ((void **) &savebuf);
}

/*
 * Returns:
 *	1 need to redraw the screen and call me again
 *	0 if input was given
 * 	-1 if abort.
 */

int  mutt_enter_string(char *buf, size_t buflen, int y, int x, int flags)
{
  int rv;
  ENTER_STATE *es = mutt_new_enter_state ();
  rv = _mutt_enter_string (buf, buflen, y, x, flags, 0, NULL, NULL, es);
  mutt_free_enter_state (&es);
  return rv;
}

int _mutt_enter_string (char *buf, size_t buflen, int y, int x,
			int flags, int multiple, char ***files, int *numfiles,
			ENTER_STATE *state)
{
  int width = COLS - x - 1;
  int redraw;
  int pass = (flags & M_PASS);
  int first = 1;
  int ch, w, r;
  size_t i;
  wchar_t *tempbuf = 0;
  size_t templen = 0;
  history_class_t hclass;

  int rv = 0;
  
  if (state->wbuf)
  {
    /* Coming back after return 1 */
    redraw = M_REDRAW_LINE;
  }
  else
  {
    /* Initialise wbuf from buf */
    state->wbuflen = 0;
    state->lastchar = my_mbstowcs (&state->wbuf, &state->wbuflen, 0, buf);
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
	state->curpos = state->lastchar;
	state->begin = width_ceiling (state->wbuf, state->lastchar, my_wcswidth (state->wbuf, state->lastchar) - width + 1);
      } 
      if (state->curpos < state->begin ||
	  my_wcswidth (state->wbuf + state->begin, state->curpos - state->begin) >= width)
	state->begin = width_ceiling (state->wbuf, state->lastchar, my_wcswidth (state->wbuf, state->curpos) - width / 2);
      move (y, x);
      w = 0;
      for (i = state->begin; i < state->lastchar; i++)
      {
	w += my_wcwidth (state->wbuf[i]);
	if (w > width)
	  break;
	my_addwch (state->wbuf[i]);
      }
      clrtoeol ();
      move (y, x + my_wcswidth (state->wbuf + state->begin, state->curpos - state->begin));
    }
    mutt_refresh ();

    if ((ch = km_dokey (MENU_EDITOR)) == -1)
    {
      rv = -1; 
      goto bye;
    }

    if (ch != OP_NULL)
    {
      first = 0;
      if (ch != OP_EDITOR_COMPLETE)
	state->tabs = 0;
      redraw = M_REDRAW_LINE;
      switch (ch)
      {
	case OP_EDITOR_HISTORY_UP:
	  state->curpos = state->lastchar;
	  replace_part (state, 0, mutt_history_prev (hclass));
	  redraw = M_REDRAW_INIT;
	  break;

	case OP_EDITOR_HISTORY_DOWN:
	  state->curpos = state->lastchar;
	  replace_part (state, 0, mutt_history_prev (hclass));
	  redraw = M_REDRAW_INIT;
	  break;

	case OP_EDITOR_BACKSPACE:
	  if (state->curpos == 0)
	    BEEP ();
	  else
	  {
	    i = state->curpos;
	    while (i && COMB_CHAR (state->wbuf[i - 1]))
	      --i;
	    if (i)
	      --i;
	    memmove (state->wbuf + i, state->wbuf + state->curpos, (state->lastchar - state->curpos) * sizeof (wchar_t));
	    state->lastchar -= state->curpos - i;
	    state->curpos = i;
	  }
	  break;

	case OP_EDITOR_BOL:
	  state->curpos = 0;
	  break;

	case OP_EDITOR_EOL:
	  redraw= M_REDRAW_INIT;
	  break;

	case OP_EDITOR_KILL_LINE:
	  state->curpos = state->lastchar = 0;
	  break;

	case OP_EDITOR_KILL_EOL:
	  state->lastchar = state->curpos;
	  break;

	case OP_EDITOR_BACKWARD_CHAR:
	  if (state->curpos == 0)
	    BEEP ();
	  else
	  {
	    while (state->curpos && COMB_CHAR (state->wbuf[state->curpos - 1]))
	      state->curpos--;
	    if (state->curpos)
	      state->curpos--;
	  }
	  break;

	case OP_EDITOR_FORWARD_CHAR:
	  if (state->curpos == state->lastchar)
	    BEEP ();
	  else
	  {
	    ++state->curpos;
	    while (state->curpos < state->lastchar && COMB_CHAR (state->wbuf[state->curpos]))
	      ++state->curpos;
	  }
	  break;

	case OP_EDITOR_BACKWARD_WORD:
	  if (state->curpos == 0)
	    BEEP ();
	  else
	  {
	    while (state->curpos && iswspace (state->wbuf[state->curpos - 1]))
	      --state->curpos;
	    while (state->curpos && !iswspace (state->wbuf[state->curpos - 1]))
	      --state->curpos;
	  }
	  break;

	case OP_EDITOR_FORWARD_WORD:
	  if (state->curpos == state->lastchar)
	    BEEP ();
	  else
	  {
	    while (state->curpos < state->lastchar && iswspace (state->wbuf[state->curpos]))
	      ++state->curpos;
	    while (state->curpos < state->lastchar && !iswspace (state->wbuf[state->curpos]))
	      ++state->curpos;
	  }
	  break;

	case OP_EDITOR_CAPITALIZE_WORD:
	case OP_EDITOR_UPCASE_WORD:
	case OP_EDITOR_DOWNCASE_WORD:
	  if (state->curpos == state->lastchar)
	  {
	    BEEP ();
	    break;
	  }
	  while (state->curpos && !iswspace (state->wbuf[state->curpos]))
	    --state->curpos;
	  while (state->curpos < state->lastchar && iswspace (state->wbuf[state->curpos]))
	    ++state->curpos;
	  while (state->curpos < state->lastchar && !iswspace (state->wbuf[state->curpos]))
	  {
	    if (ch == OP_EDITOR_DOWNCASE_WORD)
	      state->wbuf[state->curpos] = towlower (state->wbuf[state->curpos]);
	    else
	    {
	      state->wbuf[state->curpos] = towupper (state->wbuf[state->curpos]);
	      if (ch == OP_EDITOR_CAPITALIZE_WORD)
		ch = OP_EDITOR_DOWNCASE_WORD;
	    }
	    state->curpos++;
	  }
	  break;

	case OP_EDITOR_DELETE_CHAR:
	  if (state->curpos == state->lastchar)
	    BEEP ();
	  else
	  {
	    i = state->curpos;
	    while (i < state->lastchar && COMB_CHAR (state->wbuf[i]))
	      ++i;
	    if (i < state->lastchar)
	      ++i;
	    while (i < state->lastchar && COMB_CHAR (state->wbuf[i]))
	      ++i;
	    memmove (state->wbuf + state->curpos, state->wbuf + i, (state->lastchar - i) * sizeof (wchar_t));
	    state->lastchar -= i - state->curpos;
	  }
	  break;

	case OP_EDITOR_KILL_WORD:
	  /* delete to begining of word */
	  if (state->curpos != 0)
	  {
	    i = state->curpos;
	    while (i && iswspace (state->wbuf[i - 1]))
	      --i;
	    if (i)
	    {
	      if (iswalnum (state->wbuf[i - 1]))
	      {
		for (--i; i && iswalnum (state->wbuf[i - 1]); i--)
		  ;
	      }
	      else
		--i;
	    }
	    memmove (state->wbuf + i, state->wbuf + state->curpos,
		     (state->lastchar - state->curpos) * sizeof (wchar_t));
	    state->lastchar += i - state->curpos;
	    state->curpos = i;
	  }
	  break;

	case OP_EDITOR_KILL_EOW:
	  /* delete to end of word */
	  for (i = state->curpos;
	       i < state->lastchar && iswspace (state->wbuf[i]); i++)
	    ;
	  for (; i < state->lastchar && !iswspace (state->wbuf[i]); i++)
	    ;
	  memmove (state->wbuf + state->curpos, state->wbuf + i,
		   (state->lastchar - i) * sizeof (wchar_t));
	  state->lastchar += state->curpos - i;
	  break;

	case OP_EDITOR_BUFFY_CYCLE:
	  if (flags & M_EFILE)
	  {
	    first = 1; /* clear input if user types a real key later */
	    my_wcstombs (buf, buflen, state->wbuf, state->curpos);
	    mutt_buffy (buf, buflen);
	    state->curpos = state->lastchar = my_mbstowcs (&state->wbuf, &state->wbuflen, 0, buf);
	    break;
	  }
	  else if (!(flags & M_FILE))
	    goto self_insert;
	  /* fall through to completion routine (M_FILE) */

	case OP_EDITOR_COMPLETE:
	  state->tabs++;
	  if (flags & M_CMD)
	  {
	    for (i = state->curpos; i && state->wbuf[i-1] != ' '; i--)
	      ;
	    my_wcstombs (buf, buflen, state->wbuf + i, state->curpos - i);
	    if (tempbuf && templen == state->lastchar - i &&
		!memcmp (tempbuf, state->wbuf + i, (state->lastchar - i) * sizeof (wchar_t)))
	    {
	      mutt_select_file (buf, buflen, (flags & M_EFILE) ? M_SEL_FOLDER : 0);
	      set_option (OPTNEEDREDRAW);
	      if (*buf)
		replace_part (state, i, buf);
	      rv = 1; 
	      goto bye;
	    }
	    if (!mutt_complete (buf, buflen))
	    {
	      templen = state->lastchar - i;
	      safe_realloc ((void **) &tempbuf, templen * sizeof (wchar_t));
	    }
	    else
	      BEEP ();

	    replace_part (state, i, buf);
	  }
	  else if (flags & M_ALIAS)
	  {
	    /* invoke the alias-menu to get more addresses */
	    for (i = state->curpos; i && state->wbuf[i-1] != ','; i--)
	      ;
	    for (; i < state->lastchar && state->wbuf[i] == ' '; i++)
	      ;
	    my_wcstombs (buf, buflen, state->wbuf + i, state->curpos - i);
	    r = mutt_alias_complete (buf, buflen);
	    replace_part (state, i, buf);
	    if (!r)
	    {
	      rv = 1;
	      goto bye;
	    }
	    break;
	  }
	  else if (flags & M_COMMAND)
	  {
	    my_wcstombs (buf, buflen, state->wbuf, state->curpos);
	    i = strlen (buf);
	    if (i && buf[i - 1] == '=' &&
		mutt_var_value_complete (buf, buflen, i))
	      state->tabs = 0;
	    else if (!mutt_command_complete (buf, buflen, i, state->tabs))
	      BEEP ();
	    replace_part (state, 0, buf);
	  }
	  else if (flags & (M_FILE | M_EFILE))
	  {
	    my_wcstombs (buf, buflen, state->wbuf, state->curpos);

	    /* see if the path has changed from the last time */
	    if ((!tempbuf && !state->lastchar) || (tempbuf && templen == state->lastchar &&
		!memcmp (tempbuf, state->wbuf, state->lastchar * sizeof (wchar_t))))
	    {
	      _mutt_select_file (buf, buflen, 
				 ((flags & M_EFILE) ? M_SEL_FOLDER : 0) | (multiple ? M_SEL_MULTI : 0), 
				 files, numfiles);
	      set_option (OPTNEEDREDRAW);
	      if (*buf)
	      {
		mutt_pretty_mailbox (buf);
		if (!pass)
		  mutt_history_add (hclass, buf);
		rv = 0;
		goto bye;
	      }

	      /* file selection cancelled */
	      rv = 1;
	      goto bye;
	    }

	    if (!mutt_complete (buf, buflen))
	    {
	      templen = state->lastchar;
	      safe_realloc ((void **) &tempbuf, templen * sizeof (wchar_t));
	      memcpy (tempbuf, state->wbuf, templen * sizeof (wchar_t));
	    }
	    else
	      BEEP (); /* let the user know that nothing matched */
	    replace_part (state, 0, buf);
	  }
	  else
	    goto self_insert;
	  break;

	case OP_EDITOR_COMPLETE_QUERY:
	  if (flags & M_ALIAS)
	  {
	    /* invoke the query-menu to get more addresses */
	    if ((i = state->curpos))
	    {
	      for (; i && state->wbuf[i - 1] != ','; i--)
		;
	      for (; i < state->curpos && state->wbuf[i] == ' '; i++)
		;
	    }

	    my_wcstombs (buf, buflen, state->wbuf + i, state->curpos - i);
	    mutt_query_complete (buf, buflen);
	    replace_part (state, i, buf);

	    rv = 1; 
	    goto bye;
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

	case OP_EDITOR_TRANSPOSE_CHARS:
	  if (state->lastchar < 2)
	    BEEP ();
	  else
	{
	    wchar_t t;

	    if (state->curpos == 0)
	      state->curpos = 2;
	    else if (state->curpos < state->lastchar)
	      ++state->curpos;

	    t = state->wbuf[state->curpos - 2];
	    state->wbuf[state->curpos - 2] = state->wbuf[state->curpos - 1];
	    state->wbuf[state->curpos - 1] = t;
	  }
	  break;

	default:
	  BEEP ();
      }
    }
    else
    {
      
self_insert:

      state->tabs = 0;
      /* use the raw keypress */
      ch = LastKey;

      if (first && (flags & M_CLEAR))
      {
	first = 0;
	if (IsWPrint (ch)) /* why? */
	  state->curpos = state->lastchar = 0;
      }

      if (CI_is_return (ch))
      {
	/* Convert from wide characters */
	my_wcstombs (buf, buflen, state->wbuf, state->lastchar);
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
	rv = 0; 
	goto bye;
      }
      else if (ch && (ch < ' ' || IsWPrint (ch))) /* why? */
      {
	if (state->lastchar >= state->wbuflen)
	{
	  state->wbuflen = state->lastchar + 20;
	  safe_realloc ((void **) &state->wbuf, state->wbuflen * sizeof (wchar_t));
	}
	memmove (state->wbuf + state->curpos + 1, state->wbuf + state->curpos, (state->lastchar - state->curpos) * sizeof (wchar_t));
	state->wbuf[state->curpos++] = ch;
	state->lastchar++;
      }
      else
      {
	mutt_flushinp ();
	BEEP ();
      }
    }
  }
  
  bye:
  
  safe_free ((void **) &tempbuf);
  return rv;
}

void mutt_free_enter_state (ENTER_STATE **esp)
{
  if (!esp) return;
  
  safe_free ((void **) &(*esp)->wbuf);
  safe_free ((void **) esp);
}

/*
 * TODO:
 * very narrow screen might crash it
 * sort out the input side
 * unprintable chars
 */
