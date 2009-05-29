/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
#include "history.h"

struct history
{
  char **hist;
  short cur;
  short last;
}; 

/* global vars used for the string-history routines */

static struct history History[HC_LAST];
static int OldSize = 0;

#define GET_HISTORY(CLASS)	((CLASS >= HC_LAST) ? NULL : &History[CLASS])

static void init_history (struct history *h)
{
  int i;

  if(OldSize)
  {
    if (h->hist)
    {
      for (i = 0 ; i < OldSize ; i ++)
	FREE (&h->hist[i]);
      FREE (&h->hist);
    }
  }
  
  if (HistSize)
    h->hist = safe_calloc (HistSize, sizeof (char *));
  
  h->cur = 0;
  h->last = 0;
}

void mutt_read_histfile (void)
{
  FILE *f;
  int line = 0, hclass, read;
  char *linebuf = NULL, *p;
  size_t buflen;

  if ((f = fopen (HistFile, "r")) == NULL)
    return;

  while ((linebuf = mutt_read_line (linebuf, &buflen, f, &line, 0)) != NULL)
  {
    read = 0;
    if (sscanf (linebuf, "%d:%n", &hclass, &read) < 1 || read == 0 ||
        *(p = linebuf + strlen (linebuf) - 1) != '|' || hclass < 0)
    {
      mutt_error (_("Bad history file format (line %d)"), line);
      break;
    }
    /* silently ignore too high class (probably newer mutt) */
    if (hclass >= HC_LAST)
      continue;
    *p = '\0';
    p = safe_strdup (linebuf + read);
    if (p)
    {
      mutt_convert_string (&p, "utf-8", Charset, 0);
      mutt_history_add (hclass, p, 0);
      FREE (&p);
    }
  }

  safe_fclose (&f);
  FREE (&linebuf);
}

static void shrink_histfile (void)
{
  char tmpfname[_POSIX_PATH_MAX];
  FILE *f, *tmp = NULL;
  int n[HC_LAST] = { 0 };
  int line, hclass;
  char *linebuf = NULL;
  size_t buflen;

  if ((f = fopen (HistFile, "r")) == NULL)
    return;

  line = 0;
  while ((linebuf = mutt_read_line (linebuf, &buflen, f, &line, 0)) != NULL)
  {
    if (sscanf (linebuf, "%d", &hclass) < 1 || hclass < 0)
    {
      mutt_error (_("Bad history file format (line %d)"), line);
      goto cleanup;
    }
    /* silently ignore too high class (probably newer mutt) */
    if (hclass >= HC_LAST)
      continue;
    n[hclass]++;
  }

  for(hclass = HC_FIRST; hclass < HC_LAST; hclass++)
    if (n[hclass] > SaveHist)
    {
      mutt_mktemp (tmpfname);
      if ((tmp = safe_fopen (tmpfname, "w+")) == NULL)
        mutt_perror (tmpfname);
      break;
    }

  if (tmp != NULL)
  {
    rewind (f);
    line = 0;
    while ((linebuf = mutt_read_line (linebuf, &buflen, f, &line, 0)) != NULL)
    {
      if (sscanf (linebuf, "%d", &hclass) < 1 || hclass < 0)
      {
        mutt_error (_("Bad history file format (line %d)"), line);
        goto cleanup;
      }
      /* silently ignore too high class (probably newer mutt) */
      if (hclass >= HC_LAST)
	continue;
      if (n[hclass]-- <= SaveHist)
        fprintf (tmp, "%s\n", linebuf);
    }
  }

cleanup:
  safe_fclose (&f);
  FREE (&linebuf);
  if (tmp != NULL)
  {
    if (fflush (tmp) == 0 &&
        (f = fopen (HistFile, "w")) != NULL) /* __FOPEN_CHECKED__ */
    {
      rewind (tmp);
      mutt_copy_stream (tmp, f);
      safe_fclose (&f);
    }
    safe_fclose (&tmp);
    unlink (tmpfname);
  }
}

static void save_history (history_class_t hclass, const char *s)
{
  static int n = 0;
  FILE *f;
  char *tmp, *p;

  if (!s || !*s)  /* This shouldn't happen, but it's safer. */
    return;

  if ((f = fopen (HistFile, "a")) == NULL)
  {
    mutt_perror ("fopen");
    return;
  }

  tmp = safe_strdup (s);
  mutt_convert_string (&tmp, Charset, "utf-8", 0);

  /* Format of a history item (1 line): "<histclass>:<string>|".
     We add a '|' in order to avoid lines ending with '\'. */
  fprintf (f, "%d:", (int) hclass);
  for (p = tmp; *p; p++)
  {
    /* Don't copy \n as a history item must fit on one line. The string
       shouldn't contain such a character anyway, but as this can happen
       in practice, we must deal with that. */
    if (*p != '\n')
      putc ((unsigned char) *p, f);
  }
  fputs ("|\n", f);

  safe_fclose (&f);
  FREE (&tmp);

  if (--n < 0)
  {
    n = SaveHist;
    shrink_histfile();
  }
}

void mutt_init_history(void)
{
  history_class_t hclass;
  
  if (HistSize == OldSize)
    return;
  
  for(hclass = HC_FIRST; hclass < HC_LAST; hclass++)
    init_history(&History[hclass]);

  OldSize = HistSize;
}
  
void mutt_history_add (history_class_t hclass, const char *s, int save)
{
  int prev;
  struct history *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return; /* disabled */

  if (*s)
  {
    prev = h->last - 1;
    if (prev < 0) prev = HistSize - 1;

    /* don't add to prompt history:
     *  - lines beginning by a space
     *  - repeated lines
     */
    if (*s != ' ' && (!h->hist[prev] || mutt_strcmp (h->hist[prev], s) != 0))
    {
      if (save && SaveHist)
        save_history (hclass, s);
      mutt_str_replace (&h->hist[h->last++], s);
      if (h->last > HistSize - 1)
	h->last = 0;
    }
  }
  h->cur = h->last; /* reset to the last entry */
}

char *mutt_history_next (history_class_t hclass)
{
  int next;
  struct history *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return (""); /* disabled */

  next = h->cur + 1;
  if (next > HistSize - 1)
    next = 0;
  h->cur = h->hist[next] ? next : 0;
  return (h->hist[h->cur] ? h->hist[h->cur] : "");
}

char *mutt_history_prev (history_class_t hclass)
{
  int prev;
  struct history *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return (""); /* disabled */

  prev = h->cur - 1;
  if (prev < 0)
  {
    prev = HistSize - 1;
    while (prev > 0 && h->hist[prev] == NULL)
      prev--;
  }
  if (h->hist[prev])
    h->cur = prev;
  return (h->hist[h->cur] ? h->hist[h->cur] : "");
}
