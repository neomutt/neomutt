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
 * @page history Read/write command history from/to a file
 *
 * Read/write command history from/to a file.
 *
 * This history ring grows from 0..History, with last marking the
 * where new entries go:
 * ```
 *         0        the oldest entry in the ring
 *         1        entry
 *         ...
 *         x-1      most recently entered text
 *  last-> x        NULL  (this will be overwritten next)
 *         x+1      NULL
 *         ...
 *         History  NULL
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
 *         History  entry
 * ```
 * When $history_remove_dups is set, duplicate entries are scanned and removed
 * each time a new entry is added.  In order to preserve the history ring size,
 * entries 0..last are compacted up.  Entries last+1..History are
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
 *         History  entry
 * ```
 */

#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "history.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"

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

static const struct Mapping HistoryHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Search"), OP_SEARCH },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

/* global vars used for the string-history routines */

short History;          /**< Number of history entries stored in memory */
char *HistoryFile;      /**< File in which to store all the histories */
bool HistoryRemoveDups; /**< Remove duplicate history entries */
short SaveHistory; /**< Number of history entries, per category, stored on disk */

static struct History Histories[HC_LAST];
static int OldSize = 0;

/**
 * get_history - Get a particular history
 * @param hclass Type of history to find
 * @retval ptr History ring buffer
 */
static struct History *get_history(enum HistoryClass hclass)
{
  if (hclass >= HC_LAST)
    return NULL;

  return &Histories[hclass];
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

  if (History != 0)
    h->hist = mutt_mem_calloc(History + 1, sizeof(char *));

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
static int dup_hash_dec(struct Hash *dup_hash, char *str)
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
static int dup_hash_inc(struct Hash *dup_hash, char *str)
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
  char tmpfname[_POSIX_PATH_MAX];
  FILE *tmp = NULL;
  int n[HC_LAST] = { 0 };
  int line, hclass, read;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;
  bool regen_file = false;
  struct Hash *dup_hashes[HC_LAST] = { 0 };

  FILE *f = fopen(HistoryFile, "r");
  if (!f)
    return;

  if (HistoryRemoveDups)
    for (hclass = 0; hclass < HC_LAST; hclass++)
      dup_hashes[hclass] = mutt_hash_create(MAX(10, SaveHistory * 2), MUTT_HASH_STRDUP_KEYS);

  line = 0;
  while ((linebuf = mutt_file_read_line(linebuf, &buflen, f, &line, 0)) != NULL)
  {
    if (sscanf(linebuf, "%d:%n", &hclass, &read) < 1 || read == 0 ||
        *(p = linebuf + strlen(linebuf) - 1) != '|' || hclass < 0)
    {
      mutt_error(_("Bad history file format (line %d)"), line);
      goto cleanup;
    }
    /* silently ignore too high class (probably newer neomutt) */
    if (hclass >= HC_LAST)
      continue;
    *p = '\0';
    if (HistoryRemoveDups && (dup_hash_inc(dup_hashes[hclass], linebuf + read) > 1))
    {
      regen_file = true;
      continue;
    }
    n[hclass]++;
  }

  if (!regen_file)
  {
    for (hclass = HC_FIRST; hclass < HC_LAST; hclass++)
    {
      if (n[hclass] > SaveHistory)
      {
        regen_file = true;
        break;
      }
    }
  }

  if (regen_file)
  {
    mutt_mktemp(tmpfname, sizeof(tmpfname));
    tmp = mutt_file_fopen(tmpfname, "w+");
    if (!tmp)
    {
      mutt_perror(tmpfname);
      goto cleanup;
    }
    rewind(f);
    line = 0;
    while ((linebuf = mutt_file_read_line(linebuf, &buflen, f, &line, 0)) != NULL)
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
      if (HistoryRemoveDups && (dup_hash_dec(dup_hashes[hclass], linebuf + read) > 0))
      {
        continue;
      }
      *p = '|';
      if (n[hclass]-- <= SaveHistory)
        fprintf(tmp, "%s\n", linebuf);
    }
  }

cleanup:
  mutt_file_fclose(&f);
  FREE(&linebuf);
  if (tmp)
  {
    if (fflush(tmp) == 0 && (f = fopen(HistoryFile, "w")) != NULL)
    {
      rewind(tmp);
      mutt_file_copy_stream(tmp, f);
      mutt_file_fclose(&f);
    }
    mutt_file_fclose(&tmp);
    unlink(tmpfname);
  }
  if (HistoryRemoveDups)
    for (hclass = 0; hclass < HC_LAST; hclass++)
      mutt_hash_destroy(&dup_hashes[hclass]);
}

/**
 * save_history - Save one history string to a file
 * @param hclass History type
 * @param str    String to save
 */
static void save_history(enum HistoryClass hclass, const char *str)
{
  static int n = 0;
  FILE *f = NULL;
  char *tmp = NULL;

  if (!str || !*str) /* This shouldn't happen, but it's safer. */
    return;

  f = fopen(HistoryFile, "a");
  if (!f)
  {
    mutt_perror("fopen");
    return;
  }

  tmp = mutt_str_strdup(str);
  mutt_ch_convert_string(&tmp, Charset, "utf-8", 0);

  /* Format of a history item (1 line): "<histclass>:<string>|".
   * We add a '|' in order to avoid lines ending with '\'. */
  fprintf(f, "%d:", (int) hclass);
  for (char *p = tmp; *p; p++)
  {
    /* Don't copy \n as a history item must fit on one line. The string
     * shouldn't contain such a character anyway, but as this can happen
     * in practice, we must deal with that. */
    if (*p != '\n')
      putc((unsigned char) *p, f);
  }
  fputs("|\n", f);

  mutt_file_fclose(&f);
  FREE(&tmp);

  if (--n < 0)
  {
    n = SaveHistory;
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
  int source, dest, old_last;
  struct History *h = get_history(hclass);

  if ((History == 0) || !h)
    return; /* disabled */

  /* Remove dups from 0..last-1 compacting up. */
  source = dest = 0;
  while (source < h->last)
  {
    if (mutt_str_strcmp(h->hist[source], str) == 0)
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

  /* Remove dups from last+1 .. History compacting down. */
  source = dest = History;
  while (source > old_last)
  {
    if (mutt_str_strcmp(h->hist[source], str) == 0)
      FREE(&h->hist[source--]);
    else
      h->hist[dest--] = h->hist[source--];
  }

  /* Fill in moved entries with NULL */
  while (dest > old_last)
    h->hist[dest--] = NULL;
}

/**
 * mutt_hist_init - Create a set of empty History ring buffers
 *
 * This just creates empty histories.
 * To fill them, call mutt_hist_read_file().
 */
void mutt_hist_init(void)
{
  if (History == OldSize)
    return;

  for (enum HistoryClass hclass = HC_FIRST; hclass < HC_LAST; hclass++)
    init_history(&Histories[hclass]);

  OldSize = History;
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

  if ((History == 0) || !h)
    return; /* disabled */

  if (*str)
  {
    int prev = h->last - 1;
    if (prev < 0)
      prev = History;

    /* don't add to prompt history:
     *  - lines beginning by a space
     *  - repeated lines
     */
    if ((*str != ' ') && (!h->hist[prev] || (mutt_str_strcmp(h->hist[prev], str) != 0)))
    {
      if (HistoryRemoveDups)
        remove_history_dups(hclass, str);
      if (save && (SaveHistory != 0))
        save_history(hclass, str);
      mutt_str_replace(&h->hist[h->last++], str);
      if (h->last > History)
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

  if ((History == 0) || !h)
    return ""; /* disabled */

  int next = h->cur;
  do
  {
    next++;
    if (next > History)
      next = 0;
    if (next == h->last)
      break;
  } while (h->hist[next] == NULL);

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

  if ((History == 0) || !h)
    return ""; /* disabled */

  int prev = h->cur;
  do
  {
    prev--;
    if (prev < 0)
      prev = History;
    if (prev == h->last)
      break;
  } while (h->hist[prev] == NULL);

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

  if ((History == 0) || !h)
    return; /* disabled */

  h->cur = h->last;
}

/**
 * mutt_hist_read_file - Read the History from a file
 *
 * The file #HistoryFile is read and parsed into separate History ring buffers.
 */
void mutt_hist_read_file(void)
{
  int line = 0, hclass, read;
  char *linebuf = NULL, *p = NULL;
  size_t buflen;

  FILE *f = fopen(HistoryFile, "r");
  if (!f)
    return;

  while ((linebuf = mutt_file_read_line(linebuf, &buflen, f, &line, 0)) != NULL)
  {
    read = 0;
    if (sscanf(linebuf, "%d:%n", &hclass, &read) < 1 || read == 0 ||
        *(p = linebuf + strlen(linebuf) - 1) != '|' || hclass < 0)
    {
      mutt_error(_("Bad history file format (line %d)"), line);
      break;
    }
    /* silently ignore too high class (probably newer neomutt) */
    if (hclass >= HC_LAST)
      continue;
    *p = '\0';
    p = mutt_str_strdup(linebuf + read);
    if (p)
    {
      mutt_ch_convert_string(&p, "utf-8", Charset, 0);
      mutt_hist_add(hclass, p, false);
      FREE(&p);
    }
  }

  mutt_file_fclose(&f);
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

  if (!History || !h)
    return false; /* disabled */

  return (h->cur == h->last);
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

  if ((History == 0) || !h)
    return; /* disabled */

  /* Don't check if str has a value because the scratch buffer may contain
   * an old garbage value that should be overwritten */
  mutt_str_replace(&h->hist[h->last], str);
}

static const char *history_format_str(char *dest, size_t destlen, size_t col, int cols,
                                      char op, const char *src, const char *fmt,
                                      const char *ifstring, const char *elsestring,
                                      unsigned long data, enum FormatFlag flags)
{
  char *match = (char *) data;

  switch (op)
  {
    case 's':
      mutt_format_s(dest, destlen, fmt, match);
      break;
  }

  return (src);
}

static void history_entry(char *s, size_t slen, struct Menu *m, int num)
{
  char *entry = ((char **) m->data)[num];

  mutt_expando_format(s, slen, 0, MuttIndexWindow->cols, "%s", history_format_str,
                      (unsigned long) entry, MUTT_FORMAT_ARROWCURSOR);
}

static void history_menu(char *buf, size_t buflen, char **matches, int match_count)
{
  struct Menu *menu;
  int done = 0;
  char helpstr[LONG_STRING];
  char title[STRING];

  snprintf(title, sizeof(title), _("History '%s'"), buf);

  menu = mutt_menu_new(MENU_GENERIC);
  menu->make_entry = history_entry;
  menu->title = title;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_GENERIC, HistoryHelp);
  mutt_menu_push_current(menu);

  menu->max = match_count;
  menu->data = matches;

  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
        mutt_str_strfcpy(buf, matches[menu->current], buflen);
        /* fall through */

      case OP_EXIT:
        done = 1;
        break;
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_destroy(&menu);
}

static int search_history(char *search_buf, enum HistoryClass hclass, char **matches)
{
  struct History *h = get_history(hclass);
  int match_count = 0, cur;

  if ((History == 0) || !h)
    return 0;

  cur = h->last;
  do
  {
    cur--;
    if (cur < 0)
      cur = History;
    if (cur == h->last)
      break;
    if (mutt_str_stristr(h->hist[cur], search_buf))
      matches[match_count++] = h->hist[cur];
  } while (match_count < History);

  return match_count;
}

void mutt_history_complete(char *buf, size_t buflen, enum HistoryClass hclass)
{
  char **matches;
  int match_count;

  matches = mutt_mem_calloc(History, sizeof(char *));
  match_count = search_history(buf, hclass, matches);
  if (match_count)
  {
    if (match_count == 1)
      mutt_str_strfcpy(buf, matches[0], buflen);
    else
      history_menu(buf, buflen, matches, match_count);
  }
  FREE(&matches);
}
