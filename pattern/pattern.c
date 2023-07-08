/**
 * @file
 * Match patterns to emails
 *
 * @authors
 * Copyright (C) 1996-2000,2006-2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page pattern_pattern Match patterns to emails
 *
 * Match patterns to emails
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/gui.h" // IWYU pragma: keep
#include "alias/lib.h"
#include "mutt.h"
#include "lib.h"
#include "enter/lib.h"
#include "menu/lib.h"
#include "progress/lib.h"
#include "globals.h" // IWYU pragma: keep
#include "mutt_logging.h"
#include "mview.h"
#include "mx.h"
#include "opcodes.h"
#include "protos.h"
#ifndef USE_FMEMOPEN
#include <sys/stat.h>
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/**
 * RangeRegexes - Set of Regexes for various range types
 *
 * This array, will also contain the compiled regexes.
 */
struct RangeRegex RangeRegexes[] = {
  // clang-format off
  [RANGE_K_REL]  = { RANGE_REL_RX,  1, 3, 0, { 0 } },
  [RANGE_K_ABS]  = { RANGE_ABS_RX,  1, 3, 0, { 0 } },
  [RANGE_K_LT]   = { RANGE_LT_RX,   1, 2, 0, { 0 } },
  [RANGE_K_GT]   = { RANGE_GT_RX,   2, 1, 0, { 0 } },
  [RANGE_K_BARE] = { RANGE_BARE_RX, 1, 1, 0, { 0 } },
  // clang-format on
};

/**
 * @defgroup eat_arg_api Parse a pattern API
 *
 * Prototype for a function to parse a pattern
 *
 * @param pat   Pattern to store the results in
 * @param flags Flags, e.g. #MUTT_PC_PATTERN_DYNAMIC
 * @param s     String to parse
 * @param err   Buffer for error messages
 * @retval true The pattern was read successfully
 */
typedef bool (*eat_arg_t)(struct Pattern *pat, PatternCompFlags flags,
                          struct Buffer *s, struct Buffer *err);

static struct PatternList *SearchPattern = NULL; ///< current search pattern
static char LastSearch[256] = { 0 };             ///< last pattern searched for
static char LastSearchExpn[1024] = { 0 }; ///< expanded version of LastSearch

/**
 * quote_simple - Apply simple quoting to a string
 * @param str    String to quote
 * @param buf    Buffer for the result
 */
static void quote_simple(const char *str, struct Buffer *buf)
{
  buf_reset(buf);
  buf_addch(buf, '"');
  while (*str)
  {
    if ((*str == '\\') || (*str == '"'))
      buf_addch(buf, '\\');
    buf_addch(buf, *str++);
  }
  buf_addch(buf, '"');
}

/**
 * mutt_check_simple - Convert a simple search into a real request
 * @param buf    Buffer for the result
 * @param simple Search string to convert
 */
void mutt_check_simple(struct Buffer *buf, const char *simple)
{
  bool do_simple = true;

  for (const char *p = buf_string(buf); p && (p[0] != '\0'); p++)
  {
    if ((p[0] == '\\') && (p[1] != '\0'))
    {
      p++;
    }
    else if ((p[0] == '~') || (p[0] == '=') || (p[0] == '%'))
    {
      do_simple = false;
      break;
    }
  }

  /* XXX - is mutt_istr_cmp() right here, or should we use locale's
   * equivalences?  */

  if (do_simple) /* yup, so spoof a real request */
  {
    /* convert old tokens into the new format */
    if (mutt_istr_equal("all", buf_string(buf)) || mutt_str_equal("^", buf_string(buf)) ||
        mutt_str_equal(".", buf_string(buf))) /* ~A is more efficient */
    {
      buf_strcpy(buf, "~A");
    }
    else if (mutt_istr_equal("del", buf_string(buf)))
    {
      buf_strcpy(buf, "~D");
    }
    else if (mutt_istr_equal("flag", buf_string(buf)))
    {
      buf_strcpy(buf, "~F");
    }
    else if (mutt_istr_equal("new", buf_string(buf)))
    {
      buf_strcpy(buf, "~N");
    }
    else if (mutt_istr_equal("old", buf_string(buf)))
    {
      buf_strcpy(buf, "~O");
    }
    else if (mutt_istr_equal("repl", buf_string(buf)))
    {
      buf_strcpy(buf, "~Q");
    }
    else if (mutt_istr_equal("read", buf_string(buf)))
    {
      buf_strcpy(buf, "~R");
    }
    else if (mutt_istr_equal("tag", buf_string(buf)))
    {
      buf_strcpy(buf, "~T");
    }
    else if (mutt_istr_equal("unread", buf_string(buf)))
    {
      buf_strcpy(buf, "~U");
    }
    else
    {
      struct Buffer *tmp = buf_pool_get();
      quote_simple(buf_string(buf), tmp);
      mutt_file_expand_fmt(buf, simple, buf_string(tmp));
      buf_pool_release(&tmp);
    }
  }
}

