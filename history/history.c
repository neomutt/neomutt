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

/**
 * @page history_history Read/write command history from/to a file
 *
 * Read/write command history from/to a file.
 *
 * This history ring grows from 0..#C_History, with last marking the
 * where new entries go:
 * ```
 *         0        the oldest entry in the ring
 *         1        entry
 *         ...
 *         x-1      most recently entered text
 *  last-> x        NULL  (this will be overwritten next)
 *         x+1      NULL
 *         ...
 *         #C_History  NULL
 * ```
 * Once the array fills up, it is used as a ring.  last points where a new
 * entry will go.  Older entries are "up", and wrap around:
 * ```
 *         0        entry
 *         1        entry
 *         ...
 *         y-1      most recently entered text
 *  last-> y        entry (this will be overwritten next)
 *         y+1      the oldest entry in the ring
 *         ...
 *         #C_History  entry
 * ```
 * When $history_remove_dups is set, duplicate entries are scanned and removed
 * each time a new entry is added.  In order to preserve the history ring size,
 * entries 0..last are compacted up.  Entries last+1..#C_History are
 * compacted down:
 * ```
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
 *         #C_History  entry
 * ```
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "lib.h"

/* These Config Variables are only used in history/history.c */
short C_History; ///< Config: Number of history entries to keep in memory per category
char *C_HistoryFile;      ///< Config: File to save history in
bool C_HistoryRemoveDups; ///< Config: Remove duplicate entries from the history
short C_SaveHistory; ///< Config: Number of history entries to save per category

#define HC_FIRST HC_CMD

/**
 * struct History - Saved list of user-entered commands/searches
 *
 * This is a ring buffer of strings.
 */
struct History
{
  char **hist;
  short cur;
  short last;
};

/* global vars used for the string-history routines */

static struct History Histories[HC_MAX];
static int OldSize = 0;

/**
 * get_history - Get a particular history
 * @param hclass Type of history to find
 * @retval ptr History ring buffer
 */
static struct History *get_history(enum HistoryClass hclass)
{
  if ((hclass >= HC_MAX) || (C_History == 0))
    return NULL;

  struct History *hist = &Histories[hclass];
  return hist->hist ? hist : NULL;
}

/**
 * init_history - Set up a new History ring buffer
 * @param h History to populate
 *
 * If the History already has entries, they will be freed.
 */
static void init_history(struct History *h)
{
  if (OldSize != 0)
  {
    if (h->hist)
    {
      for (int i = 0; i <= OldSize; i++)
        FREE(&h->hist[i]);
      FREE(&h->hist);
    }
  }

  if (C_History != 0)
    h->hist = mutt_mem_calloc(C_History + 1, sizeof(char *));

  h->cur = 0;
  h->last = 0;
}

/**
 * dup_hash_dec - Decrease the refcount of a history string
 * @param dup_hash Hash Table containing unique history strings
 * @param str      String to find
 * @retval  0 String was deleted from the Hash Table
 * @retval >0 Refcount of string
 * @retval -1 Error, string not found
 *
 * If the string's refcount is 1, then the string will be deleted.
 */
static int dup_hash_dec(struct HashTable *dup_hash, char *str)
{
  struct HashElem *elem = mutt_hash_find_elem(dup_hash, str);
  if (!elem)
    return -1;

  uintptr_t count = (uintptr_t) elem->data;
  if (count <= 1)
  {
    mutt_hash_delete(dup_hash, str, NULL);
    return 0;
  }

  count--;
  elem->data = (void *) count;
  return count;
}

/**
 * dup_hash_inc - Increase the refcount of a history string
 * @param dup_hash Hash Table containing unique history strings
 * @param str      String to find
 * @retval num Refcount of string
 *
 * If the string isn't found it will be added to the Hash Table.
 */
static int dup_hash_inc(struct HashTable *dup_hash, char *str)
{
  uintptr_t count;

  struct HashElem *elem = mutt_hash_find_elem(dup_hash, str);
  if (!elem)
  {
    count = 1;
    mutt_hash_insert(dup_hash, str, (void *) count);
    return count;
  }

  count = (uintptr_t) elem->data;
  count++;
  elem->data = (void *) count;
  return count;
}

/**
 * shrink_histfile - Read, de-dupe and write the history file
 */
