/**
 * @file
 * RFC2231 MIME Charset routines
 *
 * @authors
 * Copyright (C) 1999-2001 Thomas Roessler <roessler@does-not-exist.org>
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

/* Yet another MIME encoding for header data.  This time, it's parameters,
 * specified in RFC2231, and modeled after the encoding used in URLs.
 *
 * Additionally, continuations and encoding are mixed in an, errrm, interesting
 * manner.
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/mutt.h"
#include "rfc2231.h"
#include "globals.h"
#include "options.h"

/**
 * struct Rfc2231Parameter - MIME section parameter
 */
struct Rfc2231Parameter
{
  char *attribute;
  char *value;
  int index;
  bool encoded;
  struct Rfc2231Parameter *next;
};

static void purge_empty_parameters(struct ParameterList *p)
{
  struct Parameter *np, *tmp;
  TAILQ_FOREACH_SAFE(np, p, entries, tmp)
  {
    if (!np->attribute || !np->value)
    {
      TAILQ_REMOVE(p, np, entries);
      mutt_param_free_one(&np);
    }
  }
}

static char *rfc2231_get_charset(char *value, char *charset, size_t chslen)
{
  char *t = strchr(value, '\'');
  if (!t)
  {
    charset[0] = '\0';
    return value;
  }

  *t = '\0';
  mutt_str_strfcpy(charset, value, chslen);

  char *u = strchr(t + 1, '\'');
  if (u)
    return u + 1;
  else
    return t + 1;
}

static void rfc2231_decode_one(char *dest, char *src)
{
  char *d = NULL;

  for (d = dest; *src; src++)
  {
    if (*src == '%' && isxdigit((unsigned char) *(src + 1)) &&
        isxdigit((unsigned char) *(src + 2)))
    {
      *d++ = (hexval(*(src + 1)) << 4) | (hexval(*(src + 2)));
      src += 2;
    }
    else
      *d++ = *src;
  }

  *d = '\0';
}

static struct Rfc2231Parameter *rfc2231_new_parameter(void)
{
  return mutt_mem_calloc(1, sizeof(struct Rfc2231Parameter));
}

/**
 * rfc2231_list_insert - insert parameter into an ordered list
 *
 * Primary sorting key: attribute
 * Secondary sorting key: index
 */
static void rfc2231_list_insert(struct Rfc2231Parameter **list, struct Rfc2231Parameter *par)
{
  struct Rfc2231Parameter **last = list;
  struct Rfc2231Parameter *p = *list;

  while (p)
  {
    const int c = strcmp(par->attribute, p->attribute);
    if ((c < 0) || (c == 0 && par->index <= p->index))
      break;

    last = &p->next;
    p = p->next;
  }

  par->next = p;
  *last = par;
}

static void rfc2231_free_parameter(struct Rfc2231Parameter **p)
{
  if (*p)
  {
    FREE(&(*p)->attribute);
    FREE(&(*p)->value);
    FREE(p);
  }
}

/**
 * rfc2231_join_continuations - process continuation parameters
 */
static void rfc2231_join_continuations(struct ParameterList *p, struct Rfc2231Parameter *par)
{
  char attribute[STRING];
  char charset[STRING];

  while (par)
  {
    char *value = NULL;
    size_t l = 0;

    mutt_str_strfcpy(attribute, par->attribute, sizeof(attribute));

    const bool encoded = par->encoded;
    char *valp;
    if (encoded)
      valp = rfc2231_get_charset(par->value, charset, sizeof(charset));
    else
      valp = par->value;

    do
    {
      if (encoded && par->encoded)
        rfc2231_decode_one(par->value, valp);

      const size_t vl = strlen(par->value);

      mutt_mem_realloc(&value, l + vl + 1);
      strcpy(value + l, par->value);
      l += vl;

      struct Rfc2231Parameter *q = par->next;
      rfc2231_free_parameter(&par);
      par = q;
      if (par)
        valp = par->value;
    } while (par && (strcmp(par->attribute, attribute) == 0));

    if (encoded)
      mutt_ch_convert_string(&value, charset, Charset, MUTT_ICONV_HOOK_FROM);

    struct Parameter *np = mutt_param_new();
    np->attribute = mutt_str_strdup(attribute);
    np->value = value;
    TAILQ_INSERT_HEAD(p, np, entries);
  }
}