/**
 * mutt_pattern_alias_func - Perform some Pattern matching for Alias
 * @param prompt    Prompt to show the user
 * @param mdata     Menu data holding Aliases
 * @param menu      Current menu
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_pattern_alias_func(char *prompt, struct AliasMenuData *mdata, struct Menu *menu)
{
  int rc = -1;
  struct Progress *progress = NULL;
  struct Buffer *buf = buf_pool_get();

  buf_strcpy(buf, mdata->limit);
  if (prompt)
  {
    if ((mw_get_field(prompt, buf, MUTT_COMP_PATTERN | MUTT_COMP_CLEAR, false,
                      NULL, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      buf_pool_release(&buf);
      return -1;
    }
  }

  mutt_message(_("Compiling search pattern..."));

  bool match_all = false;
  struct PatternList *pat = NULL;
  char *simple = buf_strdup(buf);
  if (simple)
  {
    mutt_check_simple(buf, MUTT_ALIAS_SIMPLESEARCH);
    const char *pbuf = buf->data;
    while (*pbuf == ' ')
      pbuf++;
    match_all = mutt_str_equal(pbuf, "~A");

    struct Buffer err = buf_make(0);
    pat = mutt_pattern_comp(NULL, menu, buf->data, MUTT_PC_FULL_MSG, &err);
    if (!pat)
    {
      mutt_error("%s", buf_string(&err));
      buf_dealloc(&err);
      goto bail;
    }
  }
  else
  {
    match_all = true;
  }

  progress = progress_new(_("Executing command on matching messages..."),
                          MUTT_PROGRESS_READ, ARRAY_SIZE(&mdata->ava));

  int vcounter = 0;
  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, &mdata->ava)
  {
    progress_update(progress, ARRAY_FOREACH_IDX, -1);

    if (match_all ||
        mutt_pattern_alias_exec(SLIST_FIRST(pat), MUTT_MATCH_FULL_ADDRESS, avp, NULL))
    {
      avp->is_visible = true;
      vcounter++;
    }
    else
    {
      avp->is_visible = false;
    }
  }
  progress_free(&progress);

  FREE(&mdata->limit);
  if (!match_all)
  {
    mdata->limit = simple;
    simple = NULL;
  }

  if (menu)
  {
    menu->max = vcounter;
    menu_set_index(menu, 0);
  }

  mutt_clear_error();

  rc = 0;

bail:
  buf_pool_release(&buf);
  FREE(&simple);
  mutt_pattern_free(&pat);

  return rc;
}

/**
 * mutt_pattern_func - Perform some Pattern matching
 * @param mv     Mailbox View
 * @param op     Operation to perform, e.g. #MUTT_LIMIT
 * @param prompt Prompt to show the user
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_pattern_func(struct MailboxView *mv, int op, char *prompt)
{
  if (!mv || !mv->mailbox)
    return -1;

  struct Mailbox *m = mv->mailbox;

  struct Buffer *err = NULL;
  int rc = -1;
  struct Progress *progress = NULL;
  struct Buffer *buf = buf_pool_get();

  buf_strcpy(buf, NONULL(mv->pattern));
  if (prompt || (op != MUTT_LIMIT))
  {
    if ((mw_get_field(prompt, buf, MUTT_COMP_PATTERN | MUTT_COMP_CLEAR, false,
                      NULL, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      buf_pool_release(&buf);
      return -1;
    }
  }

  mutt_message(_("Compiling search pattern..."));

  char *simple = buf_strdup(buf);
  const char *const c_simple_search = cs_subset_string(NeoMutt->sub, "simple_search");
  mutt_check_simple(buf, NONULL(c_simple_search));
  const char *pbuf = buf->data;
  while (*pbuf == ' ')
    pbuf++;
  const bool match_all = mutt_str_equal(pbuf, "~A");

  err = buf_pool_get();
  struct PatternList *pat = mutt_pattern_comp(mv, mv->menu, buf->data, MUTT_PC_FULL_MSG, err);
  if (!pat)
  {
    mutt_error("%s", buf_string(err));
    goto bail;
  }

#ifdef USE_IMAP
  if ((m->type == MUTT_IMAP) && (!imap_search(m, pat)))
    goto bail;
#endif

  progress = progress_new(_("Executing command on matching messages..."), MUTT_PROGRESS_READ,
                          (op == MUTT_LIMIT) ? m->msg_count : m->vcount);

  if (op == MUTT_LIMIT)
  {
    m->vcount = 0;
    mv->vsize = 0;
    mv->collapsed = false;
    int padding = mx_msg_padding_size(m);

    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      progress_update(progress, i, -1);
      /* new limit pattern implicitly uncollapses all threads */
      e->vnum = -1;
      e->visible = false;
      e->limit_visited = true;
      e->collapsed = false;
      e->num_hidden = 0;

      if (match_all ||
          mutt_pattern_exec(SLIST_FIRST(pat), MUTT_MATCH_FULL_ADDRESS, m, e, NULL))
      {
        e->vnum = m->vcount;
        e->visible = true;
        m->v2r[m->vcount] = i;
        m->vcount++;
        struct Body *b = e->body;
        mv->vsize += b->length + b->offset - b->hdr_offset + padding;
      }
    }
  }
  else
  {
    for (int i = 0; i < m->vcount; i++)
    {
      struct Email *e = mutt_get_virt_email(m, i);
      if (!e)
        continue;
      progress_update(progress, i, -1);
      if (mutt_pattern_exec(SLIST_FIRST(pat), MUTT_MATCH_FULL_ADDRESS, m, e, NULL))
      {
        switch (op)
        {
          case MUTT_UNDELETE:
            mutt_set_flag(m, e, MUTT_PURGE, false, true);
          /* fallthrough */
          case MUTT_DELETE:
            mutt_set_flag(m, e, MUTT_DELETE, (op == MUTT_DELETE), true);
            break;
          case MUTT_TAG:
          case MUTT_UNTAG:
            mutt_set_flag(m, e, MUTT_TAG, (op == MUTT_TAG), true);
            break;
        }
      }
    }
  }
  progress_free(&progress);

  mutt_clear_error();

  if (op == MUTT_LIMIT)
  {
    /* drop previous limit pattern */
    FREE(&mv->pattern);
    mutt_pattern_free(&mv->limit_pattern);

    if (m->msg_count && !m->vcount)
      mutt_error(_("No messages matched criteria"));

    /* record new limit pattern, unless match all */
    if (!match_all)
    {
      mv->pattern = simple;
      simple = NULL; /* don't clobber it */
      mv->limit_pattern = mutt_pattern_comp(mv, mv->menu, buf->data, MUTT_PC_FULL_MSG, err);
    }
  }

  rc = 0;