static void shrink_histfile(void)
{
  FILE *fp_tmp = NULL;
  int n[HC_MAX] = { 0 };
  int line, hclass, read;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;
  bool regen_file = false;
  struct HashTable *dup_hashes[HC_MAX] = { 0 };

  FILE *fp = mutt_file_fopen(C_HistoryFile, "r");
  if (!fp)
    return;

  if (C_HistoryRemoveDups)
    for (hclass = 0; hclass < HC_MAX; hclass++)
      dup_hashes[hclass] = mutt_hash_new(MAX(10, C_SaveHistory * 2), MUTT_HASH_STRDUP_KEYS);

  line = 0;
  while ((linebuf = mutt_file_read_line(linebuf, &buflen, fp, &line, 0)))
  {
    if ((sscanf(linebuf, "%d:%n", &hclass, &read) < 1) || (read == 0) ||
        (*(p = linebuf + strlen(linebuf) - 1) != '|') || (hclass < 0))
    {
      mutt_error(_("Bad history file format (line %d)"), line);
      goto cleanup;
    }
    /* silently ignore too high class (probably newer neomutt) */
    if (hclass >= HC_MAX)
      continue;
    *p = '\0';
    if (C_HistoryRemoveDups && (dup_hash_inc(dup_hashes[hclass], linebuf + read) > 1))
    {
      regen_file = true;
      continue;
    }
    n[hclass]++;
  }

  if (!regen_file)
  {
    for (hclass = HC_FIRST; hclass < HC_MAX; hclass++)
    {
      if (n[hclass] > C_SaveHistory)
      {
        regen_file = true;
        break;
      }
    }
  }

  if (regen_file)
  {
    fp_tmp = mutt_file_mkstemp();
    if (!fp_tmp)
    {
      mutt_perror(_("Can't create temporary file"));
      goto cleanup;
    }
    rewind(fp);
    line = 0;
    while ((linebuf = mutt_file_read_line(linebuf, &buflen, fp, &line, 0)))
    {
      if ((sscanf(linebuf, "%d:%n", &hclass, &read) < 1) || (read == 0) ||
          (*(p = linebuf + strlen(linebuf) - 1) != '|') || (hclass < 0))
      {
        mutt_error(_("Bad history file format (line %d)"), line);
        goto cleanup;
      }
      if (hclass >= HC_MAX)
        continue;
      *p = '\0';
      if (C_HistoryRemoveDups && (dup_hash_dec(dup_hashes[hclass], linebuf + read) > 0))
      {
        continue;
      }
      *p = '|';
      if (n[hclass]-- <= C_SaveHistory)
        fprintf(fp_tmp, "%s\n", linebuf);
    }
  }

cleanup:
  mutt_file_fclose(&fp);
  FREE(&linebuf);
  if (fp_tmp)
  {
    if ((fflush(fp_tmp) == 0) && (fp = fopen(NONULL(C_HistoryFile), "w")))
    {
      rewind(fp_tmp);
      mutt_file_copy_stream(fp_tmp, fp);
      mutt_file_fclose(&fp);
    }
    mutt_file_fclose(&fp_tmp);
  }
  if (C_HistoryRemoveDups)
    for (hclass = 0; hclass < HC_MAX; hclass++)
      mutt_hash_free(&dup_hashes[hclass]);
}

/**
 * save_history - Save one history string to a file
 * @param hclass History type
 * @param str    String to save
 */
static void save_history(enum HistoryClass hclass, const char *str)
{
  static int n = 0;
  char *tmp = NULL;

  if (!str || (*str == '\0')) /* This shouldn't happen, but it's safer. */
    return;

  FILE *fp = mutt_file_fopen(C_HistoryFile, "a");
  if (!fp)
    return;

  tmp = mutt_str_dup(str);
  mutt_ch_convert_string(&tmp, C_Charset, "utf-8", 0);

  /* Format of a history item (1 line): "<histclass>:<string>|".
   * We add a '|' in order to avoid lines ending with '\'. */
  fprintf(fp, "%d:", (int) hclass);
  for (char *p = tmp; *p; p++)
  {
    /* Don't copy \n as a history item must fit on one line. The string
     * shouldn't contain such a character anyway, but as this can happen
     * in practice, we must deal with that. */
    if (*p != '\n')
      putc((unsigned char) *p, fp);
  }
  fputs("|\n", fp);

  mutt_file_fclose(&fp);
  FREE(&tmp);

  if (--n < 0)
  {
    n = C_SaveHistory;
    shrink_histfile();
  }
}

