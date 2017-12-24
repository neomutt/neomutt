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

/**
 * @page regex Manage regular expressions
 *
 * Manage regular expressions.
 *
 * | Function                  | Description
 * | :------------------------ | :------------------------------------------
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "debug.h"
#include "mbyte.h"
#include "memory.h"
#include "message.h"
#include "regex3.h"
#include "string2.h"

struct Regex *mutt_regex_compile(const char *s, int flags)
{
  struct Regex *pp = mutt_mem_calloc(1, sizeof(struct Regex));
  pp->pattern = mutt_str_strdup(s);
  pp->regex = mutt_mem_calloc(1, sizeof(regex_t));
  if (REGCOMP(pp->regex, NONULL(s), flags) != 0)
    mutt_regex_free(&pp);

  return pp;
}

/**
 * mutt_regex_create - Create an Regex from a string
 * @param str   Regular expression
 * @param flags Type flags, e.g. #DT_REGEX_MATCH_CASE
 * @param err   Buffer for error messages
 * @retval ptr New Regex object
 * @retval NULL Error
 */
struct Regex *mutt_regex_create(const char *str, int flags, struct Buffer *err)
{
  if (!str)
    return NULL;

  int rflags = 0;
  struct Regex *reg = mutt_mem_calloc(1, sizeof(struct Regex));

  reg->regex = mutt_mem_calloc(1, sizeof(regex_t));
  reg->pattern = mutt_str_strdup(str);

  /* Should we use smart case matching? */
  if (((flags & DT_REGEX_MATCH_CASE) == 0) && mutt_mb_is_lower(str))
    rflags |= REG_ICASE;

  /* Is a prefix of '!' allowed? */
  if (((flags & DT_REGEX_ALLOW_NOT) != 0) && (str[0] == '!'))
  {
    reg->not = true;
    str++;
  }

  int rc = REGCOMP(reg->regex, str, rflags);
  if ((rc != 0) && err)
  {
    regerror(rc, reg->regex, err->data, err->dsize);
    mutt_regex_free(&reg);
    return NULL;
  }

  return reg;
}

void mutt_regex_free(struct Regex **pp)
{
  FREE(&(*pp)->pattern);
  regfree((*pp)->regex);
  FREE(&(*pp)->regex);
  FREE(pp);
}

int mutt_regexlist_add(struct RegexList **list, const char *s, int flags, struct Buffer *err)
{
  struct RegexList *t = NULL, *last = NULL;
  struct Regex *rx = NULL;

  if (!s || !*s)
    return 0;

  rx = mutt_regex_compile(s, flags);
  if (!rx)
  {
    snprintf(err->data, err->dsize, "Bad regex: %s\n", s);
    return -1;
  }

  /* check to make sure the item is not already on this list */
  for (last = *list; last; last = last->next)
  {
    if (mutt_str_strcasecmp(rx->pattern, last->regex->pattern) == 0)
    {
      /* already on the list, so just ignore it */
      last = NULL;
      break;
    }
    if (!last->next)
      break;
  }

  if (!*list || last)
  {
    t = mutt_regexlist_new();
    t->regex = rx;
    if (last)
    {
      last->next = t;
      last = last->next;
    }
    else
      *list = last = t;
  }
  else /* duplicate */
    mutt_regex_free(&rx);

  return 0;
}

void mutt_regexlist_free(struct RegexList **list)
{
  struct RegexList *p = NULL;

  if (!list)
    return;
  while (*list)
  {
    p = *list;
    *list = (*list)->next;
    mutt_regex_free(&p->regex);
    FREE(&p);
  }
}

bool mutt_regexlist_match(const char *s, struct RegexList *l)
{
  if (!s)
    return false;

  for (; l; l = l->next)
  {
    if (regexec(l->regex->regex, s, (size_t) 0, (regmatch_t *) 0, (int) 0) == 0)
    {
      mutt_debug(5, "%s matches %s\n", s, l->regex->pattern);
      return true;
    }
  }

  return false;
}

struct RegexList *mutt_regexlist_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct RegexList));
}

int mutt_regexlist_remove(struct RegexList **l, const char *str)
{
  struct RegexList *p = NULL, *last = NULL;
  int rc = -1;

  if (mutt_str_strcmp("*", str) == 0)
  {
    mutt_regexlist_free(l); /* ``unCMD *'' means delete all current entries */
    rc = 0;
  }
  else
  {
    p = *l;
    last = NULL;
    while (p)
    {
      if (mutt_str_strcasecmp(str, p->regex->pattern) == 0)
      {
        mutt_regex_free(&p->regex);
        if (last)
          last->next = p->next;
        else
          (*l) = p->next;
        FREE(&p);
        rc = 0;
      }
      else
      {
        last = p;
        p = p->next;
      }
    }
  }
  return rc;
}

