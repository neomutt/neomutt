/**
 * @file
 * Read/write command history from/to a file
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "history.h"
#include "charset.h"
#include "globals.h"
#include "hash.h"
#include "lib.h"
#include "protos.h"

/* This history ring grows from 0..HistSize, with last marking the
 * where new entries go:
 *         0        the oldest entry in the ring
 *         1        entry
 *         ...
 *         x-1      most recently entered text
 *  last-> x        NULL  (this will be overwritten next)
 *         x+1      NULL
 *         ...
 *         HistSize NULL
 *
 * Once the array fills up, it is used as a ring.  last points where a new
 * entry will go.  Older entries are "up", and wrap around:
 *         0        entry
 *         1        entry
 *         ...
 *         y-1      most recently entered text
 *  last-> y        entry (this will be overwritten next)
 *         y+1      the oldest entry in the ring
 *         ...
 *         HistSize entry
 *
 * When $history_remove_dups is set, duplicate entries are scanned and removed
 * each time a new entry is added.  In order to preserve the history ring size,
 * entries 0..last are compacted up.  Entries last+1..HistSize are
 * compacted down:
 *         0        entry
 *         1        entry
 *         ...
 *         z-1      most recently entered text
 *  last-> z        entry (this will be overwritten next)
 *         z+1      NULL
 *         z+2      NULL
 *         ...
 *                  the oldest entry in the ring
 *                  next oldest entry
 *         HistSize entry
 */

/**
 * struct History - Saved list of user-entered commands/searches
 */
struct History
{
  char **hist;
  short cur;
  short last;
};

/* global vars used for the string-history routines */

static struct History History[HC_LAST];
static int OldSize = 0;

#define GET_HISTORY(CLASS) ((CLASS >= HC_LAST) ? NULL : &History[CLASS])

static void init_history(struct History *h)
{
  int i;

  if (OldSize)
  {
    if (h->hist)
    {
      for (i = 0; i <= OldSize; i++)
        FREE(&h->hist[i]);
      FREE(&h->hist);
    }
  }

  if (HistSize)
    h->hist = safe_calloc(HistSize + 1, sizeof(char *));

  h->cur = 0;
  h->last = 0;
}

void mutt_read_histfile(void)
{
  FILE *f = NULL;
  int line = 0, hclass, read;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;

  if ((f = fopen(HistFile, "r")) == NULL)
    return;

  while ((linebuf = mutt_read_line(linebuf, &buflen, f, &line, 0)) != NULL)
  {
    read = 0;
    if (sscanf(linebuf, "%d:%n", &hclass, &read) < 1 || read == 0 ||
        *(p = linebuf + strlen(linebuf) - 1) != '|' || hclass < 0)
    {
      mutt_error(_("Bad history file format (line %d)"), line);
      break;
    }
    /* silently ignore too high class (probably newer mutt) */
    if (hclass >= HC_LAST)
      continue;
    *p = '\0';
    p = safe_strdup(linebuf + read);
    if (p)
    {
      mutt_convert_string(&p, "utf-8", Charset, 0);
      mutt_history_add(hclass, p, 0);
      FREE(&p);
    }
  }

  safe_fclose(&f);
  FREE(&linebuf);
}

static int dup_hash_dec(struct Hash *dup_hash, char *s)
{
  struct HashElem *elem = NULL;
  uintptr_t count;

  elem = hash_find_elem(dup_hash, s);
  if (!elem)
    return -1;

  count = (uintptr_t) elem->data;
  if (count <= 1)
  {
    hash_delete(dup_hash, s, NULL, NULL);
    return 0;
  }

  count--;
  elem->data = (void *) count;
  return count;
}

static int dup_hash_inc(struct Hash *dup_hash, char *s)
{
  struct HashElem *elem = NULL;
  uintptr_t count;

  elem = hash_find_elem(dup_hash, s);
  if (!elem)
  {
    count = 1;
    hash_insert(dup_hash, s, (void *) count);
    return count;
  }

  count = (uintptr_t) elem->data;
  count++;
  elem->data = (void *) count;
  return count;
}

