/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "history.h"

/* global vars used for the string-history routines */

struct history
{
  char **hist;
  short cur;
  short last;
}; 

static struct history History[HC_LAST];
static int OldSize = 0;

static void init_history (struct history *h)
{
  int i;

  if(OldSize)
  {
    if (h->hist)
    {
      for (i = 0 ; i < OldSize ; i ++)
	safe_free ((void **) &h->hist[i]);
      safe_free ((void **) &h->hist);
    }
  }
  
  if (HistSize)
    h->hist = safe_calloc (HistSize, sizeof (char *));
  
  h->cur = 0;
  h->last = 0;
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
  
void mutt_history_add (history_class_t hclass, const char *s)
{
  int prev;
  struct history *h = &History[hclass];
  
  if (!HistSize)
    return; /* disabled */

  if (*s)
  {
    prev = h->last - 1;
    if (prev < 0) prev = HistSize - 1;
    if (!h->hist[prev] || mutt_strcmp (h->hist[prev], s) != 0)
    {
      safe_free ((void **) &h->hist[h->last]);
      h->hist[h->last++] = safe_strdup (s);
      if (h->last > HistSize - 1)
	h->last = 0;
    }
  }
  h->cur = h->last; /* reset to the last entry */
}

char *mutt_history_next (history_class_t hclass)
{
  int next;
  struct history *h = &History[hclass];
  
  if (!HistSize)
    return (""); /* disabled */

  next = h->cur + 1;
  if (next > h->last - 1)
    next = 0;
  if (h->hist[next])
    h->cur = next;
  return (h->hist[h->cur] ? h->hist[h->cur] : "");
}

char *mutt_history_prev (history_class_t hclass)
{
  int prev;
  struct history *h = &History[hclass];

  if (!HistSize)
    return (""); /* disabled */

  prev = h->cur - 1;
  if (prev < 0)
  {
    prev = h->last - 1;
    if (prev < 0)
    {
      prev = HistSize - 1;
      while (prev > 0 && h->hist[prev] == NULL)
	prev--;
    }
  }
  if (h->hist[prev])
    h->cur = prev;
  return (h->hist[h->cur] ? h->hist[h->cur] : "");
}
