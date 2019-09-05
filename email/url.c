/**
 * @file
 * Parse and identify different URL schemes
 *
 * @authors
 * Copyright (C) 2000-2002,2004 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page email_url Parse and identify different URL schemes
 *
 * Parse and identify different URL schemes
 */

#include "config.h"
#include <ctype.h>
#include <string.h>
#include "mutt/mutt.h"
#include "url.h"
#include "mime.h"

/**
 * UrlMap - Constants for URL protocols
 */
static const struct Mapping UrlMap[] = {
  { "file", U_FILE },   { "imap", U_IMAP },     { "imaps", U_IMAPS },
  { "pop", U_POP },     { "pops", U_POPS },     { "news", U_NNTP },
  { "snews", U_NNTPS }, { "mailto", U_MAILTO }, { "notmuch", U_NOTMUCH },
  { "smtp", U_SMTP },   { "smtps", U_SMTPS },   { NULL, U_UNKNOWN },
};

/**
 * parse_query_string - Parse a URL query string
 * @param list List to store the results
 * @param src  String to parse
 * @retval  0 Success
 * @retval -1 Error
 */
static int parse_query_string(struct UrlQueryList *list, char *src)
{
  struct UrlQuery *qs = NULL;
  char *k = NULL, *v = NULL;

  while (src && *src)
  {
    qs = mutt_mem_calloc(1, sizeof(struct UrlQuery));
    k = strchr(src, '&');
    if (k)
      *k = '\0';

    v = strchr(src, '=');
    if (v)
    {
      *v = '\0';
      qs->value = v + 1;
      if (url_pct_decode(qs->value) < 0)
      {
        FREE(&qs);
        return -1;
      }
    }
    qs->name = src;
    if (url_pct_decode(qs->name) < 0)
    {
      FREE(&qs);
      return -1;
    }
    STAILQ_INSERT_TAIL(list, qs, entries);

    if (!k)
      break;
    src = k + 1;
  }
  return 0;
}

/**
 * url_pct_decode - Decode a percent-encoded string
 * @param s String to decode
 * @retval  0 Success
 * @retval -1 Error
 *
 * e.g. turn "hello%20world" into "hello world"
 * The string is decoded in-place.
 */
int url_pct_decode(char *s)
{
  if (!s)
    return -1;

  char *d = NULL;

  for (d = s; *s; s++)
  {
    if (*s == '%')
    {
      if ((s[1] != '\0') && (s[2] != '\0') && isxdigit((unsigned char) s[1]) &&
          isxdigit((unsigned char) s[2]) && (hexval(s[1]) >= 0) && (hexval(s[2]) >= 0))
      {
        *d++ = (hexval(s[1]) << 4) | (hexval(s[2]));
        s += 2;
      }
      else
        return -1;
    }
    else
      *d++ = *s;
  }
  *d = '\0';
  return 0;
}

/**
 * url_check_scheme - Check the protocol of a URL
 * @param s String to check
 * @retval num Url type, e.g. #U_IMAPS
 */
enum UrlScheme url_check_scheme(const char *s)
{
  char sbuf[256];
  char *t = NULL;
  int i;

  if (!s || !(t = strchr(s, ':')))
    return U_UNKNOWN;
  if ((size_t)(t - s) >= (sizeof(sbuf) - 1))
    return U_UNKNOWN;

  mutt_str_strfcpy(sbuf, s, t - s + 1);
  mutt_str_strlower(sbuf);

  i = mutt_map_get_value(sbuf, UrlMap);
  if (i == -1)
    return U_UNKNOWN;
  else
    return (enum UrlScheme) i;
}

/**
 * url_parse - Fill in Url
 * @param src   String to parse
 * @retval ptr  Pointer to the parsed URL
 * @retval NULL String is invalid
 *
 * To free Url, caller must call url_free()
 */
struct Url *url_parse(const char *src)
{
  if (!src || !*src)
    return NULL;

  enum UrlScheme scheme = url_check_scheme(src);
  if (scheme == U_UNKNOWN)
    return NULL;

  char *p = NULL;
  size_t srcsize = strlen(src) + 1;
  struct Url *u = mutt_mem_calloc(1, sizeof(struct Url) + srcsize);

  u->scheme = scheme;
  u->user = NULL;
  u->pass = NULL;
  u->host = NULL;
  u->port = 0;
  u->path = NULL;
  STAILQ_INIT(&u->query_strings);
  u->src = (char *) u + sizeof(struct Url);
  mutt_str_strfcpy(u->src, src, srcsize);

  char *it = u->src;

  it = strchr(it, ':') + 1;

  if (strncmp(it, "//", 2) != 0)
  {
    u->path = it;
    if (url_pct_decode(u->path) < 0)
    {
      url_free(&u);
    }
    return u;
  }

  it += 2;