static void shrink_histfile(void)
{
  char tmpfname[_POSIX_PATH_MAX];
  FILE *f = NULL, *tmp = NULL;
  int n[HC_LAST] = { 0 };
  int line, hclass, read;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;
  bool regen_file = false;
  struct Hash *dup_hashes[HC_LAST] = { 0 };

  if ((f = fopen(HistFile, "r")) == NULL)
    return;

  if (option(OPTHISTREMOVEDUPS))
    for (hclass = 0; hclass < HC_LAST; hclass++)
      dup_hashes[hclass] = hash_create(MAX(10, SaveHist * 2), MUTT_HASH_STRDUP_KEYS);

  line = 0;
  while ((linebuf = mutt_read_line(linebuf, &buflen, f, &line, 0)) != NULL)
  {
    if (sscanf(linebuf, "%d:%n", &hclass, &read) < 1 || read == 0 ||
        *(p = linebuf + strlen(linebuf) - 1) != '|' || hclass < 0)
    {
      mutt_error(_("Bad history file format (line %d)"), line);
      goto cleanup;
    }
    /* silently ignore too high class (probably newer mutt) */
    if (hclass >= HC_LAST)
      continue;
    *p = '\0';
    if (option(OPTHISTREMOVEDUPS) && (dup_hash_inc(dup_hashes[hclass], linebuf + read) > 1))
    {
      regen_file = true;
      continue;
    }
    n[hclass]++;
  }

  if (!regen_file)
    for (hclass = HC_FIRST; hclass < HC_LAST; hclass++)
      if (n[hclass] > SaveHist)
      {
        regen_file = true;
        break;
      }

  if (regen_file)
  {
    mutt_mktemp(tmpfname, sizeof(tmpfname));
    if ((tmp = safe_fopen(tmpfname, "w+")) == NULL)
    {
      mutt_perror(tmpfname);
      goto cleanup;
    }
    rewind(f);
    line = 0;
    while ((linebuf = mutt_read_line(linebuf, &buflen, f, &line, 0)) != NULL)
    {
      if (sscanf(linebuf, "%d:%n", &hclass, &read) < 1 || read == 0 ||
          *(p = linebuf + strlen(linebuf) - 1) != '|' || hclass < 0)
      {
        mutt_error(_("Bad history file format (line %d)"), line);
        goto cleanup;
      }
      if (hclass >= HC_LAST)
        continue;
      *p = '\0';
      if (option(OPTHISTREMOVEDUPS) &&
          (dup_hash_dec(dup_hashes[hclass], linebuf + read) > 0))
        continue;
      *p = '|';
      if (n[hclass]-- <= SaveHist)
        fprintf(tmp, "%s\n", linebuf);
    }
  }

cleanup:
  safe_fclose(&f);
  FREE(&linebuf);
  if (tmp)
  {
    if (fflush(tmp) == 0 && (f = fopen(HistFile, "w")) != NULL)
    {
      rewind(tmp);
      mutt_copy_stream(tmp, f);
      safe_fclose(&f);
    }
    safe_fclose(&tmp);
    unlink(tmpfname);
  }
  if (option(OPTHISTREMOVEDUPS))
    for (hclass = 0; hclass < HC_LAST; hclass++)
      hash_destroy(&dup_hashes[hclass], NULL);
}

static void save_history(enum HistoryClass hclass, const char *s)
{
  static int n = 0;
  FILE *f = NULL;
  char *tmp = NULL, *p = NULL;

  if (!s || !*s) /* This shouldn't happen, but it's safer. */
    return;

  if ((f = fopen(HistFile, "a")) == NULL)
  {
    mutt_perror("fopen");
    return;
  }

  tmp = safe_strdup(s);
  mutt_convert_string(&tmp, Charset, "utf-8", 0);

  /* Format of a history item (1 line): "<histclass>:<string>|".
     We add a '|' in order to avoid lines ending with '\'. */
  fprintf(f, "%d:", (int) hclass);
  for (p = tmp; *p; p++)
  {
    /* Don't copy \n as a history item must fit on one line. The string
       shouldn't contain such a character anyway, but as this can happen
       in practice, we must deal with that. */
    if (*p != '\n')
      putc((unsigned char) *p, f);
  }
  fputs("|\n", f);

  safe_fclose(&f);
  FREE(&tmp);

  if (--n < 0)
  {
    n = SaveHist;
    shrink_histfile();
  }
}

