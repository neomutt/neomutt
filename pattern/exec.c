/**
 * @file
 * Execute a Pattern
 *
 * @authors
 * Copyright (C) 2020 Romeu Vieira <romeu.bizz@gmail.com>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Leon Philman
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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
 * @page pattern_exec Execute a Pattern
 *
 * Execute a Pattern
 */

#include "config.h"
#include <stdarg.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/alias.h" // IWYU pragma: keep
#include "alias/gui.h"   // IWYU pragma: keep
#include "alias/lib.h"
#include "mutt.h"
#include "lib.h"
#include "attach/lib.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#include "copy.h"
#include "handler.h"
#include "maillist.h"
#include "mx.h"
#ifndef USE_FMEMOPEN
#include <sys/stat.h>
#endif

static bool pattern_exec(struct Pattern *pat, PatternExecFlags flags,
                         struct Mailbox *m, struct Email *e,
                         struct Message *msg, struct PatternCache *cache);

/**
 * patmatch - Compare a string to a Pattern
 * @param pat Pattern to use
 * @param buf String to compare
 * @retval true  Match
 * @retval false No match
 */
static bool patmatch(const struct Pattern *pat, const char *buf)
{
  if (pat->is_multi)
    return (mutt_list_find(&pat->p.multi_cases, buf) != NULL);
  if (pat->string_match)
    return pat->ign_case ? mutt_istr_find(buf, pat->p.str) : strstr(buf, pat->p.str);
  if (pat->group_match)
    return mutt_group_match(pat->p.group, buf);
  return (regexec(pat->p.regex, buf, 0, NULL, 0) == 0);
}

/**
 * print_crypt_pattern_op_error - Print an error for a disabled crypto pattern
 * @param op Operation, e.g. #MUTT_PAT_CRYPT_SIGN
 */
static void print_crypt_pattern_op_error(int op)
{
  const struct PatternFlags *entry = lookup_op(op);
  if (entry)
  {
    /* L10N: One of the crypt pattern operators: ~g, ~G, ~k, ~V
       was invoked when NeoMutt was compiled without crypto support.
       %c is the pattern character, i.e. "g".  */
    mutt_error(_("Pattern operator '~%c' is disabled"), entry->tag);
  }
  else
  {
    /* L10N: An unknown pattern operator was somehow invoked.
       This shouldn't be possible unless there is a bug.  */
    mutt_error(_("error: unknown op %d (report this error)"), op);
  }
}

/**
 * msg_search - Search an email
 * @param pat   Pattern to find
 * @param e   Email
 * @param msg Message
 * @retval true Pattern found
 * @retval false Error or pattern not found
 */
static bool msg_search(struct Pattern *pat, struct Email *e, struct Message *msg)
{
  ASSERT(msg);

  bool match = false;

  FILE *fp = NULL;
  long len = 0;
#ifdef USE_FMEMOPEN
  char *temp = NULL;
  size_t tempsize = 0;
#else
  struct stat st = { 0 };
#endif

  const bool needs_head = (pat->op == MUTT_PAT_HEADER) || (pat->op == MUTT_PAT_WHOLE_MSG);
  const bool needs_body = (pat->op == MUTT_PAT_BODY) || (pat->op == MUTT_PAT_WHOLE_MSG);
  const bool c_thorough_search = cs_subset_bool(NeoMutt->sub, "thorough_search");
  if (c_thorough_search)
  {
    /* decode the header / body */
    struct State state = { 0 };
    state.fp_in = msg->fp;
    state.flags = STATE_CHARCONV;
#ifdef USE_FMEMOPEN
    state.fp_out = open_memstream(&temp, &tempsize);
    if (!state.fp_out)
    {
      mutt_perror(_("Error opening 'memory stream'"));
      return false;
    }
#else
    state.fp_out = mutt_file_mkstemp();
    if (!state.fp_out)
    {
      mutt_perror(_("Can't create temporary file"));
      return false;
    }
#endif

    if (needs_head)
    {
      mutt_copy_header(msg->fp, e, state.fp_out, CH_FROM | CH_DECODE, NULL, 0);
    }

    if (needs_body)
    {
      mutt_parse_mime_message(e, msg->fp);

      if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT) &&
          !crypt_valid_passphrase(e->security))
      {
        if (state.fp_out)
        {
          mutt_file_fclose(&state.fp_out);
#ifdef USE_FMEMOPEN
          FREE(&temp);
#endif
        }
        return false;
      }

      if (!mutt_file_seek(msg->fp, e->offset, SEEK_SET))
      {
#ifdef USE_FMEMOPEN
        FREE(&temp);
#endif
        return false;
      }
      mutt_body_handler(e->body, &state);
    }

