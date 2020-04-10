/**
 * @file
 * Manage regular expressions
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_LIB_REGEX_H
#define MUTT_LIB_REGEX_H

#include <stddef.h>
#include "config.h"
#include <regex.h>
#include <stdbool.h>
#include "queue.h"

struct Buffer;

/* ... DT_REGEX */
#define DT_REGEX_MATCH_CASE (1 << 6)  ///< Case-sensitive matching
#define DT_REGEX_ALLOW_NOT  (1 << 7)  ///< Regex can begin with '!'
#define DT_REGEX_NOSUB      (1 << 8)  ///< Do not report what was matched (REG_NOSUB)

/* This is a non-standard option supported by Solaris 2.5.x
 * which allows patterns of the form \<...\> */
#ifndef REG_WORDS
#define REG_WORDS 0
#endif

/**
 * REG_COMP - Compile a regular expression
 * @param preg   regex_t struct to fill
 * @param regex  Regular expression string
 * @param cflags Flags
 * @retval   0 Success
 * @retval num Failure, e.g. REG_BADPAT
 */
#define REG_COMP(preg, regex, cflags) regcomp(preg, regex, REG_WORDS | REG_EXTENDED | (cflags))

/**
 * mutt_regmatch_start - Return the start of a match
 * @param match Match
 * @retval num Start of the match
 */
static inline regoff_t mutt_regmatch_start(const regmatch_t *match)
{
  return match->rm_so;
}

/**
 * mutt_regmatch_end - Return the end of a match
 * @param match Match
 * @retval num End of the match
 */
static inline regoff_t mutt_regmatch_end(const regmatch_t *match)
{
  return match->rm_eo;
}

/**
 * mutt_regmatch_len - Return the length of a match
 * @param match Match
 * @retval num Length of the match
 */
static inline size_t mutt_regmatch_len(const regmatch_t *match)
{
  return match->rm_eo - match->rm_so;
}

/**
 * struct Regex - Cached regular expression
 */
struct Regex
{
  char *pattern;  ///< printable version
  regex_t *regex; ///< compiled expression
  bool pat_not;       ///< do not match
};

/**
 * struct RegexNode - List of regular expressions
 */
struct RegexNode
{
  struct Regex *regex;             ///< Regex containing a regular expression
  STAILQ_ENTRY(RegexNode) entries; ///< Linked list
};
STAILQ_HEAD(RegexList, RegexNode);

/**
 * struct Replace - List of regular expressions
 */
struct Replace
{
  struct Regex *regex;           ///< Regex containing a regular expression
  size_t nmatch;                 ///< Match the 'nth' occurrence (0 means the whole expression)
  char *templ;                   ///< Template to match
  STAILQ_ENTRY(Replace) entries; ///< Linked list
};
STAILQ_HEAD(ReplaceList, Replace);

struct Regex *mutt_regex_compile(const char *str, int flags);
struct Regex *mutt_regex_new(const char *str, int flags, struct Buffer *err);
void          mutt_regex_free(struct Regex **r);

int               mutt_regexlist_add   (struct RegexList *rl, const char *str, int flags, struct Buffer *err);
void              mutt_regexlist_free  (struct RegexList *rl);
bool              mutt_regexlist_match (struct RegexList *rl, const char *str);
struct RegexNode *mutt_regexlist_new   (void);
int               mutt_regexlist_remove(struct RegexList *rl, const char *str);

int             mutt_replacelist_add   (struct ReplaceList *rl, const char *pat, const char *templ, struct Buffer *err);
char *          mutt_replacelist_apply (struct ReplaceList *rl, char *buf, size_t buflen, const char *str);
void            mutt_replacelist_free  (struct ReplaceList *rl);
bool            mutt_replacelist_match (struct ReplaceList *rl, char *buf, size_t buflen, const char *str);
struct Replace *mutt_replacelist_new   (void);
int             mutt_replacelist_remove(struct ReplaceList *rl, const char *pat);

bool mutt_regex_match  (const struct Regex *regex, const char *str);
bool mutt_regex_capture(const struct Regex *regex, const char *str, size_t num, regmatch_t matches[]);

#endif /* MUTT_LIB_REGEX_H */
