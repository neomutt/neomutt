/**
 * @file
 * Manage regular expressions
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Bo Yu <tsu.yubo@gmail.com>
 * Copyright (C) 2018-2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Simon Symeonidis <lethaljellybean@gmail.com>
 * Copyright (C) 2022 наб <nabijaczleweli@nabijaczleweli.xyz>
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
 * @page mutt_regex Manage regular expressions
 *
 * Manage regular expressions.
 */

#include "config.h"
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "config/types.h"
#include "atoi.h"
#include "buffer.h"
#include "logging2.h"
#include "mbyte.h"
#include "memory.h"
#include "message.h"
#include "pool.h"
#include "queue.h"
#include "regex3.h"
#include "string2.h"

/**
 * mutt_regex_compile - Create an Regex from a string
 * @param str   Regular expression
 * @param flags Type flags, e.g. REG_ICASE
 * @retval ptr New Regex object
 * @retval NULL Error
 */
struct Regex *mutt_regex_compile(const char *str, uint16_t flags)
{
  if (!str || (*str == '\0'))
    return NULL;
  struct Regex *rx = MUTT_MEM_CALLOC(1, struct Regex);
  rx->pattern = mutt_str_dup(str);
  rx->regex = MUTT_MEM_CALLOC(1, regex_t);
  if (REG_COMP(rx->regex, str, flags) != 0)
    mutt_regex_free(&rx);

  return rx;
}

/**
 * mutt_regex_new - Create an Regex from a string
 * @param str   Regular expression
 * @param flags Type flags, e.g. #D_REGEX_MATCH_CASE
 * @param err   Buffer for error messages
 * @retval ptr New Regex object
 * @retval NULL Error
 */
struct Regex *mutt_regex_new(const char *str, uint32_t flags, struct Buffer *err)
{
  if (!str || (*str == '\0'))
    return NULL;

  uint16_t rflags = 0;
  struct Regex *reg = MUTT_MEM_CALLOC(1, struct Regex);

  reg->regex = MUTT_MEM_CALLOC(1, regex_t);
  reg->pattern = mutt_str_dup(str);

  /* Should we use smart case matching? */
  if (((flags & D_REGEX_MATCH_CASE) == 0) && mutt_mb_is_lower(str))
    rflags |= REG_ICASE;

  /* Is a prefix of '!' allowed? */
  if (((flags & D_REGEX_ALLOW_NOT) != 0) && (str[0] == '!'))
  {
    reg->pat_not = true;
    str++;
  }

  int rc = REG_COMP(reg->regex, str, rflags);
  if (rc != 0)
  {
    if (err)
      regerror(rc, reg->regex, err->data, err->dsize);
    mutt_regex_free(&reg);
    return NULL;
  }

  return reg;
}

/**
 * mutt_regex_free - Free a Regex object
 * @param[out] ptr Regex to free
 */
void mutt_regex_free(struct Regex **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Regex *rx = *ptr;
  FREE(&rx->pattern);
  if (rx->regex)
    regfree(rx->regex);
  FREE(&rx->regex);
  FREE(ptr);
}

/**
 * mutt_regexlist_add - Compile a regex string and add it to a list
 * @param rl    RegexList to add to
 * @param str   String to compile into a regex
 * @param flags Flags, e.g. REG_ICASE
 * @param err   Buffer for error messages
 * @retval 0  Success, Regex compiled and added to the list
 * @retval -1 Error, see message in 'err'
 */
int mutt_regexlist_add(struct RegexList *rl, const char *str, uint16_t flags,
                       struct Buffer *err)
{
  if (!rl || !str || (*str == '\0'))
    return 0;

  struct Regex *rx = mutt_regex_compile(str, flags);
  if (!rx)
  {
    buf_printf(err, "Bad regex: %s\n", str);
    return -1;
  }

  /* check to make sure the item is not already on this rl */
  struct RegexNode *np = NULL;
  STAILQ_FOREACH(np, rl, entries)
  {
    if (mutt_istr_equal(rx->pattern, np->regex->pattern))
      break; /* already on the rl */
  }

  if (np)
  {
    mutt_regex_free(&rx);
  }
  else
  {
    np = mutt_regexlist_new();
    np->regex = rx;
    STAILQ_INSERT_TAIL(rl, np, entries);
  }