#ifdef USE_FMEMOPEN
    mutt_file_fclose(&state.fp_out);
    len = tempsize;

    if (tempsize != 0)
    {
      fp = fmemopen(temp, tempsize, "r");
      if (!fp)
      {
        mutt_perror(_("Error re-opening 'memory stream'"));
        FREE(&temp);
        return false;
      }
    }
    else
    { /* fmemopen can't handle empty buffers */
      fp = mutt_file_fopen("/dev/null", "r");
      if (!fp)
      {
        mutt_perror(_("Error opening /dev/null"));
        FREE(&temp);
        return false;
      }
    }
#else
    fp = state.fp_out;
    fflush(fp);
    if (!mutt_file_seek(fp, 0, SEEK_SET) || fstat(fileno(fp), &st))
    {
      mutt_perror(_("Error checking length of temporary file"));
      mutt_file_fclose(&fp);
      return false;
    }
    len = (long) st.st_size;
#endif
  }
  else
  {
    /* raw header / body */
    fp = msg->fp;
    if (needs_head)
    {
      if (!mutt_file_seek(fp, e->offset, SEEK_SET))
      {
        return false;
      }
      len = e->body->offset - e->offset;
    }
    if (needs_body)
    {
      if (pat->op == MUTT_PAT_BODY)
      {
        if (!mutt_file_seek(fp, e->body->offset, SEEK_SET))
        {
          return false;
        }
      }
      len += e->body->length;
    }
  }

  /* search the file "fp" */
  if (pat->op == MUTT_PAT_HEADER)
  {
    struct Buffer *buf = buf_pool_get();
    while (len > 0)
    {
      if (mutt_rfc822_read_line(fp, buf) == 0)
      {
        break;
      }
      len -= buf_len(buf);
      if (patmatch(pat, buf_string(buf)))
      {
        match = true;
        break;
      }
    }
    buf_pool_release(&buf);
  }
  else
  {
    char buf[1024] = { 0 };
    while (len > 0)
    {
      if (!fgets(buf, sizeof(buf), fp))
      {
        break; /* don't loop forever */
      }
      len -= mutt_str_len(buf);
      if (patmatch(pat, buf))
      {
        match = true;
        break;
      }
    }
  }

  if (c_thorough_search)
    mutt_file_fclose(&fp);

#ifdef USE_FMEMOPEN
  FREE(&temp);
#endif

  return match;
}

/**
 * perform_and - Perform a logical AND on a set of Patterns
 * @param pat   Patterns to test
 * @param flags Optional flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param e   Email
 * @param msg Message
 * @param cache Cached Patterns
 * @retval true ALL of the Patterns evaluates to true
 */
static bool perform_and(struct PatternList *pat, PatternExecFlags flags,
                        struct Mailbox *m, struct Email *e, struct Message *msg,
                        struct PatternCache *cache)
{
  struct Pattern *p = NULL;

  SLIST_FOREACH(p, pat, entries)
  {
    if (!pattern_exec(p, flags, m, e, msg, cache))
    {
      return false;
    }
  }
  return true;
}

/**
 * perform_alias_and - Perform a logical AND on a set of Patterns
 * @param pat   Patterns to test
 * @param flags Optional flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param av    AliasView
 * @param cache Cached Patterns
 * @retval true ALL of the Patterns evaluate to true
 */
static bool perform_alias_and(struct PatternList *pat, PatternExecFlags flags,
                              struct AliasView *av, struct PatternCache *cache)
{
  struct Pattern *p = NULL;

  SLIST_FOREACH(p, pat, entries)
  {
    if (!mutt_pattern_alias_exec(p, flags, av, cache))
    {
      return false;
    }
  }
  return true;
}

/**
 * perform_or - Perform a logical OR on a set of Patterns
 * @param pat   Patterns to test
 * @param flags Optional flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param e   Email
 * @param msg Message
 * @param cache Cached Patterns
 * @retval true ONE (or more) of the Patterns evaluates to true
 */
