/**
 * @file
 * Match patterns to emails
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page lib_pattern Pattern
 *
 * Match patterns to emails
 *
 * | File                   | Description                   |
 * | :--------------------- | :---------------------------- |
 * | pattern/compile.c      | @subpage pattern_compile      |
 * | pattern/complete.c     | @subpage pattern_complete     |
 * | pattern/config.c       | @subpage pattern_config       |
 * | pattern/dlg_pattern.c  | @subpage pattern_dlg_pattern  |
 * | pattern/exec.c         | @subpage pattern_exec         |
 * | pattern/flags.c        | @subpage pattern_flags        |
 * | pattern/functions.c    | @subpage pattern_functions    |
 * | pattern/message.c      | @subpage pattern_message      |
 * | pattern/pattern.c      | @subpage pattern_pattern      |
 * | pattern/search_state.c | @subpage pattern_search_state |
 */

#ifndef MUTT_PATTERN_LIB_H
#define MUTT_PATTERN_LIB_H

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "mutt.h"
#include "search_state.h"

struct AliasMenuData;
struct AliasView;
struct Email;
struct Envelope;
struct Mailbox;
struct MailboxView;
struct Menu;

#define MUTT_ALIAS_SIMPLESEARCH "~f %s | ~t %s | ~c %s"

extern const struct CompleteOps CompletePatternOps;

typedef uint8_t PatternCompFlags;           ///< Flags for mutt_pattern_comp(), e.g. #MUTT_PC_FULL_MSG
#define MUTT_PC_NO_FLAGS                0   ///< No flags are set
#define MUTT_PC_FULL_MSG          (1 << 0)  ///< Enable body and header matching
#define MUTT_PC_PATTERN_DYNAMIC   (1 << 1)  ///< Enable runtime date range evaluation
#define MUTT_PC_SEND_MODE_SEARCH  (1 << 2)  ///< Allow send-mode body searching

/**
 * struct Pattern - A simple (non-regex) pattern
 */
struct Pattern
{
  short op;                      ///< Operation, e.g. #MUTT_PAT_SCORE
  bool pat_not      : 1;         ///< Pattern should be inverted (not)
  bool all_addr     : 1;         ///< All Addresses in the list must match
  bool string_match : 1;         ///< Check a string for a match
  bool group_match  : 1;         ///< Check a group of Addresses
  bool ign_case     : 1;         ///< Ignore case for local string_match searches
  bool is_alias     : 1;         ///< Is there an alias for this Address?
  bool dynamic      : 1;         ///< Evaluate date ranges at run time
  bool sendmode     : 1;         ///< Evaluate searches in send-mode
  bool is_multi     : 1;         ///< Multiple case (only for ~I pattern now)
  long min;                      ///< Minimum for range checks
  long max;                      ///< Maximum for range checks
  struct PatternList *child;     ///< Arguments to logical operation
  union {
    regex_t *regex;              ///< Compiled regex, for non-pattern matching
    struct Group *group;         ///< Address group if group_match is set
    char *str;                   ///< String, if string_match is set
    struct ListHead multi_cases; ///< Multiple strings for ~I pattern
  } p;
#ifdef USE_DEBUG_GRAPHVIZ
  const char *raw_pattern;
#endif
  SLIST_ENTRY(Pattern) entries;  ///< Linked list
};
SLIST_HEAD(PatternList, Pattern);

typedef uint8_t PatternExecFlags;         ///< Flags for mutt_pattern_exec(), e.g. #MUTT_MATCH_FULL_ADDRESS
#define MUTT_PAT_EXEC_NO_FLAGS         0  ///< No flags are set
#define MUTT_MATCH_FULL_ADDRESS  (1 << 0) ///< Match the full address

/**
 * struct PatternCache - Cache commonly-used patterns
 *
 * This is used when a message is repeatedly pattern matched against.
 * e.g. for color, scoring, hooks.  It caches a few of the potentially slow
 * operations.
 * Each entry has a value of 0 = unset, 1 = false, 2 = true
 */
struct PatternCache
{
  int list_all;       ///< ^~l
  int list_one;       ///<  ~l
  int sub_all;        ///< ^~u
  int sub_one;        ///<  ~u
  int pers_recip_all; ///< ^~p
  int pers_recip_one; ///<  ~p
  int pers_from_all;  ///< ^~P
  int pers_from_one;  ///<  ~P
};

/**
 * enum PatternType - Types of pattern to match
 *
 * @note This enum piggy-backs on top of #MessageType
 *
 * @sa mutt_pattern_comp(), mutt_pattern_exec()
 */