  return 0;
}

/**
 * mutt_regexlist_free - Free a RegexList object
 * @param rl RegexList to free
 */
void mutt_regexlist_free(struct RegexList *rl)
{
  if (!rl)
    return;

  struct RegexNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, rl, entries, tmp)
  {
    STAILQ_REMOVE(rl, np, RegexNode, entries);
    mutt_regex_free(&np->regex);
    FREE(&np);
  }
  STAILQ_INIT(rl);
}

/**
 * mutt_regexlist_match - Does a string match any Regex in the list?
 * @param rl  RegexList to match against
 * @param str String to compare
 * @retval true String matches one of the Regexes in the list
 */
bool mutt_regexlist_match(struct RegexList *rl, const char *str)
{
  if (!rl || !str)
    return false;
  struct RegexNode *np = NULL;
  STAILQ_FOREACH(np, rl, entries)
  {
    if (mutt_regex_match(np->regex, str))
    {
      mutt_debug(LL_DEBUG5, "%s matches %s\n", str, np->regex->pattern);
      return true;
    }
  }

  return false;
}

/**
 * mutt_regexlist_new - Create a new RegexList
 * @retval ptr New RegexList object
 */
struct RegexNode *mutt_regexlist_new(void)
{
  return MUTT_MEM_CALLOC(1, struct RegexNode);
}

/**
 * mutt_regexlist_remove - Remove a Regex from a list
 * @param rl  RegexList to alter
 * @param str Pattern to remove from the list
 * @retval 0  Success, pattern was found and removed from the list
 * @retval -1 Error, pattern wasn't found
 *
 * If the pattern is "*", then all the Regexes are removed.
 */
int mutt_regexlist_remove(struct RegexList *rl, const char *str)
{
  if (!rl || !str)
    return -1;

  if (mutt_str_equal("*", str))
  {
    mutt_regexlist_free(rl); /* "unCMD *" means delete all current entries */
    return 0;
  }

  int rc = -1;
  struct RegexNode *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, rl, entries, tmp)
  {
    if (mutt_istr_equal(str, np->regex->pattern))
    {
      STAILQ_REMOVE(rl, np, RegexNode, entries);
      mutt_regex_free(&np->regex);
      FREE(&np);
      rc = 0;
    }
  }

  return rc;
}

/**
 * mutt_replacelist_add - Add a pattern and a template to a list
 * @param rl    ReplaceList to add to
 * @param pat   Pattern to compile into a regex
 * @param templ Template string to associate with the pattern
 * @param err   Buffer for error messages
 * @retval 0  Success, pattern added to the ReplaceList
 * @retval -1 Error, see message in 'err'
 */
int mutt_replacelist_add(struct ReplaceList *rl, const char *pat,
                         const char *templ, struct Buffer *err)
{
  if (!rl || !pat || (*pat == '\0') || !templ)
    return 0;

  struct Regex *rx = mutt_regex_compile(pat, REG_ICASE);
  if (!rx)
  {
    buf_printf(err, _("Bad regex: %s"), pat);
    return -1;
  }

  /* check to make sure the item is not already on this rl */
  struct Replace *np = NULL;
  STAILQ_FOREACH(np, rl, entries)
  {
    if (mutt_istr_equal(rx->pattern, np->regex->pattern))
    {
      /* Already on the rl. Formerly we just skipped this case, but
       * now we're supporting removals, which means we're supporting
       * re-adds conceptually. So we probably want this to imply a
       * removal, then do an add. We can achieve the removal by freeing
       * the template, and leaving t pointed at the current item.  */
      FREE(&np->templ);
      break;
    }
  }

  /* If np is set, it's pointing into an extant ReplaceList* that we want to
   * update. Otherwise we want to make a new one to link at the rl's end.  */
  if (np)
  {
    mutt_regex_free(&rx);
  }
  else
  {
    np = mutt_replacelist_new();
    np->regex = rx;
    rx = NULL;
    STAILQ_INSERT_TAIL(rl, np, entries);
  }

  /* Now np is the Replace that we want to modify. It is prepared. */
  np->templ = mutt_str_dup(templ);

  /* Find highest match number in template string */
  np->nmatch = 0;
  for (const char *p = templ; *p;)
  {
    if (*p == '%')
    {
      int n = 0;
      const char *end = mutt_str_atoi(++p, &n);
      if (!end)
      {
        // this is not an error, we might have matched %R or %L in subjectrx
        mutt_debug(LL_DEBUG2, "Invalid match number in replacelist: '%s'\n", p);
      }
      if (n > np->nmatch)
      {
        np->nmatch = n;
      }
      if (end)
      {
        p = end;
      }
      else
      {
        p++;
      }
    }
    else
    {
      p++;
    }
  }

  if (np->nmatch > np->regex->regex->re_nsub)
  {
    if (err)
      buf_addstr(err, _("Not enough subexpressions for template"));
    mutt_replacelist_remove(rl, pat);
    return -1;
  }

  np->nmatch++; /* match 0 is always the whole expr */
  return 0;
}