static int perform_or(struct PatternList *pat, PatternExecFlags flags,
                      struct Mailbox *m, struct Email *e, struct Message *msg,
                      struct PatternCache *cache)
{
  struct Pattern *p = NULL;

  SLIST_FOREACH(p, pat, entries)
  {
    if (pattern_exec(p, flags, m, e, msg, cache))
    {
      return true;
    }
  }
  return false;
}

/**
 * perform_alias_or - Perform a logical OR on a set of Patterns
 * @param pat   Patterns to test
 * @param flags Optional flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param av    AliasView
 * @param cache Cached Patterns
 * @retval true ONE (or more) of the Patterns evaluates to true
 */
static int perform_alias_or(struct PatternList *pat, PatternExecFlags flags,
                            struct AliasView *av, struct PatternCache *cache)
{
  struct Pattern *p = NULL;

  SLIST_FOREACH(p, pat, entries)
  {
    if (mutt_pattern_alias_exec(p, flags, av, cache))
    {
      return true;
    }
  }
  return false;
}

/**
 * match_tags - match a pattern against a tags list
 * @param pat  pattern to find
 * @param tags tags list
 * @retval true if any tag match
 */
static bool match_tags(struct Pattern *pat, struct TagList *tags)
{
  struct Tag *tag = NULL;
  bool matched = false;
  STAILQ_FOREACH(tag, tags, entries)
  {
    matched |= patmatch(pat, tag->name);
  }
  return pat->pat_not ^ matched;
}

/**
 * match_addrlist - match a pattern against an address list
 * @param pat            pattern to find
 * @param match_personal if true, also match the pattern against the real name
 * @param n              number of addresses supplied
 * @param ...            variable number of addresses
 * @retval true
 * - one address matches (all_addr is false)
 * - all the addresses match (all_addr is true)
 */
static int match_addrlist(struct Pattern *pat, bool match_personal, int n, ...)
{
  va_list ap;

  va_start(ap, n);
  while (n-- > 0)
  {
    struct AddressList *al = va_arg(ap, struct AddressList *);
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      if (pat->all_addr ^
          ((!pat->is_alias || alias_reverse_lookup(a)) &&
           ((a->mailbox && patmatch(pat, buf_string(a->mailbox))) ||
            (match_personal && a->personal && patmatch(pat, buf_string(a->personal))))))
      {
        va_end(ap);
        return !pat->all_addr; /* Found match, or non-match if all_addr */
      }
    }
  }
  va_end(ap);
  return pat->all_addr; /* No matches, or all matches if all_addr */
}

/**
 * match_reference - Match references against a Pattern
 * @param pat  Pattern to match
 * @param refs List of References
 * @retval true One of the references matches
 */
static bool match_reference(struct Pattern *pat, struct ListHead *refs)
{
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, refs, entries)
  {
    if (patmatch(pat, np->data))
      return true;
  }
  return false;
}

/**
 * mutt_is_predicate_recipient - Test an Envelopes Addresses using a predicate function
 * @param all_addr If true, ALL Addresses must match
 * @param env     Envelope
 * @param p       Predicate function, e.g. mutt_is_subscribed_list()
 * @retval true
 * - One Address matches (all_addr is false)
 * - All the Addresses match (all_addr is true)
 *
 * Test the 'To' and 'Cc' fields of an Address using a test function (the predicate).
 */
static bool mutt_is_predicate_recipient(bool all_addr, struct Envelope *env, addr_predicate_t p)
{
  struct AddressList *als[] = { &env->to, &env->cc };
  for (size_t i = 0; i < mutt_array_size(als); ++i)
  {
    struct AddressList *al = als[i];
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      if (all_addr ^ p(a))
        return !all_addr;
    }
  }
  return all_addr;
}

/**
 * mutt_is_subscribed_list_recipient - Matches subscribed mailing lists
 * @param all_addr If true, ALL Addresses must be on the subscribed list
 * @param env     Envelope
 * @retval true
 * - One Address is subscribed (all_addr is false)
 * - All the Addresses are subscribed (all_addr is true)
 */
bool mutt_is_subscribed_list_recipient(bool all_addr, struct Envelope *env)
{
  return mutt_is_predicate_recipient(all_addr, env, &mutt_is_subscribed_list);
}