enum PatternType
{
  MUTT_PAT_AND = MUTT_MT_MAX, ///< Both patterns must match
  MUTT_PAT_OR,                ///< Either pattern can match
  MUTT_PAT_THREAD,            ///< Pattern matches email thread
  MUTT_PAT_PARENT,            ///< Pattern matches parent
  MUTT_PAT_CHILDREN,          ///< Pattern matches a child email
  MUTT_PAT_TO,                ///< Pattern matches 'To:' field
  MUTT_PAT_CC,                ///< Pattern matches 'Cc:' field
  MUTT_PAT_BCC,               ///< Pattern matches 'Bcc:' field
  MUTT_PAT_COLLAPSED,         ///< Thread is collapsed
  MUTT_PAT_SUBJECT,           ///< Pattern matches 'Subject:' field
  MUTT_PAT_FROM,              ///< Pattern matches 'From:' field
  MUTT_PAT_DATE,              ///< Pattern matches 'Date:' field
  MUTT_PAT_DATE_RECEIVED,     ///< Pattern matches date received
  MUTT_PAT_DUPLICATED,        ///< Duplicate message
  MUTT_PAT_UNREFERENCED,      ///< Message is unreferenced in the thread
  MUTT_PAT_BROKEN,            ///< Message is part of a broken thread
  MUTT_PAT_ID,                ///< Pattern matches email's Message-Id
  MUTT_PAT_ID_EXTERNAL,       ///< Message-Id is among results from an external query
  MUTT_PAT_BODY,              ///< Pattern matches email's body
  MUTT_PAT_HEADER,            ///< Pattern matches email's header
  MUTT_PAT_HORMEL,            ///< Pattern matches email's spam score
  MUTT_PAT_WHOLE_MSG,         ///< Pattern matches raw email text
  MUTT_PAT_SENDER,            ///< Pattern matches sender
  MUTT_PAT_MESSAGE,           ///< Pattern matches message number
  MUTT_PAT_SCORE,             ///< Pattern matches email's score
  MUTT_PAT_SIZE,              ///< Pattern matches email's size
  MUTT_PAT_REFERENCE,         ///< Pattern matches 'References:' or 'In-Reply-To:' field
  MUTT_PAT_RECIPIENT,         ///< User is a recipient of the email
  MUTT_PAT_LIST,              ///< Email is on mailing list
  MUTT_PAT_SUBSCRIBED_LIST,   ///< Email is on subscribed mailing list
  MUTT_PAT_PERSONAL_RECIP,    ///< Email is addressed to the user
  MUTT_PAT_PERSONAL_FROM,     ///< Email is from the user
  MUTT_PAT_ADDRESS,           ///< Pattern matches any address field
  MUTT_PAT_CRYPT_SIGN,        ///< Message is signed
  MUTT_PAT_CRYPT_VERIFIED,    ///< Message is crypographically verified
  MUTT_PAT_CRYPT_ENCRYPT,     ///< Message is encrypted
  MUTT_PAT_PGP_KEY,           ///< Message has PGP key
  MUTT_PAT_XLABEL,            ///< Pattern matches keyword/label
  MUTT_PAT_SERVERSEARCH,      ///< Server-side pattern matches
  MUTT_PAT_DRIVER_TAGS,       ///< Pattern matches message tags
  MUTT_PAT_MIMEATTACH,        ///< Pattern matches number of attachments
  MUTT_PAT_MIMETYPE,          ///< Pattern matches MIME type
  MUTT_PAT_NEWSGROUPS,        ///< Pattern matches newsgroup
  MUTT_PAT_MAX,
};

/**
 * enum PatternAliasAction - What to do with the matching Aliases
 */
enum PatternAlias
{
  PAA_TAG,        ///< Set   AliasView.is_tagged, but don't touch the others
  PAA_UNTAG,      ///< Unset AliasView.is_tagged, but don't touch the others
  PAA_VISIBLE,    ///< Set   AliasView.is_visible and hide the rest
};

bool mutt_pattern_exec(struct Pattern *pat, PatternExecFlags flags, struct Mailbox *m,
                       struct Email *e, struct PatternCache *cache);
bool mutt_pattern_alias_exec(struct Pattern *pat, PatternExecFlags flags,
                             struct AliasView *av, struct PatternCache *cache);

struct PatternList *mutt_pattern_comp(struct MailboxView *mv, struct Menu *menu, const char *s, PatternCompFlags flags, struct Buffer *err);
void mutt_check_simple(struct Buffer *s, const char *simple);
void mutt_pattern_free(struct PatternList **pat);
bool dlg_pattern(char *buf, size_t buflen);

bool mutt_is_list_recipient(bool all_addr, struct Envelope *env);
bool mutt_is_subscribed_list_recipient(bool all_addr, struct Envelope *env);
int mutt_pattern_func(struct MailboxView *mv, int op, char *prompt);
int mutt_pattern_alias_func(char *prompt, struct AliasMenuData *mdata, enum PatternAlias action, struct Menu *menu);
int mutt_search_command(struct MailboxView *mv, struct Menu *menu, int cur,
                        struct SearchState *state, SearchFlags flags);
int mutt_search_alias_command(struct Menu *menu, int cur,
                              struct SearchState *state, SearchFlags flags);

#endif /* MUTT_PATTERN_LIB_H */