bail:
  buf_pool_release(&buf);
  buf_pool_release(&err);
  FREE(&simple);
  mutt_pattern_free(&pat);

  return rc;
}

/**
 * mutt_search_command - Perform a search
 * @param mv   Mailbox view to search through
 * @param menu Current Menu
 * @param cur  Index number of current email
 * @param op   Operation to perform, e.g. OP_SEARCH_NEXT
 * @retval >=0 Index of matching email
 * @retval -1  No match, or error
 */
int mutt_search_command(struct MailboxView *mv, struct Menu *menu, int cur, int op)
{
  struct Progress *progress = NULL;
  struct Buffer *buf = NULL;
  int rc = -1;
  struct Mailbox *m = mv ? mv->mailbox : NULL;
  if (!m)
    return -1;

  if ((*LastSearch == '\0') || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    buf = buf_pool_get();
    buf_strcpy(buf, (LastSearch[0] != '\0') ? LastSearch : "");
    if ((mw_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? _("Search for: ") : _("Reverse search for: "),
                      buf, MUTT_COMP_CLEAR | MUTT_COMP_PATTERN, false, NULL, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      goto done;
    }

    if ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT))
      OptSearchReverse = false;
    else
      OptSearchReverse = true;

    /* compare the *expanded* version of the search pattern in case
     * $simple_search has changed while we were searching */
    struct Buffer *tmp = buf_pool_get();
    buf_copy(tmp, buf);
    const char *const c_simple_search = cs_subset_string(NeoMutt->sub, "simple_search");
    mutt_check_simple(tmp, NONULL(c_simple_search));

    if (!SearchPattern || !mutt_str_equal(buf_string(tmp), LastSearchExpn))
    {
      struct Buffer err;
      buf_init(&err);
      OptSearchInvalid = true;
      mutt_str_copy(LastSearch, buf_string(buf), sizeof(LastSearch));
      mutt_str_copy(LastSearchExpn, buf_string(tmp), sizeof(LastSearchExpn));
      mutt_message(_("Compiling search pattern..."));
      mutt_pattern_free(&SearchPattern);
      err.dsize = 256;
      err.data = mutt_mem_malloc(err.dsize);
      SearchPattern = mutt_pattern_comp(mv, menu, tmp->data, MUTT_PC_FULL_MSG, &err);
      if (!SearchPattern)
      {
        buf_pool_release(&buf);
        buf_pool_release(&tmp);
        mutt_error("%s", err.data);
        FREE(&err.data);
        LastSearch[0] = '\0';
        LastSearchExpn[0] = '\0';
        return -1;
      }
      FREE(&err.data);
      mutt_clear_error();
    }

    buf_pool_release(&tmp);
  }

  if (OptSearchInvalid)
  {
    for (int i = 0; i < m->msg_count; i++)
      m->emails[i]->searched = false;
#ifdef USE_IMAP
    if ((m->type == MUTT_IMAP) && (!imap_search(m, SearchPattern)))
      return -1;
#endif
    OptSearchInvalid = false;
  }

  int incr = OptSearchReverse ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    incr = -incr;

  progress = progress_new(_("Searching..."), MUTT_PROGRESS_READ, m->vcount);

  const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
  for (int i = cur + incr, j = 0; j != m->vcount; j++)
  {
    const char *msg = NULL;
    progress_update(progress, j, -1);
    if (i > m->vcount - 1)
    {
      i = 0;
      if (c_wrap_search)
      {
        msg = _("Search wrapped to top");
      }
      else
      {
        mutt_message(_("Search hit bottom without finding match"));
        goto done;
      }
    }
    else if (i < 0)
    {
      i = m->vcount - 1;
      if (c_wrap_search)
      {
        msg = _("Search wrapped to bottom");
      }
      else
      {
        mutt_message(_("Search hit top without finding match"));
        goto done;
      }
    }

    struct Email *e = mutt_get_virt_email(m, i);
    if (!e)
      goto done;

    if (e->searched)
    {
      /* if we've already evaluated this message, use the cached value */
      if (e->matched)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message("%s", msg);
        rc = i;
        goto done;
      }
    }
    else
    {
      /* remember that we've already searched this message */
      e->searched = true;
      e->matched = mutt_pattern_exec(SLIST_FIRST(SearchPattern),
                                     MUTT_MATCH_FULL_ADDRESS, m, e, NULL);
      if (e->matched)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message("%s", msg);
        rc = i;
        goto done;
      }
    }

    if (SigInt)
    {
      mutt_error(_("Search interrupted"));
      SigInt = false;
      goto done;
    }

    i += incr;
  }

  mutt_error(_("Not found"));