/**
 * mutt_is_list_recipient - Matches known mailing lists
 * @param all_addr If true, ALL Addresses must be mailing lists
 * @param env     Envelope
 * @retval true
 * - One Address is a mailing list (all_addr is false)
 * - All the Addresses are mailing lists (all_addr is true)
 */
bool mutt_is_list_recipient(bool all_addr, struct Envelope *env)
{
  return mutt_is_predicate_recipient(all_addr, env, &mutt_is_mail_list);
}

/**
 * match_user - Matches the user's email Address
 * @param all_addr If true, ALL Addresses must refer to the user
 * @param n       number of AddressLists supplied
 * @param ...     Variable number of AddressLists
 * @retval true
 * - One Address refers to the user (all_addr is false)
 * - All the Addresses refer to the user (all_addr is true)
 */
static int match_user(bool all_addr, int n, ...)
{
  va_list ap;

  va_start(ap, n);
  while (n-- > 0)
  {
    struct AddressList *al = va_arg(ap, struct AddressList *);
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      if (all_addr ^ mutt_addr_is_user(a))
      {
        va_end(ap);
        return !all_addr;
      }
    }
  }
  va_end(ap);
  return all_addr;
}

/**
 * match_threadcomplete - Match a Pattern against an email thread
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param t     Email thread
 * @param left  Navigate to the previous email
 * @param up    Navigate to the email's parent
 * @param right Navigate to the next email
 * @param down  Navigate to the email's children
 * @retval 1  Success, match found
 * @retval 0  No match
 */
static int match_threadcomplete(struct PatternList *pat, PatternExecFlags flags,
                                struct Mailbox *m, struct MuttThread *t,
                                int left, int up, int right, int down)
{
  if (!t)
    return 0;

  int a;
  struct Email *e = t->message;
  if (e)
    if (mutt_pattern_exec(SLIST_FIRST(pat), flags, m, e, NULL))
      return 1;

  if (up && (a = match_threadcomplete(pat, flags, m, t->parent, 1, 1, 1, 0)))
    return a;
  if (right && t->parent && (a = match_threadcomplete(pat, flags, m, t->next, 0, 0, 1, 1)))
  {
    return a;
  }
  if (left && t->parent && (a = match_threadcomplete(pat, flags, m, t->prev, 1, 0, 0, 1)))
  {
    return a;
  }
  if (down && (a = match_threadcomplete(pat, flags, m, t->child, 1, 0, 1, 1)))
    return a;
  return 0;
}

/**
 * match_threadparent - Match Pattern against an email's parent
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param t     Thread of email
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 */
static int match_threadparent(struct PatternList *pat, PatternExecFlags flags,
                              struct Mailbox *m, struct MuttThread *t)
{
  if (!t || !t->parent || !t->parent->message)
    return 0;

  return mutt_pattern_exec(SLIST_FIRST(pat), flags, m, t->parent->message, NULL);
}

/**
 * match_threadchildren - Match Pattern against an email's children
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param t     Thread of email
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 */
static int match_threadchildren(struct PatternList *pat, PatternExecFlags flags,
                                struct Mailbox *m, struct MuttThread *t)
{
  if (!t || !t->child)
    return 0;

  for (t = t->child; t; t = t->next)
    if (t->message && mutt_pattern_exec(SLIST_FIRST(pat), flags, m, t->message, NULL))
      return 1;

  return 0;
}

/**
 * match_content_type - Match a Pattern against an Attachment's Content-Type
 * @param pat   Pattern to match
 * @param b     Attachment
 * @retval true  Success, pattern matched
 * @retval false Pattern did not match
 */
static bool match_content_type(const struct Pattern *pat, struct Body *b)
{
  if (!b)
    return false;

  char buf[256] = { 0 };
  snprintf(buf, sizeof(buf), "%s/%s", TYPE(b), b->subtype);

  if (patmatch(pat, buf))
    return true;
  if (match_content_type(pat, b->parts))
    return true;
  if (match_content_type(pat, b->next))
    return true;
  return false;
}

/**
 * match_mime_content_type - Match a Pattern against an email's Content-Type
 * @param pat Pattern to match
 * @param e   Email
 * @param fp  Message file
 * @retval true  Success, pattern matched
 * @retval false Pattern did not match
 */