/**
 * remove_history_dups - De-dupe the history
 * @param hclass History to de-dupe
 * @param str    String to find
 *
 * If the string is found, it is removed from the history.
 *
 * When removing dups, we want the created "blanks" to be right below the
 * resulting h->last position.  See the comment section above 'struct History'.
 */
static void remove_history_dups(enum HistoryClass hclass, const char *str)
{
  struct History *h = get_history(hclass);
  if (!h)
    return; /* disabled */

  /* Remove dups from 0..last-1 compacting up. */
  int source = 0;
  int dest = 0;
  while (source < h->last)
  {
    if (mutt_str_equal(h->hist[source], str))
      FREE(&h->hist[source++]);
    else
      h->hist[dest++] = h->hist[source++];
  }

  /* Move 'last' entry up. */
  h->hist[dest] = h->hist[source];
  int old_last = h->last;
  h->last = dest;

  /* Fill in moved entries with NULL */
  while (source > h->last)
    h->hist[source--] = NULL;

  /* Remove dups from last+1 .. C_History compacting down. */
  source = C_History;
  dest = C_History;
  while (source > old_last)
  {
    if (mutt_str_equal(h->hist[source], str))
      FREE(&h->hist[source--]);
    else
      h->hist[dest--] = h->hist[source--];
  }

  /* Fill in moved entries with NULL */
  while (dest > old_last)
    h->hist[dest--] = NULL;
}

/**
 * mutt_hist_search - Find matches in a history list
 * @param[in]  search_buf String to find
 * @param[in]  hclass     History list
 * @param[out] matches    All the matching lines
 * @retval num Matches found
 */
int mutt_hist_search(const char *search_buf, enum HistoryClass hclass, char **matches)
{
  if (!search_buf || !matches)
    return 0;

  struct History *h = get_history(hclass);
  if (!h)
    return 0;

  int match_count = 0;
  int cur = h->last;
  do
  {
    cur--;
    if (cur < 0)
      cur = C_History;
    if (cur == h->last)
      break;
    if (mutt_istr_find(h->hist[cur], search_buf))
      matches[match_count++] = h->hist[cur];
  } while (match_count < C_History);

  return match_count;
}

/**
 * mutt_hist_free - Free all the history lists
 */
void mutt_hist_free(void)
{
  for (enum HistoryClass hclass = HC_FIRST; hclass < HC_MAX; hclass++)
  {
    struct History *h = &Histories[hclass];
    if (!h->hist)
      continue;

    /* The array has (C_History+1) elements */
    for (int i = 0; i <= C_History; i++)
    {
      FREE(&h->hist[i]);
    }
    FREE(&h->hist);
  }
}

/**
 * mutt_hist_init - Create a set of empty History ring buffers
 *
 * This just creates empty histories.
 * To fill them, call mutt_hist_read_file().
 */
void mutt_hist_init(void)
{
  if (C_History == OldSize)
    return;

  for (enum HistoryClass hclass = HC_FIRST; hclass < HC_MAX; hclass++)
    init_history(&Histories[hclass]);

  OldSize = C_History;
}

/**
 * mutt_hist_add - Add a string to a history
 * @param hclass History to add to
 * @param str    String to add
 * @param save   Should the changes be saved to file immediately?
 */
void mutt_hist_add(enum HistoryClass hclass, const char *str, bool save)
{
  struct History *h = get_history(hclass);
  if (!h)
    return; /* disabled */

  if (*str)
  {
    int prev = h->last - 1;
    if (prev < 0)
      prev = C_History;

    /* don't add to prompt history:
     *  - lines beginning by a space
     *  - repeated lines */
    if ((*str != ' ') && (!h->hist[prev] || !mutt_str_equal(h->hist[prev], str)))
    {
      if (C_HistoryRemoveDups)
        remove_history_dups(hclass, str);
      if (save && (C_SaveHistory != 0) && C_HistoryFile)
        save_history(hclass, str);
      mutt_str_replace(&h->hist[h->last++], str);
      if (h->last > C_History)
        h->last = 0;
    }
  }
  h->cur = h->last; /* reset to the last entry */
}

