/**
 * @file
 * RFC2231 MIME Charset routines
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Federico Kircheis <federico.kircheis@gmail.com>
 * Copyright (C) 2018-2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page email_rfc2231 RFC2231 MIME Charset functions
 *
 * RFC2231 MIME Charset routines
 *
 * Yet another MIME encoding for header data.  This time, it's parameters,
 * specified in RFC2231, and modelled after the encoding used in URLs.
 *
 * Additionally, continuations and encoding are mixed in an, errrm, interesting
 * manner.
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "rfc2231.h"
#include "mime.h"
#include "parameter.h"
#include "rfc2047.h"

/**
 * struct Rfc2231Parameter - MIME section parameter
 */
struct Rfc2231Parameter
{
  char *attribute;               ///< Attribute name
  char *value;                   ///< Attribute value
  int index;                     ///< Index number in the list
  bool encoded;                  ///< Is the value encoded?
  struct Rfc2231Parameter *next; ///< Linked list
};

/**
 * purge_empty_parameters - Remove any ill-formed Parameters from a list
 * @param pl Parameter List to check
 */
static void purge_empty_parameters(struct ParameterList *pl)
{
  struct Parameter *np = NULL, *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, pl, entries, tmp)
  {
    if (!np->attribute || !np->value)
    {
      TAILQ_REMOVE(pl, np, entries);
      mutt_param_free_one(&np);
    }
  }
}

/**
 * get_charset - Get the charset from an RFC2231 header
 * @param value   Header string
 * @param charset Buffer for the result
 * @param chslen  Length of buffer
 * @retval ptr First character after charset
 */
static char *get_charset(char *value, char *charset, size_t chslen)
{
  char *t = strchr(value, '\'');
  if (!t)
  {
    charset[0] = '\0';
    return value;
  }

  *t = '\0';
  mutt_str_copy(charset, value, chslen);

  char *u = strchr(t + 1, '\'');
  if (u)
    return u + 1;
  return t + 1;
}

/**
 * decode_one - Decode one percent-encoded character
 * @param[out] dest Where to save the result
 * @param[in]  src  Source string
 */
static void decode_one(char *dest, char *src)
{
  char *d = NULL;

  for (d = dest; *src; src++)
  {
    if ((src[0] == '%') && isxdigit((unsigned char) src[1]) &&
        isxdigit((unsigned char) src[2]))
    {
      *d++ = (hexval(src[1]) << 4) | hexval(src[2]);
      src += 2;
    }
    else
    {
      *d++ = *src;
    }
  }

  *d = '\0';
}

/**
 * parameter_new - Create a new Rfc2231Parameter
 * @retval ptr Newly allocated Rfc2231Parameter
 */
static struct Rfc2231Parameter *parameter_new(void)
{
  return MUTT_MEM_CALLOC(1, struct Rfc2231Parameter);
}

/**
 * list_insert - Insert parameter into an ordered list
 * @param[out] list List to insert into
 * @param[in]  par  Parameter to insert
 *
 * Primary sorting key: attribute
 * Secondary sorting key: index
 */
static void list_insert(struct Rfc2231Parameter **list, struct Rfc2231Parameter *par)
{
  struct Rfc2231Parameter **last = list;
  struct Rfc2231Parameter *p = *list;

  while (p)
  {
    const int c = mutt_str_cmp(par->attribute, p->attribute);
    if ((c < 0) || ((c == 0) && (par->index <= p->index)))
      break;

    last = &p->next;
    p = p->next;
  }

  par->next = p;
  *last = par;
}

/**
 * parameter_free - Free an Rfc2231Parameter
 * @param[out] ptr Rfc2231Parameter to free
 */
static void parameter_free(struct Rfc2231Parameter **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Rfc2231Parameter *p = *ptr;
  FREE(&p->attribute);
  FREE(&p->value);

  FREE(ptr);
}

/**
 * join_continuations - Process continuation parameters
 * @param pl  Parameter List for the results
 * @param par Continuation Parameter
 */