void rfc2231_decode_parameters(struct ParameterList *p)
{
  struct Rfc2231Parameter *conthead = NULL;
  struct Rfc2231Parameter *conttmp = NULL;

  char *s = NULL, *t = NULL;
  char charset[STRING];

  bool encoded;
  int index;
  bool dirty = false; /* set to 1 when we may have created
                       * empty parameters. */
  if (!p)
    return;

  purge_empty_parameters(p);

  struct Parameter *np, *tmp;
  TAILQ_FOREACH_SAFE(np, p, entries, tmp)
  {
    s = strchr(np->attribute, '*');
    if (!s)
    {
      /*
       * Using RFC2047 encoding in MIME parameters is explicitly
       * forbidden by that document.  Nevertheless, it's being
       * generated by some software, including certain Lotus Notes to
       * Internet Gateways.  So we actually decode it.
       */

      if (Rfc2047Parameters && np->value && strstr(np->value, "=?"))
        mutt_rfc2047_decode(&np->value);
      else if (AssumedCharset && *AssumedCharset)
        mutt_ch_convert_nonmime_string(&np->value);
    }
    else if (*(s + 1) == '\0')
    {
      *s = '\0';

      s = rfc2231_get_charset(np->value, charset, sizeof(charset));
      rfc2231_decode_one(np->value, s);
      mutt_ch_convert_string(&np->value, charset, Charset, MUTT_ICONV_HOOK_FROM);
      mutt_mb_filter_unprintable(&np->value);
      dirty = true;
    }
    else
    {
      *s = '\0';
      s++; /* let s point to the first character of index. */
      for (t = s; *t && isdigit((unsigned char) *t); t++)
        ;
      encoded = (*t == '*');
      *t = '\0';

      index = atoi(s);

      conttmp = rfc2231_new_parameter();
      conttmp->attribute = np->attribute;
      conttmp->value = np->value;
      conttmp->encoded = encoded;
      conttmp->index = index;

      np->attribute = NULL;
      np->value = NULL;
      TAILQ_REMOVE(p, np, entries);
      FREE(&np);

      rfc2231_list_insert(&conthead, conttmp);
    }
  }

  if (conthead)
  {
    rfc2231_join_continuations(p, conthead);
    dirty = true;
  }

  if (dirty)
    purge_empty_parameters(p);
}

int rfc2231_encode_string(char **pd)
{
  int ext = 0, encode = 0;
  char *charset = NULL, *s = NULL, *t = NULL, *e = NULL, *d = NULL;
  size_t slen, dlen = 0;

  /*
   * A shortcut to detect pure 7bit data.
   *
   * This should prevent the worst when character set handling
   * is flawed.
   */

  for (s = *pd; *s; s++)
    if (*s & 0x80)
      break;

  if (!*s)
    return 0;

  if (!Charset || !SendCharset ||
      !(charset = mutt_ch_choose(Charset, SendCharset, *pd, strlen(*pd), &d, &dlen)))
  {
    charset = mutt_str_strdup(Charset ? Charset : "unknown-8bit");
    FREE(&d);
    d = *pd;
    dlen = strlen(d);
  }

  if (!mutt_ch_is_us_ascii(charset))
    encode = 1;

  for (s = d, slen = dlen; slen; s++, slen--)
  {
    if (*s < 0x20 || *s >= 0x7f)
    {
      encode = 1;
      ext++;
    }
    else if (strchr(MimeSpecials, *s) || strchr("*'%", *s))
      ext++;
  }

  if (encode)
  {
    e = mutt_mem_malloc(dlen + 2 * ext + strlen(charset) + 3);
    sprintf(e, "%s''", charset);
    t = e + strlen(e);
    for (s = d, slen = dlen; slen; s++, slen--)
    {
      if (*s < 0x20 || *s >= 0x7f || strchr(MimeSpecials, *s) || strchr("*'%", *s))
      {
        sprintf(t, "%%%02X", (unsigned char) *s);
        t += 3;
      }
      else
      {
        *t++ = *s;
      }
    }
    *t = '\0';

    if (d != *pd)
      FREE(&d);
    FREE(pd);
    *pd = e;
  }
  else if (d != *pd)
  {
    FREE(pd);
    *pd = d;
  }

  FREE(&charset);

  return encode;
}