int mutt_replacelist_add(struct ReplaceList **list, const char *pat,
                         const char *templ, struct Buffer *err)
{
  struct ReplaceList *t = NULL, *last = NULL;
  struct Regex *rx = NULL;
  int n;
  const char *p = NULL;

  if (!pat || !*pat || !templ)
    return 0;

  rx = mutt_regex_compile(pat, REG_ICASE);
  if (!rx)
  {
    snprintf(err->data, err->dsize, _("Bad regex: %s"), pat);
    return -1;
  }

  /* check to make sure the item is not already on this list */
  for (last = *list; last; last = last->next)
  {
    if (mutt_str_strcasecmp(rx->pattern, last->regex->pattern) == 0)
    {
      /* Already on the list. Formerly we just skipped this case, but
       * now we're supporting removals, which means we're supporting
       * re-adds conceptually. So we probably want this to imply a
       * removal, then do an add. We can achieve the removal by freeing
       * the template, and leaving t pointed at the current item.
       */
      t = last;
      FREE(&t->template);
      break;
    }
    if (!last->next)
      break;
  }

  /* If t is set, it's pointing into an extant ReplaceList* that we want to
   * update. Otherwise we want to make a new one to link at the list's end.
   */
  if (!t)
  {
    t = mutt_replacelist_new();
    t->regex = rx;
    rx = NULL;
    if (last)
      last->next = t;
    else
      *list = t;
  }
  else
    mutt_regex_free(&rx);

  /* Now t is the ReplaceList* that we want to modify. It is prepared. */
  t->template = mutt_str_strdup(templ);

  /* Find highest match number in template string */
  t->nmatch = 0;
  for (p = templ; *p;)
  {
    if (*p == '%')
    {
      n = atoi(++p);
      if (n > t->nmatch)
        t->nmatch = n;
      while (*p && isdigit((int) *p))
        p++;
    }
    else
      p++;
  }

  if (t->nmatch > t->regex->regex->re_nsub)
  {
    snprintf(err->data, err->dsize, "%s", _("Not enough subexpressions for "
                                            "template"));
    mutt_replacelist_remove(list, pat);
    return -1;
  }

  t->nmatch++; /* match 0 is always the whole expr */

  return 0;
}

/**
 * mutt_replacelist_apply - Apply replacements to a buffer
 *
 * Note this function uses a fixed size buffer of LONG_STRING and so
 * should only be used for visual modifications, such as disp_subj.
 */
char *mutt_replacelist_apply(char *dbuf, size_t dlen, char *sbuf, struct ReplaceList *rlist)
{
  struct ReplaceList *l = NULL;
  static regmatch_t *pmatch = NULL;
  static int nmatch = 0;
  static char twinbuf[2][LONG_STRING];
  int switcher = 0;
  char *p = NULL;
  int n;
  size_t cpysize, tlen;
  char *src = NULL, *dst = NULL;

  if (dbuf && dlen)
    dbuf[0] = '\0';

  if (sbuf == NULL || *sbuf == '\0' || (dbuf && !dlen))
    return dbuf;

  twinbuf[0][0] = '\0';
  twinbuf[1][0] = '\0';
  src = twinbuf[switcher];
  dst = src;

  mutt_str_strfcpy(src, sbuf, LONG_STRING);

  for (l = rlist; l; l = l->next)
  {
    /* If this pattern needs more matches, expand pmatch. */
    if (l->nmatch > nmatch)
    {
      mutt_mem_realloc(&pmatch, l->nmatch * sizeof(regmatch_t));
      nmatch = l->nmatch;
    }

    if (regexec(l->regex->regex, src, l->nmatch, pmatch, 0) == 0)
    {
      tlen = 0;
      switcher ^= 1;
      dst = twinbuf[switcher];

      mutt_debug(5, "%s matches %s\n", src, l->regex->pattern);

      /* Copy into other twinbuf with substitutions */
      if (l->template)
      {
        for (p = l->template; *p && (tlen < LONG_STRING - 1);)
        {
          if (*p == '%')
          {
            p++;
            if (*p == 'L')
            {
              p++;
              cpysize = MIN(pmatch[0].rm_so, LONG_STRING - tlen - 1);
              strncpy(&dst[tlen], src, cpysize);
              tlen += cpysize;
            }
            else if (*p == 'R')
            {
              p++;
              cpysize = MIN(strlen(src) - pmatch[0].rm_eo, LONG_STRING - tlen - 1);
              strncpy(&dst[tlen], &src[pmatch[0].rm_eo], cpysize);
              tlen += cpysize;
            }
            else
            {
              n = strtoul(p, &p, 10);             /* get subst number */
              while (isdigit((unsigned char) *p)) /* skip subst token */
                p++;
              for (int i = pmatch[n].rm_so;
                   (i < pmatch[n].rm_eo) && (tlen < LONG_STRING - 1); i++)
                dst[tlen++] = src[i];
            }
          }
          else
            dst[tlen++] = *p++;
        }
      }
      dst[tlen] = '\0';
      mutt_debug(5, "subst %s\n", dst);
    }
    src = dst;
  }

  if (dbuf)
    mutt_str_strfcpy(dbuf, dst, dlen);
  else
    dbuf = mutt_str_strdup(dst);
  return dbuf;
}