static void join_continuations(struct ParameterList *pl, struct Rfc2231Parameter *par)
{
  char attribute[256] = { 0 };
  char charset[256] = { 0 };

  const char *const c_charset = cc_charset();
  while (par)
  {
    char *value = NULL;
    size_t l = 0;

    mutt_str_copy(attribute, par->attribute, sizeof(attribute));

    const bool encoded = par->encoded;
    char *valp = NULL;
    if (encoded)
      valp = get_charset(par->value, charset, sizeof(charset));
    else
      valp = par->value;

    do
    {
      if (encoded && par->encoded)
        decode_one(par->value, valp);

      const size_t vl = strlen(par->value);

      MUTT_MEM_REALLOC(&value, l + vl + 1, char);
      strcpy(value + l, par->value);
      l += vl;

      struct Rfc2231Parameter *q = par->next;
      parameter_free(&par);
      par = q;
      if (par)
        valp = par->value;
    } while (par && (mutt_str_equal(par->attribute, attribute)));

    if (encoded)
    {
      mutt_ch_convert_string(&value, charset, c_charset, MUTT_ICONV_HOOK_FROM);
      mutt_mb_filter_unprintable(&value);
    }

    struct Parameter *np = mutt_param_new();
    np->attribute = mutt_str_dup(attribute);
    np->value = value;
    TAILQ_INSERT_HEAD(pl, np, entries);
  }
}

/**
 * rfc2231_decode_parameters - Decode a Parameter list
 * @param pl List to decode
 */
void rfc2231_decode_parameters(struct ParameterList *pl)
{
  if (!pl)
    return;

  struct Rfc2231Parameter *conthead = NULL;
  struct Rfc2231Parameter *conttmp = NULL;

  char *s = NULL, *t = NULL;
  char charset[256] = { 0 };

  bool encoded;
  int index;
  bool dirty = false; /* set when we may have created empty parameters. */

  purge_empty_parameters(pl);

  struct Parameter *np = NULL, *tmp = NULL;
  const bool c_rfc2047_parameters = cs_subset_bool(NeoMutt->sub, "rfc2047_parameters");
  const struct Slist *c_assumed_charset = cc_assumed_charset();
  const char *c_charset = cc_charset();

  TAILQ_FOREACH_SAFE(np, pl, entries, tmp)
  {
    s = strchr(np->attribute, '*');
    if (!s)
    {
      /* Single value, non encoded:
       *   attr=value
       */
      /* Using RFC2047 encoding in MIME parameters is explicitly
       * forbidden by that document.  Nevertheless, it's being
       * generated by some software, including certain Lotus Notes to
       * Internet Gateways.  So we actually decode it.  */

      if (c_rfc2047_parameters && np->value && strstr(np->value, "=?"))
      {
        rfc2047_decode(&np->value);
      }
      else if (!slist_is_empty(c_assumed_charset))
      {
        mutt_ch_convert_nonmime_string(c_assumed_charset, c_charset, &np->value);
      }
    }
    else if (s[1] == '\0')
    {
      /* Single value with encoding:
       *   attr*=us-ascii''the%20value
       */
      s[0] = '\0';

      s = get_charset(np->value, charset, sizeof(charset));
      decode_one(np->value, s);
      mutt_ch_convert_string(&np->value, charset, c_charset, MUTT_ICONV_HOOK_FROM);
      mutt_mb_filter_unprintable(&np->value);
      dirty = true;
    }
    else
    {
      /* A parameter continuation, which may or may not be encoded:
       *   attr*0=value
       *     -or-
       *   attr*0*=us-ascii''the%20value
       */
      s[0] = '\0';
      s++; /* let s point to the first character of index. */
      for (t = s; (t[0] != '\0') && isdigit((unsigned char) t[0]); t++)
        ; // do nothing

      encoded = (t[0] == '*');
      t[0] = '\0';

      /* RFC2231 says that the index starts at 0 and increments by 1,
       * thus an overflow should never occur in a valid message, thus
       * the value INT_MAX in case of overflow does not really matter
       * (the goal is just to avoid undefined behaviour). */
      if (!mutt_str_atoi_full(s, &index))
        index = INT_MAX;

      conttmp = parameter_new();
      conttmp->attribute = np->attribute;
      conttmp->value = np->value;
      conttmp->encoded = encoded;
      conttmp->index = index;

      np->attribute = NULL;
      np->value = NULL;
      TAILQ_REMOVE(pl, np, entries);
      FREE(&np);

      list_insert(&conthead, conttmp);
    }
  }

  if (conthead)
  {
    join_continuations(pl, conthead);
    dirty = true;
  }

  if (dirty)
    purge_empty_parameters(pl);
}

/**
 * rfc2231_encode_string - Encode a string to be suitable for an RFC2231 header
 * @param head      String encoded as a ParameterList, empty on error
 * @param attribute Name of attribute to encode
 * @param value     Value of attribute to encode
 * @retval num Number of Parameters in the List
 *
 * If the value is large, the list will contain continuation lines.
 */