/**
 * remove_history_dups - De-dupe the history
 *
 * When removing dups, we want the created "blanks" to be right below the
 * resulting h->last position.  See the comment section above 'struct History'.
 */
static void remove_history_dups(enum HistoryClass hclass, const char *s)
{
  int source, dest, old_last;
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return; /* disabled */

  /* Remove dups from 0..last-1 compacting up. */
  source = dest = 0;
  while (source < h->last)
  {
    if (!mutt_strcmp(h->hist[source], s))
      FREE(&h->hist[source++]);
    else
      h->hist[dest++] = h->hist[source++];
  }

  /* Move 'last' entry up. */
  h->hist[dest] = h->hist[source];
  old_last = h->last;
  h->last = dest;

  /* Fill in moved entries with NULL */
  while (source > h->last)
    h->hist[source--] = NULL;

  /* Remove dups from last+1 .. HistSize compacting down. */
  source = dest = HistSize;
  while (source > old_last)
  {
    if (!mutt_strcmp(h->hist[source], s))
      FREE(&h->hist[source--]);
    else
      h->hist[dest--] = h->hist[source--];
  }

  /* Fill in moved entries with NULL */
  while (dest > old_last)
    h->hist[dest--] = NULL;
}

void mutt_init_history(void)
{
  if (HistSize == OldSize)
    return;

  for (enum HistoryClass hclass = HC_FIRST; hclass < HC_LAST; hclass++)
    init_history(&History[hclass]);

  OldSize = HistSize;
}

void mutt_history_add(enum HistoryClass hclass, const char *s, int save)
{
  int prev;
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return; /* disabled */

  if (*s)
  {
    prev = h->last - 1;
    if (prev < 0)
      prev = HistSize;

    /* don't add to prompt history:
     *  - lines beginning by a space
     *  - repeated lines
     */
    if (*s != ' ' && (!h->hist[prev] || (mutt_strcmp(h->hist[prev], s) != 0)))
    {
      if (option(OPTHISTREMOVEDUPS))
        remove_history_dups(hclass, s);
      if (save && SaveHist)
        save_history(hclass, s);
      mutt_str_replace(&h->hist[h->last++], s);
      if (h->last > HistSize)
        h->last = 0;
    }
  }
  h->cur = h->last; /* reset to the last entry */
}

char *mutt_history_next(enum HistoryClass hclass)
{
  int next;
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return ""; /* disabled */

  next = h->cur;
  do
  {
    next++;
    if (next > HistSize)
      next = 0;
    if (next == h->last)
      break;
  } while (h->hist[next] == NULL);

  h->cur = next;
  return (h->hist[h->cur] ? h->hist[h->cur] : "");
}

char *mutt_history_prev(enum HistoryClass hclass)
{
  int prev;
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return ""; /* disabled */

  prev = h->cur;
  do
  {
    prev--;
    if (prev < 0)
      prev = HistSize;
    if (prev == h->last)
      break;
  } while (h->hist[prev] == NULL);

  h->cur = prev;
  return (h->hist[h->cur] ? h->hist[h->cur] : "");
}

void mutt_reset_history_state(enum HistoryClass hclass)
{
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return; /* disabled */

  h->cur = h->last;
}

int mutt_history_at_scratch(enum HistoryClass hclass)
{
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return 0; /* disabled */

  return h->cur == h->last;
}

void mutt_history_save_scratch(enum HistoryClass hclass, const char *s)
{
  struct History *h = GET_HISTORY(hclass);

  if (!HistSize || !h)
    return; /* disabled */

  /* Don't check if s has a value because the scratch buffer may contain
   * an old garbage value that should be overwritten */
  mutt_str_replace(&h->hist[h->last], s);
}