/**
 * mutt_replacelist_apply - Apply replacements to a buffer
 * @param rl     ReplaceList to apply
 * @param str    String to manipulate
 * @retval ptr New string with replacements
 *
 * @note Caller must free the returned string
 */
char *mutt_replacelist_apply(struct ReplaceList *rl, const char *str)
{
  if (!rl || !str || (*str == '\0'))
    return NULL;

  static regmatch_t *pmatch = NULL;
  static size_t nmatch = 0;
  char *p = NULL;

  struct Buffer *src = buf_pool_get();
  struct Buffer *dst = buf_pool_get();

  buf_strcpy(src, str);

  struct Replace *np = NULL;
  STAILQ_FOREACH(np, rl, entries)
  {
    /* If this pattern needs more matches, expand pmatch. */
    if (np->nmatch > nmatch)
    {
      mutt_mem_reallocarray(&pmatch, np->nmatch, sizeof(regmatch_t));
      nmatch = np->nmatch;
    }

    if (mutt_regex_capture(np->regex, buf_string(src), np->nmatch, pmatch))
    {
      mutt_debug(LL_DEBUG5, "%s matches %s\n", buf_string(src), np->regex->pattern);

      buf_reset(dst);
      if (np->templ)
      {
        for (p = np->templ; *p;)
        {
          if (*p == '%')
          {
            p++;
            if (*p == 'L')
            {
              p++;
              buf_addstr_n(dst, buf_string(src), pmatch[0].rm_so);
            }
            else if (*p == 'R')
            {
              p++;
              buf_addstr(dst, src->data + pmatch[0].rm_eo);
            }
            else
            {
              long n = strtoul(p, &p, 10); /* get subst number */
              if (n < np->nmatch)
              {
                buf_addstr_n(dst, src->data + pmatch[n].rm_so,
                             pmatch[n].rm_eo - pmatch[n].rm_so);
              }
              while (isdigit((unsigned char) *p)) /* skip subst token */
                p++;
            }
          }
          else
          {
            buf_addch(dst, *p++);
          }
        }
      }

      buf_strcpy(src, buf_string(dst));
      mutt_debug(LL_DEBUG5, "subst %s\n", buf_string(dst));
    }
  }

  char *result = buf_strdup(src);

  buf_pool_release(&src);
  buf_pool_release(&dst);
  return result;
}

/**
 * mutt_replacelist_free - Free a ReplaceList object
 * @param rl ReplaceList to free
 */
void mutt_replacelist_free(struct ReplaceList *rl)
{
  if (!rl)
    return;

  struct Replace *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, rl, entries, tmp)
  {
    STAILQ_REMOVE(rl, np, Replace, entries);
    mutt_regex_free(&np->regex);
    FREE(&np->templ);
    FREE(&np);
  }
}

/**
 * mutt_replacelist_match - Does a string match a pattern?
 * @param rl     ReplaceList of patterns
 * @param buf    Buffer to save match
 * @param buflen Buffer length
 * @param str    String to check
 * @retval true String matches a patterh in the ReplaceList
 *
 * Match a string against the patterns defined by the 'spam' command and output
 * the expanded format into `buf` when there is a match.  If buflen<=0, the
 * match is performed but the format is not expanded and no assumptions are
 * made about the value of `buf` so it may be NULL.
 */