done:
  progress_free(&progress);
  buf_pool_release(&buf);
  return rc;
}

/**
 * mutt_search_alias_command - Perform a search
 * @param menu Menu to search through
 * @param cur  Index number of current alias
 * @param op   Operation to perform, e.g. OP_SEARCH_NEXT
 * @retval >=0 Index of matching alias
 * @retval -1 No match, or error
 */
int mutt_search_alias_command(struct Menu *menu, int cur, int op)
{
  struct Progress *progress = NULL;
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasViewArray *ava = &mdata->ava;
  struct Buffer *buf = NULL;
  int rc = -1;

  if ((*LastSearch == '\0') || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    buf = buf_pool_get();
    buf_strcpy(buf, (LastSearch[0] != '\0') ? LastSearch : "");
    if ((mw_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? _("Search for: ") : _("Reverse search for: "),
                      buf, MUTT_COMP_CLEAR | MUTT_COMP_PATTERN, false, NULL, NULL, NULL) != 0) ||
        buf_is_empty(buf))
    {
      goto done;
    }

    if ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT))
      OptSearchReverse = false;
    else
      OptSearchReverse = true;

    /* compare the *expanded* version of the search pattern in case
     * $simple_search has changed while we were searching */
    struct Buffer *tmp = buf_pool_get();
    buf_copy(tmp, buf);
    mutt_check_simple(tmp, MUTT_ALIAS_SIMPLESEARCH);

    if (!SearchPattern || !mutt_str_equal(buf_string(tmp), LastSearchExpn))
    {
      struct Buffer err;
      buf_init(&err);
      OptSearchInvalid = true;
      mutt_str_copy(LastSearch, buf_string(buf), sizeof(LastSearch));
      mutt_str_copy(LastSearchExpn, buf_string(tmp), sizeof(LastSearchExpn));
      mutt_message(_("Compiling search pattern..."));
      mutt_pattern_free(&SearchPattern);
      err.dsize = 256;
      err.data = mutt_mem_malloc(err.dsize);
      SearchPattern = mutt_pattern_comp(NULL, menu, tmp->data, MUTT_PC_FULL_MSG, &err);
      if (!SearchPattern)
      {
        buf_pool_release(&tmp);
        mutt_error("%s", err.data);
        FREE(&err.data);
        LastSearch[0] = '\0';
        LastSearchExpn[0] = '\0';
        return -1;
      }
      FREE(&err.data);
      mutt_clear_error();
    }

    buf_pool_release(&tmp);
  }

  if (OptSearchInvalid)
  {
    struct AliasView *av = NULL;
    ARRAY_FOREACH(av, ava)
    {
      av->is_searched = false;
    }

    OptSearchInvalid = false;
  }

  int incr = OptSearchReverse ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    incr = -incr;

  progress = progress_new(_("Searching..."), MUTT_PROGRESS_READ, ARRAY_SIZE(ava));

  const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
  for (int i = cur + incr, j = 0; j != ARRAY_SIZE(ava); j++)
  {
    const char *msg = NULL;
    progress_update(progress, j, -1);
    if (i > ARRAY_SIZE(ava) - 1)
    {
      i = 0;
      if (c_wrap_search)
      {
        msg = _("Search wrapped to top");
      }
      else
      {
        mutt_message(_("Search hit bottom without finding match"));
        goto done;
      }
    }
    else if (i < 0)
    {
      i = ARRAY_SIZE(ava) - 1;
      if (c_wrap_search)
      {
        msg = _("Search wrapped to bottom");
      }
      else
      {
        mutt_message(_("Search hit top without finding match"));
        goto done;
      }
    }

    struct AliasView *av = ARRAY_GET(ava, i);
    if (av->is_searched)
    {
      /* if we've already evaluated this message, use the cached value */
      if (av->is_matched)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message("%s", msg);
        rc = i;
        goto done;
      }
    }
    else
    {
      /* remember that we've already searched this message */
      av->is_searched = true;
      av->is_matched = mutt_pattern_alias_exec(SLIST_FIRST(SearchPattern),
                                               MUTT_MATCH_FULL_ADDRESS, av, NULL);
      if (av->is_matched)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message("%s", msg);
        rc = i;
        goto done;
      }
    }

    if (SigInt)
    {
      mutt_error(_("Search interrupted"));
      SigInt = false;
      goto done;
    }

    i += incr;
  }

  mutt_error(_("Not found"));
done:
  progress_free(&progress);
  buf_pool_release(&buf);
  return rc;
}
