/**
 * @file
 * IMAP search routines
 *
 * @authors
 * Copyright (C) 1996-1998,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012,2017 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page imap_search IMAP search routines
 *
 * IMAP search routines
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "pattern.h"
#include "imap/lib.h"

// fwd decl, mutually recursive: check_pattern_list, check_pattern
static int check_pattern_list(const struct PatternList *patterns);

// fwd-decl, mutually recursive: compile_search, compile_search_children
static bool compile_search(const struct ImapAccountData *adata,
                           const struct Pattern *pat, struct Buffer *buf);

/**
 * check_pattern - Check whether a pattern can be searched server-side
 * @param pat Pattern to check
 * @retval true  Pattern can be searched server-side
 * @retval false Pattern cannot be searched server-side
 */
static bool check_pattern(const struct Pattern *pat)
{
  switch (pat->op)
  {
    case MUTT_PAT_BODY:
    case MUTT_PAT_HEADER:
    case MUTT_PAT_WHOLE_MSG:
      if (pat->string_match)
        return true;
      break;
    case MUTT_PAT_SERVERSEARCH:
      return true;
      break;
    default:
      if (pat->child && check_pattern_list(pat->child))
        return true;
      break;
  }
  return false;
}

/**
 * check_pattern_list - Check how many patterns in a list can be searched server-side
 * @param patterns List of patterns to match
 * @retval num Number of patterns search that can be searched server-side
 */
static int check_pattern_list(const struct PatternList *patterns)
{
  int positives = 0;

  const struct Pattern *pat = NULL;
  SLIST_FOREACH(pat, patterns, entries)
  {
    positives += check_pattern(pat);
  }

  return positives;
}

/**
 * compile_search_children - Compile a search command for a pattern's children
 * @param adata Imap Account data
 * @param pat Parent pattern
 * @param buf Buffer for the resulting command
 * @retval True on success
 * @retval False on failure
 */
static bool compile_search_children(const struct ImapAccountData *adata,
                                    const struct Pattern *pat, struct Buffer *buf)
{
  int clauses = check_pattern_list(pat->child);
  if (clauses == 0)
    return true;

  mutt_buffer_addch(buf, '(');

  struct Pattern *c;
  SLIST_FOREACH(c, pat->child, entries)
  {
    if (!check_pattern(c))
      continue;

    if ((pat->op == MUTT_PAT_OR) && (clauses > 1))
      mutt_buffer_addstr(buf, "OR ");

    if (!compile_search(adata, c, buf))
      return false;

    if (clauses > 1)
      mutt_buffer_addch(buf, ' ');

    clauses--;
  }

  mutt_buffer_addch(buf, ')');
  return true;
}

/**
 * compile_search_self - Compile a search command for a pattern
 * @param adata Imap Account data
 * @param pat Pattern
 * @param buf Buffer for the resulting command
 * @retval True on success
 * @retval False on failure
 */
static bool compile_search_self(const struct ImapAccountData *adata,
                                const struct Pattern *pat, struct Buffer *buf)
{
  char term[256];
  char *delim = NULL;

  switch (pat->op)
  {
    case MUTT_PAT_HEADER:
      mutt_buffer_addstr(buf, "HEADER ");

      /* extract header name */
      delim = strchr(pat->p.str, ':');
      if (!delim)
      {
        mutt_error(_("Header search without header name: %s"), pat->p.str);
        return false;
      }
      *delim = '\0';
      imap_quote_string(term, sizeof(term), pat->p.str, false);
      mutt_buffer_addstr(buf, term);
      mutt_buffer_addch(buf, ' ');

      /* and field */
      *delim = ':';
      delim++;
      SKIPWS(delim);
      imap_quote_string(term, sizeof(term), delim, false);
      mutt_buffer_addstr(buf, term);
      break;
    case MUTT_PAT_BODY:
      mutt_buffer_addstr(buf, "BODY ");
      imap_quote_string(term, sizeof(term), pat->p.str, false);
      mutt_buffer_addstr(buf, term);
      break;
    case MUTT_PAT_WHOLE_MSG:
      mutt_buffer_addstr(buf, "TEXT ");
      imap_quote_string(term, sizeof(term), pat->p.str, false);
      mutt_buffer_addstr(buf, term);
      break;
    case MUTT_PAT_SERVERSEARCH:
      if (!(adata->capabilities & IMAP_CAP_X_GM_EXT_1))
      {
        mutt_error(_("Server-side custom search not supported: %s"), pat->p.str);
        return false;
      }
      mutt_buffer_addstr(buf, "X-GM-RAW ");
      imap_quote_string(term, sizeof(term), pat->p.str, false);
      mutt_buffer_addstr(buf, term);
      break;
  }
  return true;
}

/**
 * compile_search - Convert NeoMutt pattern to IMAP search
 * @param adata Imap Account data
 * @param pat Pattern to convert
 * @param buf Buffer for result
 * @retval True on success
 * @retval False on failure
 *
 * Convert neomutt Pattern to IMAP SEARCH command containing only elements
 * that require full-text search (neomutt already has what it needs for most
 * match types, and does a better job (eg server doesn't support regexes).
 */
static bool compile_search(const struct ImapAccountData *adata,
                           const struct Pattern *pat, struct Buffer *buf)
{
  if (!check_pattern(pat))
    return true;

  if (pat->pat_not)
    mutt_buffer_addstr(buf, "NOT ");

  return pat->child ? compile_search_children(adata, pat, buf) :
                      compile_search_self(adata, pat, buf);
}

/**
 * imap_search - Find messages in mailbox matching a pattern
 * @param m   Mailbox
 * @param pat Pattern to match
 * @retval true  Success
 * @retval false Failure
 */
bool imap_search(struct Mailbox *m, const struct PatternList *pat)
{
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    e->matched = false;
  }

  if (check_pattern_list(pat) == 0)
    return true;

  struct Buffer buf;
  mutt_buffer_init(&buf);
  mutt_buffer_addstr(&buf, "UID SEARCH ");

  struct ImapAccountData *adata = imap_adata_get(m);
  const bool ok = compile_search(adata, SLIST_FIRST(pat), &buf) &&
                  (imap_exec(adata, buf.data, IMAP_CMD_NO_FLAGS) == IMAP_EXEC_SUCCESS);

  FREE(&buf.data);
  return ok;
}

/**
 * cmd_parse_search - store SEARCH response for later use
 * @param adata Imap Account data
 * @param s     Command string with search results
 */
void cmd_parse_search(struct ImapAccountData *adata, const char *s)
{
  unsigned int uid;
  struct Email *e = NULL;
  struct ImapMboxData *mdata = adata->mailbox->mdata;

  mutt_debug(LL_DEBUG2, "Handling SEARCH\n");

  while ((s = imap_next_word((char *) s)) && (*s != '\0'))
  {
    if (mutt_str_atoui(s, &uid) < 0)
      continue;
    e = mutt_hash_int_find(mdata->uid_hash, uid);
    if (e)
      e->matched = true;
  }
}
