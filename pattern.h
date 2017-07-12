/**
 * @file
 * Match patterns to emails
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

#ifndef _MUTT_PATTERN_H
#define _MUTT_PATTERN_H

#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include "lib.h"

struct Address;
struct Buffer;
struct Header;
struct Context;

/**
 * struct Pattern - A simple (non-regex) pattern
 */
struct Pattern
{
  short op;
  bool not : 1;
  bool alladdr : 1;
  bool stringmatch : 1;
  bool groupmatch : 1;
  bool ign_case : 1; /**< ignore case for local stringmatch searches */
  bool isalias : 1;
  int min;
  int max;
  struct Pattern *next;
  struct Pattern *child; /**< arguments to logical op */
  union {
    regex_t *rx;
    struct Group *g;
    char *str;
  } p;
};

/**
 * enum PatternExecFlag - Flags for mutt_pattern_exec()
 */
enum PatternExecFlag
{
  MUTT_MATCH_FULL_ADDRESS = 1
};

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
  int list_all;       /**< ^~l */
  int list_one;       /**<  ~l */
  int sub_all;        /**< ^~u */
  int sub_one;        /**<  ~u */
  int pers_recip_all; /**< ^~p */
  int pers_recip_one; /**<  ~p */
  int pers_from_all;  /**< ^~P */
  int pers_from_one;  /**<  ~P */
};

static inline struct Pattern *new_pattern(void)
{
  return safe_calloc(1, sizeof(struct Pattern));
}

int mutt_pattern_exec(struct Pattern *pat, enum PatternExecFlag flags,
                      struct Context *ctx, struct Header *h, struct PatternCache *cache);
struct Pattern *mutt_pattern_comp(/* const */ char *s, int flags, struct Buffer *err);
void mutt_check_simple(char *s, size_t len, const char *simple);
void mutt_pattern_free(struct Pattern **pat);

int mutt_which_case(const char *s);
int mutt_is_list_recipient(int alladdr, struct Address *a1, struct Address *a2);
int mutt_is_list_cc(int alladdr, struct Address *a1, struct Address *a2);
int mutt_pattern_func(int op, char *prompt);
int mutt_search_command(int cur, int op);

bool mutt_limit_current_thread(struct Header *h);

#endif /* _MUTT_PATTERN_H */