void mutt_replacelist_free(struct ReplaceList **list)
{
  struct ReplaceList *p = NULL;

  if (!list)
    return;
  while (*list)
  {
    p = *list;
    *list = (*list)->next;
    mutt_regex_free(&p->regex);
    FREE(&p->template);
    FREE(&p);
  }
}

/**
 * mutt_replacelist_match - Does a string match a spam pattern
 * @param s        String to check
 * @param l        List of spam patterns
 * @param text     Buffer to save match
 * @param textsize Buffer length
 * @retval true if \a s matches a pattern in \a l
 * @retval false otherwise
 *
 * Match a string against the patterns defined by the 'spam' command and output
 * the expanded format into `text` when there is a match.  If textsize<=0, the
 * match is performed but the format is not expanded and no assumptions are made
 * about the value of `text` so it may be NULL.
 */
bool mutt_replacelist_match(const char *s, struct ReplaceList *l, char *text, int textsize)
{
  static regmatch_t *pmatch = NULL;
  static int nmatch = 0;
  int tlen = 0;
  char *p = NULL;

  if (!s)
    return false;

  for (; l; l = l->next)
  {
    /* If this pattern needs more matches, expand pmatch. */
    if (l->nmatch > nmatch)
    {
      mutt_mem_realloc(&pmatch, l->nmatch * sizeof(regmatch_t));
      nmatch = l->nmatch;
    }

    /* Does this pattern match? */
    if (regexec(l->regex->regex, s, (size_t) l->nmatch, (regmatch_t *) pmatch, (int) 0) == 0)
    {
      mutt_debug(5, "%s matches %s\n", s, l->regex->pattern);
      mutt_debug(5, "%d subs\n", (int) l->regex->regex->re_nsub);

      /* Copy template into text, with substitutions. */
      for (p = l->template; *p && tlen < textsize - 1;)
      {
        /* backreference to pattern match substring, eg. %1, %2, etc) */
        if (*p == '%')
        {
          char *e = NULL; /* used as pointer to end of integer backreference in strtol() call */
          int n;

          p++; /* skip over % char */
          n = strtol(p, &e, 10);
          /* Ensure that the integer conversion succeeded (e!=p) and bounds check.  The upper bound check
           * should not strictly be necessary since add_to_spam_list() finds the largest value, and
           * the static array above is always large enough based on that value. */
          if (e != p && n >= 0 && n <= l->nmatch && pmatch[n].rm_so != -1)
          {
            /* copy as much of the substring match as will fit in the output buffer, saving space for
             * the terminating nul char */
            int idx;
            for (idx = pmatch[n].rm_so;
                 (idx < pmatch[n].rm_eo) && (tlen < textsize - 1); ++idx)
              text[tlen++] = s[idx];
          }
          p = e; /* skip over the parsed integer */
        }
        else
        {
          text[tlen++] = *p++;
        }
      }
      /* tlen should always be less than textsize except when textsize<=0
       * because the bounds checks in the above code leave room for the
       * terminal nul char.   This should avoid returning an unterminated
       * string to the caller.  When textsize<=0 we make no assumption about
       * the validity of the text pointer. */
      if (tlen < textsize)
      {
        text[tlen] = '\0';
        mutt_debug(5, "\"%s\"\n", text);
      }
      return true;
    }
  }

  return false;
}

struct ReplaceList *mutt_replacelist_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ReplaceList));
}

int mutt_replacelist_remove(struct ReplaceList **list, const char *pat)
{
  struct ReplaceList *cur = NULL, *prev = NULL;
  int nremoved = 0;

  /* Being first is a special case. */
  cur = *list;
  if (!cur)
    return 0;
  if (cur->regex && (mutt_str_strcmp(cur->regex->pattern, pat) == 0))
  {
    *list = cur->next;
    mutt_regex_free(&cur->regex);
    FREE(&cur->template);
    FREE(&cur);
    return 1;
  }

  prev = cur;
  for (cur = prev->next; cur;)
  {
    if (mutt_str_strcmp(cur->regex->pattern, pat) == 0)
    {
      prev->next = cur->next;
      mutt_regex_free(&cur->regex);
      FREE(&cur->template);
      FREE(&cur);
      cur = prev->next;
      nremoved++;
    }
    else
      cur = cur->next;
  }

  return nremoved;
}
