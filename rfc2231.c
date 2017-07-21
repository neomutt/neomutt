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

/*
 * Yet another MIME encoding for header data.  This time, it's
 * parameters, specified in RFC2231, and modeled after the
 * encoding used in URLs.
 *
 * Additionally, continuations and encoding are mixed in an, errrm,
 * interesting manner.
 */

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rfc2231.h"
#include "charset.h"
#include "globals.h"
#include "lib.h"
#include "mbyte.h"
#include "mime.h"
#include "options.h"
#include "parameter.h"
#include "protos.h"
#include "rfc2047.h"

/**
 * struct Rfc2231Parameter - MIME section parameter
 */
struct Rfc2231Parameter
{
  char *attribute;
  char *value;
  int index;
  int encoded;
  struct Rfc2231Parameter *next;
};

static void purge_empty_parameters(struct Parameter **headp)
{
  struct Parameter *p = NULL, *q = NULL, **last = NULL;

  for (last = headp, p = *headp; p; p = q)
  {
    q = p->next;
    if (!p->attribute || !p->value)
    {
      *last = q;
      p->next = NULL;
      mutt_free_parameter(&p);
    }
    else
      last = &p->next;
  }
}


static char *rfc2231_get_charset(char *value, char *charset, size_t chslen)
{
  char *t = NULL, *u = NULL;

  if (!(t = strchr(value, '\'')))
  {
    charset[0] = '\0';
    return value;
  }

  *t = '\0';
  strfcpy(charset, value, chslen);

  if ((u = strchr(t + 1, '\'')))
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
  return safe_calloc(1, sizeof(struct Rfc2231Parameter));
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
  int c;

  while (p)
  {
    c = strcmp(par->attribute, p->attribute);
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
static void rfc2231_join_continuations(struct Parameter **head, struct Rfc2231Parameter *par)
{
  struct Rfc2231Parameter *q = NULL;

  char attribute[STRING];
  char charset[STRING];
  char *value = NULL;
  char *valp = NULL;
  int encoded;

  size_t l, vl;

  while (par)
  {
    value = NULL;
    l = 0;

    strfcpy(attribute, par->attribute, sizeof(attribute));

    if ((encoded = par->encoded))
      valp = rfc2231_get_charset(par->value, charset, sizeof(charset));
    else
      valp = par->value;

    do
    {
      if (encoded && par->encoded)
        rfc2231_decode_one(par->value, valp);

      vl = strlen(par->value);

      safe_realloc(&value, l + vl + 1);
      strcpy(value + l, par->value);
      l += vl;

      q = par->next;
      rfc2231_free_parameter(&par);
      if ((par = q))
        valp = par->value;
    } while (par && (strcmp(par->attribute, attribute) == 0));

    if (encoded)
      mutt_convert_string(&value, charset, Charset, MUTT_ICONV_HOOK_FROM);
    *head = mutt_new_parameter();
    (*head)->attribute = safe_strdup(attribute);
    (*head)->value = value;
    head = &(*head)->next;
  }
}

void rfc2231_decode_parameters(struct Parameter **headp)
{
  struct Parameter *head = NULL;
  struct Parameter **last = NULL;
  struct Parameter *p = NULL, *q = NULL;

  struct Rfc2231Parameter *conthead = NULL;
  struct Rfc2231Parameter *conttmp = NULL;

  char *s = NULL, *t = NULL;
  char charset[STRING];

  int encoded;
  int index;
  bool dirty = false; /* set to 1 when we may have created
                       * empty parameters. */
  if (!headp)
    return;

  purge_empty_parameters(headp);

  for (last = &head, p = *headp; p; p = q)
  {
    q = p->next;

    if (!(s = strchr(p->attribute, '*')))
    {
      /*
       * Using RFC2047 encoding in MIME parameters is explicitly
       * forbidden by that document.  Nevertheless, it's being
       * generated by some software, including certain Lotus Notes to
       * Internet Gateways.  So we actually decode it.
       */

      if (option(OPTRFC2047PARAMS) && p->value && strstr(p->value, "=?"))
        rfc2047_decode(&p->value);
      else if (AssumedCharset && *AssumedCharset)
        convert_nonmime_string(&p->value);

      *last = p;
      last = &p->next;
      p->next = NULL;
    }
    else if (*(s + 1) == '\0')
    {
      *s = '\0';

      s = rfc2231_get_charset(p->value, charset, sizeof(charset));
      rfc2231_decode_one(p->value, s);
      mutt_convert_string(&p->value, charset, Charset, MUTT_ICONV_HOOK_FROM);
      mutt_filter_unprintable(&p->value);

      *last = p;
      last = &p->next;
      p->next = NULL;

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
      conttmp->attribute = p->attribute;
      conttmp->value = p->value;
      conttmp->encoded = encoded;
      conttmp->index = index;

      p->attribute = NULL;
      p->value = NULL;
      FREE(&p);

      rfc2231_list_insert(&conthead, conttmp);
    }
  }

  if (conthead)
  {
    rfc2231_join_continuations(last, conthead);
    dirty = true;
  }

  *headp = head;

  if (dirty)
    purge_empty_parameters(headp);
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
      !(charset = mutt_choose_charset(Charset, SendCharset, *pd, strlen(*pd), &d, &dlen)))
  {
    charset = safe_strdup(Charset ? Charset : "unknown-8bit");
    FREE(&d);
    d = *pd;
    dlen = strlen(d);
  }

  if (!mutt_is_us_ascii(charset))
    encode = 1;

  for (s = d, slen = dlen; slen; s++, slen--)
    if (*s < 0x20 || *s >= 0x7f)
      encode = 1, ++ext;
    else if (strchr(MimeSpecials, *s) || strchr("*'%", *s))
      ++ext;

  if (encode)
  {
    e = safe_malloc(dlen + 2 * ext + strlen(charset) + 3);
    sprintf(e, "%s''", charset);
    t = e + strlen(e);
    for (s = d, slen = dlen; slen; s++, slen--)
      if (*s < 0x20 || *s >= 0x7f || strchr(MimeSpecials, *s) || strchr("*'%", *s))
      {
        sprintf(t, "%%%02X", (unsigned char) *s);
        t += 3;
      }
      else
        *t++ = *s;
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