  /* We have the length of the string, so let's be fancier than strrchr */
  for (char *q = u->src + srcsize - 1; q >= it; --q)
  {
    if (*q == '?')
    {
      *q = '\0';
      if (parse_query_string(&u->query_strings, q + 1) < 0)
      {
        goto err;
      }
      break;
    }
  }

  u->path = strchr(it, '/');
  if (u->path)
  {
    *u->path++ = '\0';
    if (url_pct_decode(u->path) < 0)
      goto err;
  }

  char *at = strrchr(it, '@');
  if (at)
  {
    *at = '\0';
    p = strchr(it, ':');
    if (p)
    {
      *p = '\0';
      u->pass = p + 1;
      if (url_pct_decode(u->pass) < 0)
        goto err;
    }
    u->user = it;
    if (url_pct_decode(u->user) < 0)
      goto err;
    it = at + 1;
  }

  /* IPv6 literal address.  It may contain colons, so set p to start the port
   * scan after it.  */
  if ((*it == '[') && (p = strchr(it, ']')))
  {
    it++;
    *p++ = '\0';
  }
  else
    p = it;

  p = strchr(p, ':');
  if (p)
  {
    int num;
    *p++ = '\0';
    if ((mutt_str_atoi(p, &num) < 0) || (num < 0) || (num > 0xffff))
      goto err;
    u->port = (unsigned short) num;
  }
  else
    u->port = 0;

  if (mutt_str_strlen(it) != 0)
  {
    u->host = it;
    if (url_pct_decode(u->host) < 0)
      goto err;
  }
  else if (u->path)
  {
    /* No host are provided, we restore the / because this is absolute path */
    u->path = it;
    *it++ = '/';
  }

  return u;

err:
  url_free(&u);
  return NULL;
}

/**
 * url_free - Free the contents of a URL
 * @param[out] u Url to empty
 *
 * @note The Url itself is not freed
 */
void url_free(struct Url **u)
{
  if (!u || !*u)
    return;

  struct UrlQuery *np = STAILQ_FIRST(&(*u)->query_strings);
  struct UrlQuery *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    /* NOTE(sileht): We don't free members, they will be freed when
     * the src char* passed to url_parse() is freed */
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(&(*u)->query_strings);
  FREE(u);
}

/**
 * url_pct_encode - Percent-encode a string
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param src String to encode
 *
 * e.g. turn "hello world" into "hello%20world"
 */
void url_pct_encode(char *buf, size_t buflen, const char *src)
{
  static const char *hex = "0123456789ABCDEF";

  if (!buf)
    return;

  *buf = '\0';
  buflen--;
  while (src && *src && (buflen != 0))
  {
    if (strchr("/:&%", *src))
    {
      if (buflen < 3)
        break;

      *buf++ = '%';
      *buf++ = hex[(*src >> 4) & 0xf];
      *buf++ = hex[*src & 0xf];
      src++;
      buflen -= 3;
      continue;
    }
    *buf++ = *src++;
    buflen--;
  }
  *buf = '\0';
}

/**
 * url_tobuffer - Output the URL string for a given Url object
 * @param u      Url to turn into a string
 * @param buf    Buffer for the result
 * @param flags  Flags, e.g. #U_PATH
 * @retval  0 Success
 * @retval -1 Error
 */
int url_tobuffer(struct Url *u, struct Buffer *buf, int flags)
{
  if (!u || !buf)
    return -1;
  if (u->scheme == U_UNKNOWN)
    return -1;

  mutt_buffer_printf(buf, "%s:", mutt_map_get_name(u->scheme, UrlMap));

  if (u->host)
  {
    if (!(flags & U_PATH))
      mutt_buffer_addstr(buf, "//");

    if (u->user && (u->user[0] || !(flags & U_PATH)))
    {
      char str[256];
      url_pct_encode(str, sizeof(str), u->user);
      mutt_buffer_add_printf(buf, "%s@", str);
    }

    if (strchr(u->host, ':'))
      mutt_buffer_add_printf(buf, "[%s]", u->host);
    else
      mutt_buffer_add_printf(buf, "%s", u->host);

    if (u->port)
      mutt_buffer_add_printf(buf, ":%hu/", u->port);
    else
      mutt_buffer_addstr(buf, "/");
  }

  if (u->path)
    mutt_buffer_addstr(buf, u->path);

  return 0;
}

/**
 * url_tostring - Output the URL string for a given Url object
 * @param u      Url to turn into a string
 * @param dest   Buffer for the result
 * @param len    Length of buffer
 * @param flags  Flags, e.g. #U_PATH
 * @retval  0 Success
 * @retval -1 Error
 */
int url_tostring(struct Url *u, char *dest, size_t len, int flags)
{
  if (!u || !dest)
    return -1;

  struct Buffer *dest_buf = mutt_buffer_pool_get();

  int retval = url_tobuffer(u, dest_buf, flags);
  if (retval == 0)
    mutt_str_strfcpy(dest, mutt_b2s(dest_buf), len);

  mutt_buffer_pool_release(&dest_buf);

  return retval;
}
