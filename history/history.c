/**
 * @file
 * Read/write command history from/to a file
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page history_history History file handling
 *
 * Read/write command history from/to a file.
 *
 * This history ring grows from 0..`$history`, with last marking the
 * where new entries go:
 * ```
 *         0        the oldest entry in the ring
 *         1        entry
 *         ...
 *         x-1      most recently entered text
 *  last-> x        NULL  (this will be overwritten next)
 *         x+1      NULL
 *         ...
 *         `$history`  NULL
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
 *         `$history`  entry
 * ```
 * When $history_remove_dups is set, duplicate entries are scanned and removed
 * each time a new entry is added.  In order to preserve the history ring size,
 * entries 0..last are compacted up.  Entries last+1..`$history` are
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
 *         `$history`  entry
 * ```
 */

#include "config.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"

#define HC_FIRST HC_EXT_COMMAND

/**
 * struct History - Saved list of user-entered commands/searches
 *
 * This is a ring buffer of strings.
 */
struct History
{
  char **hist; ///< Array of history items
  short cur;   ///< Current history item
  short last;  ///< Last history item
};

/* global vars used for the string-history routines */

/// Command histories, one for each #HistoryClass
static struct History Histories[HC_MAX];
/// The previous number of history entries to save
/// @sa $history
static int OldSize = 0;

/**
 * get_history - Get a particular history
 * @param hclass Type of history to find
 * @retval ptr History ring buffer
 */
static struct History *get_history(enum HistoryClass hclass)
{
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  if ((hclass >= HC_MAX) || (c_history == 0))
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

  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  if (c_history != 0)
    h->hist = MUTT_MEM_CALLOC(c_history + 1, char *);

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
  struct HashElem *he = mutt_hash_find_elem(dup_hash, str);
  if (!he)
    return -1;

  uintptr_t count = (uintptr_t) he->data;
  if (count <= 1)
  {
    mutt_hash_delete(dup_hash, str, NULL);
    return 0;
  }

  count--;
  he->data = (void *) count;
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

  struct HashElem *he = mutt_hash_find_elem(dup_hash, str);
  if (!he)
  {
    count = 1;
    mutt_hash_insert(dup_hash, str, (void *) count);
    return count;
  }

  count = (uintptr_t) he->data;
  count++;
  he->data = (void *) count;
  return count;
}

/**
 * shrink_histfile - Read, de-dupe and write the history file
 */
