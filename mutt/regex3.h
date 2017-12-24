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

#ifndef _MUTT_REGEX_H
#define _MUTT_REGEX_H

#include <regex.h>
#include <stdbool.h>

struct Buffer;

/* ... DT_REGEX */
#define DT_REGEX_MATCH_CASE 0x010 /**< Case-sensitive matching */
#define DT_REGEX_ALLOW_NOT  0x020 /**< Regex can begin with '!' */

/* This is a non-standard option supported by Solaris 2.5.x
 * which allows patterns of the form \<...\> */
#ifndef REG_WORDS
#define REG_WORDS 0
#endif

/**
 * REGCOMP - Compile a regular expression
 * @param X regex_t struct to fill
 * @param Y Regular expression string
 * @param Z Flags
 * @retval   0 Success
 * @retval num Failure, e.g. REG_BADPAT
 */
#define REGCOMP(X, Y, Z) regcomp(X, Y, REG_WORDS | REG_EXTENDED | (Z))
/**
 * REGEXEC - Perform a regular expression comparison
 * @param X regex_t containing compiled regular expression
 * @param Y String to compare
 * @retval 0           Success
 * @retval REG_NOMATCH Failure
 */
#define REGEXEC(X, Y) regexec(&X, Y, (size_t) 0, (regmatch_t *) 0, (int) 0)

/**
 * struct Regex - Cached regular expression
 */
struct Regex
{
  char *pattern;  /**< printable version */
  regex_t *regex; /**< compiled expression */
  bool not;       /**< do not match */
};

/**
 * struct RegexList - List of regular expressions
 */
struct RegexList
{
  struct Regex *regex;
  struct RegexList *next;
};

/**
 * struct ReplaceList - List of regular expressions
 */
struct ReplaceList
{
  struct Regex *regex;
  int nmatch;
  char *template;
  struct ReplaceList *next;
};

struct Regex *      mutt_regex_compile(const char *s, int flags);
struct Regex *      mutt_regex_create(const char *str, int flags, struct Buffer *err);
void                mutt_regex_free(struct Regex **pp);

int                 mutt_regexlist_add(struct RegexList **list, const char *s, int flags, struct Buffer *err);
void                mutt_regexlist_free(struct RegexList **list);
bool                mutt_regexlist_match(const char *s, struct RegexList *l);
struct RegexList *  mutt_regexlist_new(void);
int                 mutt_regexlist_remove(struct RegexList **l, const char *str);

int                 mutt_replacelist_add(struct ReplaceList **list, const char *pat, const char *templ, struct Buffer *err);
char *              mutt_replacelist_apply(char *dbuf, size_t dlen, char *sbuf, struct ReplaceList *rlist);
void                mutt_replacelist_free(struct ReplaceList **list);
bool                mutt_replacelist_match(const char *s, struct ReplaceList *l, char *text, int textsize);
struct ReplaceList *mutt_replacelist_new(void);
int                 mutt_replacelist_remove(struct ReplaceList **list, const char *pat);

#endif /* _MUTT_REGEX_H */