static bool match_mime_content_type(const struct Pattern *pat, struct Email *e, FILE *fp)
{
  mutt_parse_mime_message(e, fp);
  return match_content_type(pat, e->body);
}

/**
 * match_update_dynamic_date - Update a dynamic date pattern
 * @param pat Pattern to modify
 * @retval true  Pattern valid and updated
 * @retval false Pattern invalid
 */
static bool match_update_dynamic_date(struct Pattern *pat)
{
  struct Buffer *err = buf_pool_get();

  bool rc = eval_date_minmax(pat, pat->p.str, err);
  buf_pool_release(&err);

  return rc;
}

/**
 * set_pattern_cache_value - Sets a value in the PatternCache cache entry
 * @param cache_entry Cache entry to update
 * @param value       Value to set
 *
 * Normalizes the "true" value to 2.
 */
static void set_pattern_cache_value(int *cache_entry, int value)
{
  *cache_entry = (value != 0) ? 2 : 1;
}

/**
 * get_pattern_cache_value - Get pattern cache value
 * @param cache_entry Cache entry to get
 * @retval 1 The cache value is set and has a true value
 * @retval 0 otherwise (even if unset!)
 */
static bool get_pattern_cache_value(int cache_entry)
{
  return cache_entry == 2;
}

/**
 * is_pattern_cache_set - Is a given Pattern cached?
 * @param cache_entry Cache entry to check
 * @retval true Pattern is cached
 */
static int is_pattern_cache_set(int cache_entry)
{
  return cache_entry != 0;
}

/**
 * msg_search_sendmode - Search in send-mode
 * @param e   Email to search
 * @param pat Pattern to find
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 */
static int msg_search_sendmode(struct Email *e, struct Pattern *pat)
{
  bool match = false;
  char *buf = NULL;
  size_t blen = 0;
  FILE *fp = NULL;

  if ((pat->op == MUTT_PAT_HEADER) || (pat->op == MUTT_PAT_WHOLE_MSG))
  {
    struct Buffer *tempfile = buf_pool_get();
    buf_mktemp(tempfile);
    fp = mutt_file_fopen(buf_string(tempfile), "w+");
    if (!fp)
    {
      mutt_perror("%s", buf_string(tempfile));
      buf_pool_release(&tempfile);
      return 0;
    }

    mutt_rfc822_write_header(fp, e->env, e->body, MUTT_WRITE_HEADER_POSTPONE,
                             false, false, NeoMutt->sub);
    fflush(fp);
    if (mutt_file_seek(fp, 0, SEEK_SET))
    {
      while ((buf = mutt_file_read_line(buf, &blen, fp, NULL, MUTT_RL_NO_FLAGS)) != NULL)
      {
        if (patmatch(pat, buf) == 0)
        {
          match = true;
          break;
        }
      }
    }

    FREE(&buf);
    mutt_file_fclose(&fp);
    unlink(buf_string(tempfile));
    buf_pool_release(&tempfile);

    if (match)
      return match;
  }

  if ((pat->op == MUTT_PAT_BODY) || (pat->op == MUTT_PAT_WHOLE_MSG))
  {
    fp = mutt_file_fopen(e->body->filename, "r");
    if (!fp)
    {
      mutt_perror("%s", e->body->filename);
      return 0;
    }

    while ((buf = mutt_file_read_line(buf, &blen, fp, NULL, MUTT_RL_NO_FLAGS)) != NULL)
    {
      if (patmatch(pat, buf) == 0)
      {
        match = true;
        break;
      }
    }

    FREE(&buf);
    mutt_file_fclose(&fp);
  }

  return match;
}

/**
 * pattern_needs_msg - Check whether a pattern needs a full message
 * @param m Mailbox
 * @param pat Pattern
 * @retval true The pattern needs a full message
 * @retval false The pattern does not need a full message
 */
static bool pattern_needs_msg(const struct Mailbox *m, const struct Pattern *pat)
{
  if (!m)
  {
    return false;
  }

  if ((pat->op == MUTT_PAT_MIMETYPE) || (pat->op == MUTT_PAT_MIMEATTACH))
  {
    return true;
  }

  if ((pat->op == MUTT_PAT_WHOLE_MSG) || (pat->op == MUTT_PAT_BODY) || (pat->op == MUTT_PAT_HEADER))
  {
    return !((m->type == MUTT_IMAP) && pat->string_match);
  }

  if ((pat->op == MUTT_PAT_AND) || (pat->op == MUTT_PAT_OR))
  {
    struct Pattern *p = NULL;
    SLIST_FOREACH(p, pat->child, entries)
    {
      if (pattern_needs_msg(m, p))
      {
        return true;
      }
    }
  }

  return false;
}

