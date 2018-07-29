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
#include <stdio.h>
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
 * @param u Url to store the results
 * @param src String to parse
 * @retval  0 Success
 * @retval -1 Error
 */
static int parse_query_string(struct Url *u, char *src)
{
  struct UrlQueryString *qs = NULL;
  char *k = NULL, *v = NULL;

  while (src && *src)
  {
    qs = mutt_mem_calloc(1, sizeof(struct UrlQueryString));
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
    STAILQ_INSERT_TAIL(&u->query_strings, qs, entries);

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
  char *d = NULL;

  if (!s)
    return -1;

  for (d = s; *s; s++)
  {
    if (*s == '%')
    {
      if (s[1] && s[2] && isxdigit((unsigned char) s[1]) &&
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
  char sbuf[STRING];
  char *t = NULL;
  int i;

  if (!s || !(t = strchr(s, ':')))
    return U_UNKNOWN;
  if ((size_t)(t - s) >= (sizeof(sbuf) - 1))
    return U_UNKNOWN;

  mutt_str_strfcpy(sbuf, s, t - s + 1);
  for (t = sbuf; *t; t++)
    *t = tolower(*t);

  i = mutt_map_get_value(sbuf, UrlMap);
  if (i == -1)
    return U_UNKNOWN;
  else
    return (enum UrlScheme) i;
}

/**
 * url_parse - Fill in Url
 * @param u   Url where the result is stored
 * @param src String to parse
 * @retval 0  String is valid
 * @retval -1 String is invalid
 *
 * char* elements are pointers into src, which is modified by this call
 * (duplicate it first if you need to).
 *
 * This method doesn't allocated any additional char* of the Url and
 * UrlQueryString structs.
 *
 * To free Url, caller must free "src" and call url_free()
 */
int url_parse(struct Url *u, char *src)
{
  char *t = NULL, *p = NULL;

  u->user = NULL;
  u->pass = NULL;
  u->host = NULL;
  u->port = 0;
  u->path = NULL;
  STAILQ_INIT(&u->query_strings);

  u->scheme = url_check_scheme(src);
  if (u->scheme == U_UNKNOWN)
    return -1;

  src = strchr(src, ':') + 1;

  if (strncmp(src, "//", 2) != 0)
  {
    u->path = src;
    return url_pct_decode(u->path);
  }

  src += 2;

  /* Notmuch and mailto schemes can include a query */
#ifdef USE_NOTMUCH
  if ((u->scheme == U_NOTMUCH) || (u->scheme == U_MAILTO))
#else
  if (u->scheme == U_MAILTO)
#endif
  {
    t = strrchr(src, '?');
    if (t)
    {
      *t++ = '\0';
      if (parse_query_string(u, t) < 0)
        goto err;
    }
  }

  u->path = strchr(src, '/');
  if (u->path)
  {
    *u->path++ = '\0';
    if (url_pct_decode(u->path) < 0)
      goto err;
  }

  t = strrchr(src, '@');
  if (t)
  {
    *t = '\0';
    p = strchr(src, ':');
    if (p)
    {
      *p = '\0';
      u->pass = p + 1;
      if (url_pct_decode(u->pass) < 0)
        goto err;
    }
    u->user = src;
    if (url_pct_decode(u->user) < 0)
      goto err;
    src = t + 1;
  }

  /* IPv6 literal address.  It may contain colons, so set t to start
   * the port scan after it.
   */
  if ((*src == '[') && (t = strchr(src, ']')))
  {
    src++;
    *t++ = '\0';
  }
  else
    t = src;

  p = strchr(t, ':');
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

  if (mutt_str_strlen(src) != 0)
  {
    u->host = src;
    if (url_pct_decode(u->host) < 0)
      goto err;
  }
  else if (u->path)
  {
    /* No host are provided, we restore the / because this is absolute path */
    u->path = src;
    *src++ = '/';
  }

  return 0;

err:
  url_free(u);
  return -1;
}

/**
 * url_free - Free the contents of a URL
 * @param u Url to empty
 *
 * @note The Url itself is not freed
 */
void url_free(struct Url *u)
{
  struct UrlQueryString *np = STAILQ_FIRST(&u->query_strings), *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    /* NOTE(sileht): We don't free members, they will be freed when
     * the src char* passed to url_parse() is freed */
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(&u->query_strings);
}

/**
 * url_pct_encode - Percent-encode a string
 * @param dst Buffer for the result
 * @param l   Length of buffer
 * @param src String to encode
 *
 * e.g. turn "hello world" into "hello%20world"
 */
void url_pct_encode(char *dst, size_t l, const char *src)
{
  static const char *alph = "0123456789ABCDEF";

  *dst = 0;
  l--;
  while (src && *src && (l != 0))
  {
    if (strchr("/:&%", *src))
    {
      if (l < 3)
        break;

      *dst++ = '%';
      *dst++ = alph[(*src >> 4) & 0xf];
      *dst++ = alph[*src & 0xf];
      src++;
      l -= 3;
      continue;
    }
    *dst++ = *src++;
    l--;
  }
  *dst = 0;
}

/**
 * url_tostring - Output the URL string for a given Url object
 * @param u     Url to turn into a string
 * @param dest  Buffer for the result
 * @param len   Length of buffer
 * @param flags Flags, e.g. #U_DECODE_PASSWD
 * @retval  0 Success
 * @retval -1 Error
 */
int url_tostring(struct Url *u, char *dest, size_t len, int flags)
{
  if (u->scheme == U_UNKNOWN)
    return -1;

  snprintf(dest, len, "%s:", mutt_map_get_name(u->scheme, UrlMap));

  if (u->host)
  {
    if (!(flags & U_PATH))
      mutt_str_strcat(dest, len, "//");
    size_t l = strlen(dest);
    len -= l;
    dest += l;

    if (u->user && (u->user[0] || !(flags & U_PATH)))
    {
      char str[STRING];
      url_pct_encode(str, sizeof(str), u->user);

      if (flags & U_DECODE_PASSWD && u->pass)
      {
        char p[STRING];
        url_pct_encode(p, sizeof(p), u->pass);
        snprintf(dest, len, "%s:%s@", str, p);
      }
      else
        snprintf(dest, len, "%s@", str);

      l = strlen(dest);
      len -= l;
      dest += l;
    }

    if (strchr(u->host, ':'))
      snprintf(dest, len, "[%s]", u->host);
    else
      snprintf(dest, len, "%s", u->host);

    l = strlen(dest);
    len -= l;
    dest += l;

    if (u->port)
      snprintf(dest, len, ":%hu/", u->port);
    else
      snprintf(dest, len, "/");
  }

  if (u->path)
    mutt_str_strcat(dest, len, u->path);

  return 0;
}
