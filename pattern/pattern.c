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
#include "email/lib.h"
#include "core/lib.h"
#include "alias/gui.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "context.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "progress.h"
#include "protos.h"
#ifndef USE_FMEMOPEN
#include <sys/stat.h>
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

// clang-format off
/**
 * range_regexes - Set of Regexes for various range types
 *
 * This array, will also contain the compiled regexes.
 */
struct RangeRegex range_regexes[] = {
  [RANGE_K_REL]  = { RANGE_REL_RX,  1, 3, 0, { 0 } },
  [RANGE_K_ABS]  = { RANGE_ABS_RX,  1, 3, 0, { 0 } },
  [RANGE_K_LT]   = { RANGE_LT_RX,   1, 2, 0, { 0 } },
  [RANGE_K_GT]   = { RANGE_GT_RX,   2, 1, 0, { 0 } },
  [RANGE_K_BARE] = { RANGE_BARE_RX, 1, 1, 0, { 0 } },
};
// clang-format on

/**
 * eat_arg_t - Function to parse a pattern
 * @param pat   Pattern to store the results in
 * @param flags Flags, e.g. #MUTT_PC_PATTERN_DYNAMIC
 * @param s     String to parse
 * @param err   Buffer for error messages
 * @retval true If the pattern was read successfully
 */
bool (*eat_arg_t)(struct Pattern *pat, int flags, struct Buffer *s, struct Buffer *err);

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
  mutt_buffer_reset(buf);
  mutt_buffer_addch(buf, '"');
  while (*str)
  {
    if ((*str == '\\') || (*str == '"'))
      mutt_buffer_addch(buf, '\\');
    mutt_buffer_addch(buf, *str++);
  }
  mutt_buffer_addch(buf, '"');
}

/**
 * mutt_check_simple - Convert a simple search into a real request
 * @param buf    Buffer for the result
 * @param simple Search string to convert
 */