/**
 * mutt_hist_next - Get the next string in a History
 * @param hclass History to choose
 * @retval ptr Next string
 *
 * If there is no next string, and empty string will be returned.
 */
char *mutt_hist_next(enum HistoryClass hclass)
{
  struct History *h = get_history(hclass);
  if (!h)
    return ""; /* disabled */

  int next = h->cur;
  do
  {
    next++;
    if (next > C_History)
      next = 0;
    if (next == h->last)
      break;
  } while (!h->hist[next]);

  h->cur = next;
  return NONULL(h->hist[h->cur]);
}

/**
 * mutt_hist_prev - Get the previous string in a History
 * @param hclass History to choose
 * @retval ptr Previous string
 *
 * If there is no previous string, and empty string will be returned.
 */
char *mutt_hist_prev(enum HistoryClass hclass)
{
  struct History *h = get_history(hclass);
  if (!h)
    return ""; /* disabled */

  int prev = h->cur;
  do
  {
    prev--;
    if (prev < 0)
      prev = C_History;
    if (prev == h->last)
      break;
  } while (!h->hist[prev]);

  h->cur = prev;
  return NONULL(h->hist[h->cur]);
}

/**
 * mutt_hist_reset_state - Move the 'current' position to the end of the History
 * @param hclass History to reset
 *
 * After calling mutt_hist_next() and mutt_hist_prev(), this function resets
 * the current position ('cur' pointer).
 */
void mutt_hist_reset_state(enum HistoryClass hclass)
{
  struct History *h = get_history(hclass);
  if (!h)
    return; /* disabled */

  h->cur = h->last;
}

/**
 * mutt_hist_read_file - Read the History from a file
 *
 * The file #C_HistoryFile is read and parsed into separate History ring buffers.
 */
void mutt_hist_read_file(void)
{
  int line = 0, hclass, read;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;

  if (!C_HistoryFile)
    return;

  FILE *fp = mutt_file_fopen(C_HistoryFile, "r");
  if (!fp)
    return;

  while ((linebuf = mutt_file_read_line(linebuf, &buflen, fp, &line, 0)))
  {
    read = 0;
    if ((sscanf(linebuf, "%d:%n", &hclass, &read) < 1) || (read == 0) ||
        (*(p = linebuf + strlen(linebuf) - 1) != '|') || (hclass < 0))
    {
      mutt_error(_("Bad history file format (line %d)"), line);
      break;
    }
    /* silently ignore too high class (probably newer neomutt) */
    if (hclass >= HC_MAX)
      continue;
    *p = '\0';
    p = mutt_str_dup(linebuf + read);
    if (p)
    {
      mutt_ch_convert_string(&p, "utf-8", C_Charset, 0);
      mutt_hist_add(hclass, p, false);
      FREE(&p);
    }
  }

  mutt_file_fclose(&fp);
  FREE(&linebuf);
}

/**
 * mutt_hist_at_scratch - Is the current History position at the 'scratch' place?
 * @param hclass History to use
 * @retval true History is at 'scratch' place
 *
 * The last entry in the history is used as a 'scratch' area.
 * It can be overwritten as the user types and edits.
 *
 * To get (back) to the scratch area, call mutt_hist_next(), mutt_hist_prev()
 * or mutt_hist_reset_state().
 */
bool mutt_hist_at_scratch(enum HistoryClass hclass)
{
  struct History *h = get_history(hclass);
  if (!h)
    return false; /* disabled */

  return h->cur == h->last;
}

/**
 * mutt_hist_save_scratch - Save a temporary string to the History
 * @param hclass History to alter
 * @param str    String to set
 *
 * Write a 'scratch' string into the History's current position.
 * This is useful to preserver a user's edits.
 */
void mutt_hist_save_scratch(enum HistoryClass hclass, const char *str)
{
  struct History *h = get_history(hclass);
  if (!h)
    return; /* disabled */

  /* Don't check if str has a value because the scratch buffer may contain
   * an old garbage value that should be overwritten */
  mutt_str_replace(&h->hist[h->last], str);
}