size_t rfc2231_encode_string(struct ParameterList *head, const char *attribute, char *value)
{
  if (!attribute || !value)
    return 0;

  size_t count = 0;
  bool encode = false;
  bool add_quotes = false;
  bool free_src_value = false;
  bool split = false;
  int continuation_number = 0;
  size_t dest_value_len = 0, max_value_len = 0, cur_value_len = 0;
  char *cur = NULL, *charset = NULL, *src_value = NULL;
  struct Parameter *current = NULL;

  struct Buffer *cur_attribute = buf_pool_get();
  struct Buffer *cur_value = buf_pool_get();

  // Perform charset conversion
  for (cur = value; *cur; cur++)
  {
    if ((*cur < 0x20) || (*cur >= 0x7f))
    {
      encode = true;
      break;
    }
  }

  if (encode)
  {
    const struct Slist *const c_send_charset = cs_subset_slist(NeoMutt->sub, "send_charset");
    const char *c_charset = cc_charset();
    if (c_charset && c_send_charset)
    {
      charset = mutt_ch_choose(c_charset, c_send_charset, value,
                               mutt_str_len(value), &src_value, NULL);
    }
    if (src_value)
      free_src_value = true;
    if (!charset)
      charset = mutt_str_dup(c_charset ? c_charset : "unknown-8bit");
  }
  if (!src_value)
    src_value = value;

  // Count the size the resultant value will need in total
  if (encode)
    dest_value_len = mutt_str_len(charset) + 2; /* charset'' prefix */

  for (cur = src_value; *cur; cur++)
  {
    dest_value_len++;

    if (encode)
    {
      /* These get converted to %xx so need a total of three chars */
      if ((*cur < 0x20) || (*cur >= 0x7f) || strchr(MimeSpecials, *cur) ||
          strchr("*'%", *cur))
      {
        dest_value_len += 2;
      }
    }
    else
    {
      /* rfc822_cat() will add outer quotes if it finds MimeSpecials. */
      if (!add_quotes && strchr(MimeSpecials, *cur))
        add_quotes = true;
      /* rfc822_cat() will add a backslash if it finds '\' or '"'. */
      if ((*cur == '\\') || (*cur == '"'))
        dest_value_len++;
    }
  }

  // Determine if need to split into parameter value continuations
  max_value_len = 78 -                      // rfc suggested line length
                  1 -                       // Leading tab on continuation line
                  mutt_str_len(attribute) - // attribute
                  (encode ? 1 : 0) -        // '*' encoding marker
                  1 -                       // '='
                  (add_quotes ? 2 : 0) -    // "...."
                  1;                        // ';'

  if (max_value_len < 30)
    max_value_len = 30;

  if (dest_value_len > max_value_len)
  {
    split = true;
    max_value_len -= 4; /* '*n' continuation number and extra encoding
                         * space to keep loop below simpler */
  }

  // Generate list of parameter continuations
  cur = src_value;
  if (encode)
  {
    buf_printf(cur_value, "%s''", charset);
    cur_value_len = buf_len(cur_value);
  }

  while (*cur)
  {
    current = mutt_param_new();
    TAILQ_INSERT_TAIL(head, current, entries);
    count++;

    buf_strcpy(cur_attribute, attribute);
    if (split)
      buf_add_printf(cur_attribute, "*%d", continuation_number++);
    if (encode)
      buf_addch(cur_attribute, '*');

    while (*cur && (!split || (cur_value_len < max_value_len)))
    {
      if (encode)
      {
        if ((*cur < 0x20) || (*cur >= 0x7f) || strchr(MimeSpecials, *cur) ||
            strchr("*'%", *cur))
        {
          buf_add_printf(cur_value, "%%%02X", (unsigned char) *cur);
          cur_value_len += 3;
        }
        else
        {
          buf_addch(cur_value, *cur);
          cur_value_len++;
        }
      }
      else
      {
        buf_addch(cur_value, *cur);
        cur_value_len++;
        if ((*cur == '\\') || (*cur == '"'))
          cur_value_len++;
      }

      cur++;
    }

    current->attribute = buf_strdup(cur_attribute);
    current->value = buf_strdup(cur_value);

    buf_reset(cur_value);
    cur_value_len = 0;
  }

  buf_pool_release(&cur_attribute);
  buf_pool_release(&cur_value);

  FREE(&charset);
  if (free_src_value)
    FREE(&src_value);

  return count;
}