void mutt_check_simple(struct Buffer *buf, const char *simple)
{
  bool do_simple = true;

  for (const char *p = mutt_b2s(buf); p && (p[0] != '\0'); p++)
  {
    if ((p[0] == '\\') && (p[1] != '\0'))
      p++;
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
    if (mutt_istr_equal("all", mutt_b2s(buf)) || mutt_str_equal("^", mutt_b2s(buf)) ||
        mutt_str_equal(".", mutt_b2s(buf))) /* ~A is more efficient */
    {
      mutt_buffer_strcpy(buf, "~A");
    }
    else if (mutt_istr_equal("del", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~D");
    else if (mutt_istr_equal("flag", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~F");
    else if (mutt_istr_equal("new", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~N");
    else if (mutt_istr_equal("old", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~O");
    else if (mutt_istr_equal("repl", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~Q");
    else if (mutt_istr_equal("read", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~R");
    else if (mutt_istr_equal("tag", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~T");
    else if (mutt_istr_equal("unread", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~U");
    else
    {
      struct Buffer *tmp = mutt_buffer_pool_get();
      quote_simple(mutt_b2s(buf), tmp);
      mutt_file_expand_fmt(buf, simple, mutt_b2s(tmp));
      mutt_buffer_pool_release(&tmp);
    }
  }
}

/**
 * top_of_thread - Find the first email in the current thread
 * @param e Current Email
 * @retval ptr  Success, email found
 * @retval NULL Error
 */
static struct MuttThread *top_of_thread(struct Email *e)
{
  if (!e)
    return NULL;

  struct MuttThread *t = e->thread;

  while (t && t->parent)
    t = t->parent;

  return t;
}

/**
 * mutt_limit_current_thread - Limit the email view to the current thread
 * @param e Current Email
 * @retval true Success
 * @retval false Failure
 */
bool mutt_limit_current_thread(struct Email *e)
{
  struct Mailbox *m = ctx_mailbox(Context);
  if (!e || !m)
    return false;

  struct MuttThread *me = top_of_thread(e);
  if (!me)
    return false;

  m->vcount = 0;
  Context->vsize = 0;
  Context->collapsed = false;

  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      break;

    e->vnum = -1;
    e->visible = false;
    e->collapsed = false;
    e->num_hidden = 0;

    if (top_of_thread(e) == me)
    {
      struct Body *body = e->body;

      e->vnum = m->vcount;
      e->visible = true;
      m->v2r[m->vcount] = i;
      m->vcount++;
      Context->vsize += (body->length + body->offset - body->hdr_offset);
    }
  }
  return true;
}

/**
 * mutt_pattern_alias_func - Perform some Pattern matching for Alias
 * @param op        Operation to perform, e.g. #MUTT_LIMIT
 * @param prompt    Prompt to show the user
 * @param menu_name Name of the current menu, e.g. Aliases, Query
 * @param mdata     Menu data holding Aliases
 * @param menu      Current menu
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_pattern_alias_func(int op, char *prompt, char *menu_name,
                            struct AliasMenuData *mdata, struct Menu *menu)
{
  int rc = -1;
  struct Progress progress;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, mdata->str);
  if (prompt)
  {
    if ((mutt_buffer_get_field(prompt, buf, MUTT_PATTERN | MUTT_CLEAR) != 0) ||
        mutt_buffer_is_empty(buf))
    {
      mutt_buffer_pool_release(&buf);
      return -1;
    }
  }

  mutt_message(_("Compiling search pattern..."));

  bool match_all = false;
  struct PatternList *pat = NULL;
  char *simple = mutt_buffer_strdup(buf);
  if (simple)
  {
    mutt_check_simple(buf, MUTT_ALIAS_SIMPLESEARCH);
    const char *pbuf = buf->data;
    while (*pbuf == ' ')
      pbuf++;
    match_all = mutt_str_equal(pbuf, "~A");

    struct Buffer err = mutt_buffer_make(0);
    pat = mutt_pattern_comp(buf->data, MUTT_PC_FULL_MSG, &err);
    if (!pat)
    {
      mutt_error("%s", mutt_b2s(&err));
      mutt_buffer_dealloc(&err);
      goto bail;
    }
  }
  else
  {
    match_all = true;
  }

  mutt_progress_init(&progress, _("Executing command on matching messages..."),
                     MUTT_PROGRESS_READ, ARRAY_SIZE(&mdata->ava));

  int vcounter = 0;
  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, &mdata->ava)
  {
    mutt_progress_update(&progress, ARRAY_FOREACH_IDX, -1);

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

  mutt_str_replace(&mdata->str, simple);

  if (menu)
  {
    menu->max = vcounter;
    menu->current = 0;

    FREE(&menu->title);

    if (match_all)
      menu->title = menu_create_alias_title(menu_name, NULL);
    else
      menu->title = menu_create_alias_title(menu_name, simple);
  }

  mutt_clear_error();

  rc = 0;

bail:
  mutt_buffer_pool_release(&buf);
  FREE(&simple);
  mutt_pattern_free(&pat);

  return rc;
}

/**
 * mutt_pattern_func - Perform some Pattern matching
 * @param op     Operation to perform, e.g. #MUTT_LIMIT
 * @param prompt Prompt to show the user
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_pattern_func(int op, char *prompt)
{
  struct Mailbox *m = ctx_mailbox(Context);
  if (!m)
    return -1;

  struct Buffer err;
  int rc = -1;
  struct Progress progress;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, NONULL(Context->pattern));
  if (prompt || (op != MUTT_LIMIT))
  {
    if ((mutt_buffer_get_field(prompt, buf, MUTT_PATTERN | MUTT_CLEAR) != 0) ||
        mutt_buffer_is_empty(buf))
    {
      mutt_buffer_pool_release(&buf);
      return -1;
    }
  }

  mutt_message(_("Compiling search pattern..."));

  char *simple = mutt_buffer_strdup(buf);
  mutt_check_simple(buf, NONULL(C_SimpleSearch));
  const char *pbuf = buf->data;
  while (*pbuf == ' ')
    pbuf++;
  const bool match_all = mutt_str_equal(pbuf, "~A");

  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_malloc(err.dsize);
  struct PatternList *pat = mutt_pattern_comp(buf->data, MUTT_PC_FULL_MSG, &err);
  if (!pat)
  {
    mutt_error("%s", err.data);
    goto bail;
  }

#ifdef USE_IMAP
  if ((m->type == MUTT_IMAP) && (!imap_search(m, pat)))
    goto bail;
#endif

  mutt_progress_init(&progress, _("Executing command on matching messages..."),
                     MUTT_PROGRESS_READ, (op == MUTT_LIMIT) ? m->msg_count : m->vcount);

  if (op == MUTT_LIMIT)
  {
    m->vcount = 0;
    Context->vsize = 0;
    Context->collapsed = false;
    int padding = mx_msg_padding_size(m);

    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      mutt_progress_update(&progress, i, -1);
      /* new limit pattern implicitly uncollapses all threads */
      e->vnum = -1;
      e->visible = false;
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
        Context->vsize += b->length + b->offset - b->hdr_offset + padding;
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
      mutt_progress_update(&progress, i, -1);
      if (mutt_pattern_exec(SLIST_FIRST(pat), MUTT_MATCH_FULL_ADDRESS, m, e, NULL))
      {
        switch (op)
        {
          case MUTT_UNDELETE:
            mutt_set_flag(m, e, MUTT_PURGE, false);
          /* fallthrough */
          case MUTT_DELETE:
            mutt_set_flag(m, e, MUTT_DELETE, (op == MUTT_DELETE));
            break;
          case MUTT_TAG:
          case MUTT_UNTAG:
            mutt_set_flag(m, e, MUTT_TAG, (op == MUTT_TAG));
            break;
        }
      }
    }
  }

  mutt_clear_error();

  if (op == MUTT_LIMIT)
  {
    /* drop previous limit pattern */
    FREE(&Context->pattern);
    mutt_pattern_free(&Context->limit_pattern);

    if (m->msg_count && !m->vcount)
      mutt_error(_("No messages matched criteria"));

    /* record new limit pattern, unless match all */
    if (!match_all)
    {
      Context->pattern = simple;
      simple = NULL; /* don't clobber it */
      Context->limit_pattern = mutt_pattern_comp(buf->data, MUTT_PC_FULL_MSG, &err);
    }
  }

  rc = 0;

bail:
  mutt_buffer_pool_release(&buf);
  FREE(&simple);
  mutt_pattern_free(&pat);
  FREE(&err.data);

  return rc;
}

/**
 * mutt_search_command - Perform a search
 * @param m   Mailbox to search through
 * @param cur Index number of current email
 * @param op  Operation to perform, e.g. OP_SEARCH_NEXT
 * @retval >= 0 Index of matching email
 * @retval -1 No match, or error
 */
int mutt_search_command(struct Mailbox *m, int cur, int op)
{
  struct Progress progress;

  if ((*LastSearch == '\0') || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    char buf[256];
    mutt_str_copy(buf, (LastSearch[0] != '\0') ? LastSearch : "", sizeof(buf));
    if ((mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                            _("Search for: ") :
                            _("Reverse search for: "),
                        buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN) != 0) ||
        (buf[0] == '\0'))
    {
      return -1;
    }

    if ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT))
      OptSearchReverse = false;
    else
      OptSearchReverse = true;

    /* compare the *expanded* version of the search pattern in case
     * $simple_search has changed while we were searching */
    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_buffer_strcpy(tmp, buf);
    mutt_check_simple(tmp, NONULL(C_SimpleSearch));

    if (!SearchPattern || !mutt_str_equal(mutt_b2s(tmp), LastSearchExpn))
    {
      struct Buffer err;
      mutt_buffer_init(&err);
      OptSearchInvalid = true;
      mutt_str_copy(LastSearch, buf, sizeof(LastSearch));
      mutt_str_copy(LastSearchExpn, mutt_b2s(tmp), sizeof(LastSearchExpn));
      mutt_message(_("Compiling search pattern..."));
      mutt_pattern_free(&SearchPattern);
      err.dsize = 256;
      err.data = mutt_mem_malloc(err.dsize);
      SearchPattern = mutt_pattern_comp(tmp->data, MUTT_PC_FULL_MSG, &err);
      if (!SearchPattern)
      {
        mutt_buffer_pool_release(&tmp);
        mutt_error("%s", err.data);
        FREE(&err.data);
        LastSearch[0] = '\0';
        LastSearchExpn[0] = '\0';
        return -1;
      }
      FREE(&err.data);
      mutt_clear_error();
    }

    mutt_buffer_pool_release(&tmp);
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

  mutt_progress_init(&progress, _("Searching..."), MUTT_PROGRESS_READ, m->vcount);

  for (int i = cur + incr, j = 0; j != m->vcount; j++)
  {
    const char *msg = NULL;
    mutt_progress_update(&progress, j, -1);
    if (i > m->vcount - 1)
    {
      i = 0;
      if (C_WrapSearch)
        msg = _("Search wrapped to top");
      else
      {
        mutt_message(_("Search hit bottom without finding match"));
        return -1;
      }
    }
    else if (i < 0)
    {
      i = m->vcount - 1;
      if (C_WrapSearch)
        msg = _("Search wrapped to bottom");
      else
      {
        mutt_message(_("Search hit top without finding match"));
        return -1;
      }
    }

    struct Email *e = mutt_get_virt_email(m, i);
    if (e->searched)
    {
      /* if we've already evaluated this message, use the cached value */
      if (e->matched)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message(msg);
        return i;
      }
    }
    else
    {
      /* remember that we've already searched this message */
      e->searched = true;
      e->matched = mutt_pattern_exec(SLIST_FIRST(SearchPattern),
                                     MUTT_MATCH_FULL_ADDRESS, m, e, NULL);
      if (e->matched > 0)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message(msg);
        return i;
      }
    }

    if (SigInt)
    {
      mutt_error(_("Search interrupted"));
      SigInt = 0;
      return -1;
    }

    i += incr;
  }

  mutt_error(_("Not found"));
  return -1;
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
  struct Progress progress;

  struct AliasViewArray *ava = &((struct AliasMenuData *) menu->mdata)->ava;

  if ((*LastSearch == '\0') || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    char buf[256];
    mutt_str_copy(buf, (LastSearch[0] != '\0') ? LastSearch : "", sizeof(buf));
    if ((mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                            _("Search for: ") :
                            _("Reverse search for: "),
                        buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN) != 0) ||
        (buf[0] == '\0'))
    {
      return -1;
    }

    if ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT))
      OptSearchReverse = false;
    else
      OptSearchReverse = true;

    /* compare the *expanded* version of the search pattern in case
     * $simple_search has changed while we were searching */
    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_buffer_strcpy(tmp, buf);
    mutt_check_simple(tmp, MUTT_ALIAS_SIMPLESEARCH);

    if (!SearchPattern || !mutt_str_equal(mutt_b2s(tmp), LastSearchExpn))
    {
      struct Buffer err;
      mutt_buffer_init(&err);
      OptSearchInvalid = true;
      mutt_str_copy(LastSearch, buf, sizeof(LastSearch));
      mutt_str_copy(LastSearchExpn, mutt_b2s(tmp), sizeof(LastSearchExpn));
      mutt_message(_("Compiling search pattern..."));
      mutt_pattern_free(&SearchPattern);
      err.dsize = 256;
      err.data = mutt_mem_malloc(err.dsize);
      SearchPattern = mutt_pattern_comp(tmp->data, MUTT_PC_FULL_MSG, &err);
      if (!SearchPattern)
      {
        mutt_buffer_pool_release(&tmp);
        mutt_error("%s", err.data);
        FREE(&err.data);
        LastSearch[0] = '\0';
        LastSearchExpn[0] = '\0';
        return -1;
      }
      FREE(&err.data);
      mutt_clear_error();
    }

    mutt_buffer_pool_release(&tmp);
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

  mutt_progress_init(&progress, _("Searching..."), MUTT_PROGRESS_READ, ARRAY_SIZE(ava));

  for (int i = cur + incr, j = 0; j != ARRAY_SIZE(ava); j++)
  {
    const char *msg = NULL;
    mutt_progress_update(&progress, j, -1);
    if (i > ARRAY_SIZE(ava) - 1)
    {
      i = 0;
      if (C_WrapSearch)
        msg = _("Search wrapped to top");
      else
      {
        mutt_message(_("Search hit bottom without finding match"));
        return -1;
      }
    }
    else if (i < 0)
    {
      i = ARRAY_SIZE(ava) - 1;
      if (C_WrapSearch)
        msg = _("Search wrapped to bottom");
      else
      {
        mutt_message(_("Search hit top without finding match"));
        return -1;
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
          mutt_message(msg);
        return i;
      }
    }
    else
    {
      /* remember that we've already searched this message */
      av->is_searched = true;
      av->is_matched = mutt_pattern_alias_exec(SLIST_FIRST(SearchPattern),
                                               MUTT_MATCH_FULL_ADDRESS, av, NULL);
      if (av->is_matched > 0)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message(msg);
        return i;
      }
    }

    if (SigInt)
    {
      mutt_error(_("Search interrupted"));
      SigInt = 0;
      return -1;
    }

    i += incr;
  }

  mutt_error(_("Not found"));
  return -1;
}