bool mutt_replacelist_match(struct ReplaceList *rl, char *buf, size_t buflen, const char *str)
{
  if (!rl || !buf || !str)
    return false;

  static regmatch_t *pmatch = NULL;
  static size_t nmatch = 0;
  int tlen = 0;
  char *p = NULL;

  struct Replace *np = NULL;
  STAILQ_FOREACH(np, rl, entries)
  {
    /* If this pattern needs more matches, expand pmatch. */
    if (np->nmatch > nmatch)
    {
      mutt_mem_reallocarray(&pmatch, np->nmatch, sizeof(regmatch_t));
      nmatch = np->nmatch;
    }

    /* Does this pattern match? */
    if (mutt_regex_capture(np->regex, str, (size_t) np->nmatch, pmatch))
    {
      mutt_debug(LL_DEBUG5, "%s matches %s\n", str, np->regex->pattern);
      mutt_debug(LL_DEBUG5, "%d subs\n", (int) np->regex->regex->re_nsub);

      /* Copy template into buf, with substitutions. */
      for (p = np->templ; *p && (tlen < (buflen - 1));)
      {
        /* backreference to pattern match substring, eg. %1, %2, etc) */
        if (*p == '%')
        {
          char *e = NULL; /* used as pointer to end of integer backreference in strtol() call */

          p++; /* skip over % char */
          long n = strtol(p, &e, 10);
          /* Ensure that the integer conversion succeeded (e!=p) and bounds check.  The upper bound check
           * should not strictly be necessary since add_to_spam_list() finds the largest value, and
           * the static array above is always large enough based on that value. */
          if ((e != p) && (n >= 0) && (n < np->nmatch) && (pmatch[n].rm_so != -1))
          {
            /* copy as much of the substring match as will fit in the output buffer, saving space for
             * the terminating nul char */
            for (int idx = pmatch[n].rm_so;
                 (idx < pmatch[n].rm_eo) && (tlen < (buflen - 1)); idx++)
            {
              buf[tlen++] = str[idx];
            }
          }
          p = e; /* skip over the parsed integer */
        }
        else
        {
          buf[tlen++] = *p++;
        }
      }
      /* tlen should always be less than buflen except when buflen<=0
       * because the bounds checks in the above code leave room for the
       * terminal nul char.   This should avoid returning an unterminated
       * string to the caller.  When buflen<=0 we make no assumption about
       * the validity of the buf pointer. */
      if (tlen < buflen)
      {
        buf[tlen] = '\0';
        mutt_debug(LL_DEBUG5, "\"%s\"\n", buf);
      }
      return true;
    }
  }

  return false;
}

/**
 * mutt_replacelist_new - Create a new ReplaceList
 * @retval ptr New ReplaceList
 */
struct Replace *mutt_replacelist_new(void)
{
  return MUTT_MEM_CALLOC(1, struct Replace);
}

/**
 * mutt_replacelist_remove - Remove a pattern from a list
 * @param rl  ReplaceList to modify
 * @param pat Pattern to remove
 * @retval num Matching patterns removed
 */
int mutt_replacelist_remove(struct ReplaceList *rl, const char *pat)
{
  if (!rl || !pat)
    return 0;

  int nremoved = 0;
  struct Replace *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, rl, entries, tmp)
  {
    if (mutt_str_equal(np->regex->pattern, pat))
    {
      STAILQ_REMOVE(rl, np, Replace, entries);
      mutt_regex_free(&np->regex);
      FREE(&np->templ);
      FREE(&np);
      nremoved++;
    }
  }

  return nremoved;
}

/**
 * mutt_regex_capture - Match a regex against a string, with provided options
 * @param regex   Regex to execute
 * @param str     String to apply regex on
 * @param nmatch  Length of matches
 * @param matches regmatch_t to hold match indices
 * @retval true  str matches
 * @retval false str does not match
 */
bool mutt_regex_capture(const struct Regex *regex, const char *str,
                        size_t nmatch, regmatch_t matches[])
{
  if (!regex || !str || !regex->regex)
    return false;

  int rc = regexec(regex->regex, str, nmatch, matches, 0);
  return ((rc == 0) ^ regex->pat_not);
}

/**
 * mutt_regex_match - Shorthand to mutt_regex_capture()
 * @param regex Regex which is desired to match against
 * @param str   String to search with given regex
 * @retval true  str matches
 * @retval false str does not match
 */
bool mutt_regex_match(const struct Regex *regex, const char *str)
{
  return mutt_regex_capture(regex, str, 0, NULL);
}