static void shrink_histfile(void)
{
  FILE *fp_tmp = NULL;
  int n[HC_MAX] = { 0 };
  int line, hclass = 0, read = 0;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;
  bool regen_file = false;
  struct HashTable *dup_hashes[HC_MAX] = { 0 };

  const char *const c_history_file = cs_subset_path(NeoMutt->sub, "history_file");
  FILE *fp = mutt_file_fopen(c_history_file, "r");
  if (!fp)
    return;

  const bool c_history_remove_dups = cs_subset_bool(NeoMutt->sub, "history_remove_dups");
  const short c_save_history = cs_subset_number(NeoMutt->sub, "save_history");
  if (c_history_remove_dups)
  {
    for (hclass = 0; hclass < HC_MAX; hclass++)
      dup_hashes[hclass] = mutt_hash_new(MAX(10, c_save_history * 2), MUTT_HASH_STRDUP_KEYS);
  }

  line = 0;
  while ((linebuf = mutt_file_read_line(linebuf, &buflen, fp, &line, MUTT_RL_NO_FLAGS)))
  {
    if ((sscanf(linebuf, "%d:%n", &hclass, &read) < 1) || (read == 0) ||
        (*(p = linebuf + strlen(linebuf) - 1) != '|') || (hclass < 0))
    {
      mutt_error(_("%s:%d: Bad history file format"), c_history_file, line);
      regen_file = true;
      continue;
    }
    /* silently ignore too high class (probably newer neomutt) */
    if (hclass >= HC_MAX)
      continue;
    *p = '\0';
    if (c_history_remove_dups && (dup_hash_inc(dup_hashes[hclass], linebuf + read) > 1))
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
      if (n[hclass] > c_save_history)
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
    while ((linebuf = mutt_file_read_line(linebuf, &buflen, fp, &line, MUTT_RL_NO_FLAGS)))
    {
      if ((sscanf(linebuf, "%d:%n", &hclass, &read) < 1) || (read == 0) ||
          (*(p = linebuf + strlen(linebuf) - 1) != '|') || (hclass < 0))
      {
        continue;
      }
      if (hclass >= HC_MAX)
        continue;
      *p = '\0';
      if (c_history_remove_dups && (dup_hash_dec(dup_hashes[hclass], linebuf + read) > 0))
      {
        continue;
      }
      *p = '|';
      if (n[hclass]-- <= c_save_history)
        fprintf(fp_tmp, "%s\n", linebuf);
    }
  }

cleanup:
  mutt_file_fclose(&fp);
  FREE(&linebuf);
  if (fp_tmp)
  {
    if (fflush(fp_tmp) == 0)
    {
      if (truncate(c_history_file, 0) < 0)
        mutt_debug(LL_DEBUG1, "truncate: %s\n", strerror(errno));
      fp = mutt_file_fopen(c_history_file, "w");
      if (fp)
      {
        rewind(fp_tmp);
        mutt_file_copy_stream(fp_tmp, fp);
        mutt_file_fclose(&fp);
      }
    }
    mutt_file_fclose(&fp_tmp);
  }
  if (c_history_remove_dups)
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

  if (!str || (*str == '\0')) // This shouldn't happen, but it's safer
    return;

  const char *const c_history_file = cs_subset_path(NeoMutt->sub, "history_file");
  FILE *fp = mutt_file_fopen(c_history_file, "a");
  if (!fp)
    return;

  char *tmp = mutt_str_dup(str);
  mutt_ch_convert_string(&tmp, cc_charset(), "utf-8", MUTT_ICONV_NO_FLAGS);

  // If tmp contains '\n' terminate it there.
  char *nl = strchr(tmp, '\n');
  if (nl)
    *nl = '\0';

  /* Format of a history item (1 line): "<histclass>:<string>|".
   * We add a '|' in order to avoid lines ending with '\'. */
  fprintf(fp, "%d:%s|\n", (int) hclass, tmp);

  mutt_file_fclose(&fp);
  FREE(&tmp);

  if (--n < 0)
  {
    const short c_save_history = cs_subset_number(NeoMutt->sub, "save_history");
    n = c_save_history;
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

  /* Remove dups from last+1 .. `$history` compacting down. */
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  source = c_history;
  dest = c_history;
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
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  do
  {
    cur--;
    if (cur < 0)
      cur = c_history;
    if (cur == h->last)
      break;
    if (mutt_istr_find(h->hist[cur], search_buf))
      matches[match_count++] = h->hist[cur];
  } while (match_count < c_history);

  return match_count;
}

/**
 * mutt_hist_cleanup - Free all the history lists
 */
void mutt_hist_cleanup(void)
{
  if (!NeoMutt)
    return;

  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  for (enum HistoryClass hclass = HC_FIRST; hclass < HC_MAX; hclass++)
  {
    struct History *h = &Histories[hclass];
    if (!h->hist)
      continue;

    /* The array has (`$history`+1) elements */
    for (int i = 0; i <= c_history; i++)
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
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  if (c_history == OldSize)
    return;

  for (enum HistoryClass hclass = HC_FIRST; hclass < HC_MAX; hclass++)
    init_history(&Histories[hclass]);

  OldSize = c_history;
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
    const short c_history = cs_subset_number(NeoMutt->sub, "history");
    if (prev < 0)
      prev = c_history;

    /* don't add to prompt history:
     *  - lines beginning by a space
     *  - repeated lines */
    if ((*str != ' ') && (!h->hist[prev] || !mutt_str_equal(h->hist[prev], str)))
    {
      const bool c_history_remove_dups = cs_subset_bool(NeoMutt->sub, "history_remove_dups");
      if (c_history_remove_dups)
        remove_history_dups(hclass, str);
      const short c_save_history = cs_subset_number(NeoMutt->sub, "save_history");
      const char *const c_history_file = cs_subset_path(NeoMutt->sub, "history_file");
      if (save && (c_save_history != 0) && c_history_file)
        save_history(hclass, str);
      mutt_str_replace(&h->hist[h->last++], str);
      if (h->last > c_history)
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
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  do
  {
    next++;
    if (next > c_history)
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
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  do
  {
    prev--;
    if (prev < 0)
      prev = c_history;
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
 * The file `$history_file` is read and parsed into separate History ring buffers.
 */
void mutt_hist_read_file(void)
{
  const char *const c_history_file = cs_subset_path(NeoMutt->sub, "history_file");
  if (!c_history_file)
    return;

  FILE *fp = mutt_file_fopen(c_history_file, "r");
  if (!fp)
    return;

  int line = 0, hclass = 0, read = 0;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;

  const char *const c_charset = cc_charset();
  while ((linebuf = mutt_file_read_line(linebuf, &buflen, fp, &line, MUTT_RL_NO_FLAGS)))
  {
    read = 0;
    if ((sscanf(linebuf, "%d:%n", &hclass, &read) < 1) || (read == 0) ||
        (*(p = linebuf + strlen(linebuf) - 1) != '|') || (hclass < 0))
    {
      mutt_error(_("%s:%d: Bad history file format"), c_history_file, line);
      continue;
    }
    /* silently ignore too high class (probably newer neomutt) */
    if (hclass >= HC_MAX)
      continue;
    *p = '\0';
    p = mutt_str_dup(linebuf + read);
    if (p)
    {
      mutt_ch_convert_string(&p, "utf-8", c_charset, MUTT_ICONV_NO_FLAGS);
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

/**
 * mutt_hist_complete - Complete a string from a history list
 * @param buf    Buffer in which to save string
 * @param buflen Buffer length
 * @param hclass History list to use
 */
void mutt_hist_complete(char *buf, size_t buflen, enum HistoryClass hclass)
{
  const short c_history = cs_subset_number(NeoMutt->sub, "history");
  char **matches = MUTT_MEM_CALLOC(c_history, char *);
  int match_count = mutt_hist_search(buf, hclass, matches);
  if (match_count)
  {
    if (match_count == 1)
      mutt_str_copy(buf, matches[0], buflen);
    else
      dlg_history(buf, buflen, matches, match_count);
  }
  FREE(&matches);
}

/**
 * main_hist_observer - Notification that a Config Variable has change - Implements ::observer_t - @ingroup observer_api
 */
int main_hist_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "history"))
    return 0;

  mutt_hist_init();
  mutt_debug(LL_DEBUG5, "history done\n");
  return 0;
}