/**
 * pattern_exec - Match a pattern against an email header
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m     Mailbox
 * @param e     Email
 * @param msg   MEssage
 * @param cache Cache for common Patterns
 * @retval true Success, pattern matched
 * @retval false Pattern did not match
 *
 * flags: MUTT_MATCH_FULL_ADDRESS: match both personal and machine address
 * cache: For repeated matches against the same Header, passing in non-NULL will
 *        store some of the cacheable pattern matches in this structure.
 */
static bool pattern_exec(struct Pattern *pat, PatternExecFlags flags,
                         struct Mailbox *m, struct Email *e,
                         struct Message *msg, struct PatternCache *cache)
{
  switch (pat->op)
  {
    case MUTT_PAT_AND:
      return pat->pat_not ^ (perform_and(pat->child, flags, m, e, msg, cache) > 0);
    case MUTT_PAT_OR:
      return pat->pat_not ^ (perform_or(pat->child, flags, m, e, msg, cache) > 0);
    case MUTT_PAT_THREAD:
      return pat->pat_not ^
             match_threadcomplete(pat->child, flags, m, e->thread, 1, 1, 1, 1);
    case MUTT_PAT_PARENT:
      return pat->pat_not ^ match_threadparent(pat->child, flags, m, e->thread);
    case MUTT_PAT_CHILDREN:
      return pat->pat_not ^ match_threadchildren(pat->child, flags, m, e->thread);
    case MUTT_ALL:
      return !pat->pat_not;
    case MUTT_EXPIRED:
      return pat->pat_not ^ e->expired;
    case MUTT_SUPERSEDED:
      return pat->pat_not ^ e->superseded;
    case MUTT_FLAG:
      return pat->pat_not ^ e->flagged;
    case MUTT_TAG:
      return pat->pat_not ^ e->tagged;
    case MUTT_NEW:
      return pat->pat_not ? e->old || e->read : !(e->old || e->read);
    case MUTT_UNREAD:
      return pat->pat_not ? e->read : !e->read;
    case MUTT_REPLIED:
      return pat->pat_not ^ e->replied;
    case MUTT_OLD:
      return pat->pat_not ? (!e->old || e->read) : (e->old && !e->read);
    case MUTT_READ:
      return pat->pat_not ^ e->read;
    case MUTT_DELETED:
      return pat->pat_not ^ e->deleted;
    case MUTT_PAT_MESSAGE:
      return pat->pat_not ^
             ((email_msgno(e) >= pat->min) && (email_msgno(e) <= pat->max));
    case MUTT_PAT_DATE:
      if (pat->dynamic)
        match_update_dynamic_date(pat);
      return pat->pat_not ^ ((e->date_sent >= pat->min) && (e->date_sent <= pat->max));
    case MUTT_PAT_DATE_RECEIVED:
      if (pat->dynamic)
        match_update_dynamic_date(pat);
      return pat->pat_not ^ ((e->received >= pat->min) && (e->received <= pat->max));
    case MUTT_PAT_BODY:
    case MUTT_PAT_HEADER:
    case MUTT_PAT_WHOLE_MSG:
      if (pat->sendmode)
      {
        if (!e->body || !e->body->filename)
          return false;
        return pat->pat_not ^ msg_search_sendmode(e, pat);
      }
      /* m can be NULL in certain cases, such as when replying to a message
       * from the attachment menu and the user has a reply-hook using "~e".
       * This is also the case when message scoring.  */
      if (!m)
        return false;
      /* IMAP search sets e->matched at search compile time */
      if ((m->type == MUTT_IMAP) && pat->string_match)
        return e->matched;
      return pat->pat_not ^ msg_search(pat, e, msg);
    case MUTT_PAT_SERVERSEARCH:
      if (!m)
        return false;
      if (m->type == MUTT_IMAP)
      {
        return (pat->string_match) ? e->matched : false;
      }
      mutt_error(_("error: server custom search only supported with IMAP"));
      return false;
    case MUTT_PAT_SENDER:
      if (!e->env)
        return false;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS),
                                           1, &e->env->sender);
    case MUTT_PAT_FROM:
      if (!e->env)
        return false;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->from);
    case MUTT_PAT_TO:
      if (!e->env)
        return false;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->to);
    case MUTT_PAT_CC:
      if (!e->env)
        return false;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->cc);
    case MUTT_PAT_BCC:
      if (!e->env)
        return false;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->bcc);
    case MUTT_PAT_SUBJECT:
      if (!e->env)
        return false;
      return pat->pat_not ^ (e->env->subject && patmatch(pat, e->env->subject));
    case MUTT_PAT_ID:
    case MUTT_PAT_ID_EXTERNAL:
      if (!e->env)
        return false;
      return pat->pat_not ^ (e->env->message_id && patmatch(pat, e->env->message_id));
    case MUTT_PAT_SCORE:
      return pat->pat_not ^ (e->score >= pat->min &&
                             (pat->max == MUTT_MAXRANGE || e->score <= pat->max));
    case MUTT_PAT_SIZE:
      return pat->pat_not ^ (e->body->length >= pat->min &&
                             (pat->max == MUTT_MAXRANGE || e->body->length <= pat->max));
    case MUTT_PAT_REFERENCE:
      if (!e->env)
        return false;
      return pat->pat_not ^ (match_reference(pat, &e->env->references) ||
                             match_reference(pat, &e->env->in_reply_to));
    case MUTT_PAT_ADDRESS:
      if (!e->env)
        return false;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS),
                                           5, &e->env->from, &e->env->sender,
                                           &e->env->to, &e->env->cc, &e->env->bcc);
    case MUTT_PAT_RECIPIENT:
      if (!e->env)
        return false;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 3,
                                           &e->env->to, &e->env->cc, &e->env->bcc);
    case MUTT_PAT_LIST: /* known list, subscribed or not */
    {
      if (!e->env)
        return false;

      bool result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->list_all : &cache->list_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(cache_entry,
                                  mutt_is_list_recipient(pat->all_addr, e->env));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
      {
        result = mutt_is_list_recipient(pat->all_addr, e->env);
      }
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_SUBSCRIBED_LIST:
    {
      if (!e->env)
        return false;

      bool result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->sub_all : &cache->sub_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(cache_entry,
                                  mutt_is_subscribed_list_recipient(pat->all_addr, e->env));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
      {
        result = mutt_is_subscribed_list_recipient(pat->all_addr, e->env);
      }
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_PERSONAL_RECIP:
    {
      if (!e->env)
        return false;

      bool result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->pers_recip_all : &cache->pers_recip_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(cache_entry,
                                  match_user(pat->all_addr, 3, &e->env->to,
                                             &e->env->cc, &e->env->bcc));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
      {
        result = match_user(pat->all_addr, 3, &e->env->to, &e->env->cc, &e->env->bcc);
      }
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_PERSONAL_FROM:
    {
      if (!e->env)
        return false;

      bool result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->pers_from_all : &cache->pers_from_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(cache_entry,
                                  match_user(pat->all_addr, 1, &e->env->from));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
      {
        result = match_user(pat->all_addr, 1, &e->env->from);
      }
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_COLLAPSED:
      return pat->pat_not ^ (e->collapsed && e->num_hidden > 1);
    case MUTT_PAT_CRYPT_SIGN:
      if (!WithCrypto)
      {
        print_crypt_pattern_op_error(pat->op);
        return false;
      }
      return pat->pat_not ^ ((e->security & SEC_SIGN) ? 1 : 0);
    case MUTT_PAT_CRYPT_VERIFIED:
      if (!WithCrypto)
      {
        print_crypt_pattern_op_error(pat->op);
        return false;
      }
      return pat->pat_not ^ ((e->security & SEC_GOODSIGN) ? 1 : 0);
    case MUTT_PAT_CRYPT_ENCRYPT:
      if (!WithCrypto)
      {
        print_crypt_pattern_op_error(pat->op);
        return false;
      }
      return pat->pat_not ^ ((e->security & SEC_ENCRYPT) ? 1 : 0);
    case MUTT_PAT_PGP_KEY:
      if (!(WithCrypto & APPLICATION_PGP))
      {
        print_crypt_pattern_op_error(pat->op);
        return false;
      }
      return pat->pat_not ^ ((e->security & PGP_KEY) == PGP_KEY);
    case MUTT_PAT_XLABEL:
      if (!e->env)
        return false;
      return pat->pat_not ^ (e->env->x_label && patmatch(pat, e->env->x_label));
    case MUTT_PAT_DRIVER_TAGS:
    {
      return match_tags(pat, &e->tags);
    }
    case MUTT_PAT_HORMEL:
      if (!e->env)
        return false;
      return pat->pat_not ^ (e->env->spam.data && patmatch(pat, e->env->spam.data));
    case MUTT_PAT_DUPLICATED:
      return pat->pat_not ^ (e->thread && e->thread->duplicate_thread);
    case MUTT_PAT_MIMEATTACH:
    {
      int count = mutt_count_body_parts(e, msg->fp);
      return pat->pat_not ^
             (count >= pat->min && (pat->max == MUTT_MAXRANGE || count <= pat->max));
    }
    case MUTT_PAT_MIMETYPE:
      if (!m)
        return false;
      return pat->pat_not ^ match_mime_content_type(pat, e, msg->fp);
    case MUTT_PAT_UNREFERENCED:
      return pat->pat_not ^ (e->thread && !e->thread->child);
    case MUTT_PAT_BROKEN:
      return pat->pat_not ^ (e->thread && e->thread->fake_thread);
    case MUTT_PAT_NEWSGROUPS:
      if (!e->env)
        return false;
      return pat->pat_not ^ (e->env->newsgroups && patmatch(pat, e->env->newsgroups));
  }
  mutt_error(_("error: unknown op %d (report this error)"), pat->op);
  return false;
}

/**
 * mutt_pattern_exec - Match a pattern against an email header
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m     Mailbox
 * @param e     Email
 * @param cache Cache for common Patterns
 * @retval true Success, pattern matched
 * @retval false Pattern did not match
 *
 * flags: MUTT_MATCH_FULL_ADDRESS: match both personal and machine address
 * cache: For repeated matches against the same Header, passing in non-NULL will
 *        store some of the cacheable pattern matches in this structure.
 */
bool mutt_pattern_exec(struct Pattern *pat, PatternExecFlags flags,
                       struct Mailbox *m, struct Email *e, struct PatternCache *cache)
{
  const bool needs_msg = pattern_needs_msg(m, pat);
  struct Message *msg = needs_msg ? mx_msg_open(m, e) : NULL;
  if (needs_msg && !msg)
  {
    return false;
  }
  const bool matched = pattern_exec(pat, flags, m, e, msg, cache);
  mx_msg_close(m, &msg);
  return matched;
}

/**
 * mutt_pattern_alias_exec - Match a pattern against an alias
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param av    AliasView
 * @param cache Cache for common Patterns
 * @retval true Success, pattern matched
 * @retval false Pattern did not match
 *
 * flags: MUTT_MATCH_FULL_ADDRESS: match both personal and machine address
 * cache: For repeated matches against the same Alias, passing in non-NULL will
 *        store some of the cacheable pattern matches in this structure.
 */
bool mutt_pattern_alias_exec(struct Pattern *pat, PatternExecFlags flags,
                             struct AliasView *av, struct PatternCache *cache)
{
  switch (pat->op)
  {
    case MUTT_PAT_FROM: /* alias */
      if (!av->alias)
        return false;
      return pat->pat_not ^ (av->alias->name && patmatch(pat, av->alias->name));
    case MUTT_PAT_CC: /* comment */
      if (!av->alias)
        return false;
      return pat->pat_not ^ (av->alias->comment && patmatch(pat, av->alias->comment));
    case MUTT_PAT_TO: /* alias address list */
      if (!av->alias)
        return false;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS),
                                           1, &av->alias->addr);
    case MUTT_PAT_DRIVER_TAGS:
      if (!av->alias)
        return false;
      return match_tags(pat, &av->alias->tags);
    case MUTT_PAT_AND:
      return pat->pat_not ^ (perform_alias_and(pat->child, flags, av, cache) > 0);
    case MUTT_PAT_OR:
      return pat->pat_not ^ (perform_alias_or(pat->child, flags, av, cache) > 0);
  }

  return false;
}
