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
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "mutt/lib.h"
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
  int rc = 0;

  if (!src || !*src)
    return rc;

  regmatch_t match[3];
  regmatch_t *keymatch = &match[1];
  regmatch_t *valmatch = &match[2];

  struct Regex *re = mutt_regex_compile("([^&]+)=([^&]+)", REG_EXTENDED);
  assert(re && "Something is wrong with your RE engine.");

  bool again = true;
  while (again)
  {
    if (!mutt_regex_capture(re, src, mutt_array_size(match), match))
    {
      goto done;
    }
    again = src[valmatch->rm_eo] != '\0';

    char *key = src + keymatch->rm_so;
    char *val = src + valmatch->rm_so;
    src[keymatch->rm_eo] = '\0';
    src[valmatch->rm_eo] = '\0';
    if ((url_pct_decode(key) < 0) || (url_pct_decode(val) < 0))
    {
      rc = -1;
      goto done;
    }

    struct UrlQuery *qs = mutt_mem_calloc(1, sizeof(struct UrlQuery));
    qs->name = key;
    qs->value = val;
    STAILQ_INSERT_TAIL(list, qs, entries);

    src += valmatch->rm_eo + again;
  }

done:
  mutt_regex_free(&re);
  return rc;
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
 * parse - Parse a URL
 * @param src String to parse
 * @param scheme If not NULL, save the parsed scheme
 *
 * @note If scheme is not NULL, the function stops after identifying the scheme
 * and returns NULL.
 */
static struct Url *parse(const char *src, enum UrlScheme *scheme)
{
  if (!src || !*src)
    return NULL;

  if (scheme)
    *scheme = U_UNKNOWN;

  regmatch_t *match = mutt_prex_capture(PREX_URL, src);
  if (!match)
    return NULL;

  enum UrlScheme us =
      mutt_map_get_value_n(src, mutt_regmatch_len(&match[PREX_URL_MATCH_SCHEME]), UrlMap);
  if (us == -1)
  {
    us = U_UNKNOWN;
  }

  if (scheme)
  {
    *scheme = us;
    return NULL;
  }

  if (us == U_UNKNOWN)
    return NULL;

  const regmatch_t *userinfo = &match[PREX_URL_MATCH_USERINFO];
  const regmatch_t *user = &match[PREX_URL_MATCH_USER];
  const regmatch_t *pass = &match[PREX_URL_MATCH_PASS];
  const regmatch_t *host = &match[PREX_URL_MATCH_HOSTNAME];
  const regmatch_t *ipvx = &match[PREX_URL_MATCH_HOSTIPVX];
  const regmatch_t *port = &match[PREX_URL_MATCH_PORT];
  const regmatch_t *path = &match[PREX_URL_MATCH_PATH];
  const regmatch_t *query = &match[PREX_URL_MATCH_QUERY];
  const regmatch_t *pathonly = &match[PREX_URL_MATCH_PATH_ONLY];

  struct Url *url = url_new();
  url->scheme = us;
  url->src = mutt_str_strdup(src);

  /* If the scheme is not followed by two forward slashes, then it's a simple
   * path (see https://tools.ietf.org/html/rfc3986#section-3). */
  if (mutt_regmatch_start(pathonly) != -1)
  {
    url->path = url->src + mutt_regmatch_start(pathonly);
    if (url_pct_decode(url->path) < 0)
      goto err;
    return url;
  }

  /* separate userinfo part */
  if (mutt_regmatch_end(userinfo) != -1)
  {
    url->src[mutt_regmatch_end(userinfo) - 1] = '\0';
  }

  /* user */
  if (mutt_regmatch_end(user) != -1)
  {
    url->src[mutt_regmatch_end(user)] = '\0';
    url->user = url->src + mutt_regmatch_start(user);
    if (url_pct_decode(url->user) < 0)
      goto err;
  }

  /* pass */
  if (mutt_regmatch_end(pass) != -1)
  {
    url->pass = url->src + mutt_regmatch_start(pass);
    if (url_pct_decode(url->pass) < 0)
      goto err;
  }

  /* host */
  if (mutt_regmatch_end(host) != -1)
  {
    url->host = url->src + mutt_regmatch_start(host);
    url->src[mutt_regmatch_end(host)] = '\0';
  }
  else if (mutt_regmatch_end(ipvx) != -1)
  {
    url->host = url->src + mutt_regmatch_start(ipvx) + 1; /* skip opening '[' */
    url->src[mutt_regmatch_end(ipvx) - 1] = '\0';         /* skip closing ']' */
  }

  /* port */
  if (mutt_regmatch_end(port) != -1)
  {
    url->src[mutt_regmatch_end(port)] = '\0';
    const char *ports = url->src + mutt_regmatch_start(port);
    int num;
    if ((mutt_str_atoi(ports, &num) < 0) || (num < 0) || (num > 0xffff))
    {
      goto err;
    }
    url->port = (unsigned short) num;
  }

  /* path */
  if (mutt_regmatch_end(path) != -1)
  {
    url->src[mutt_regmatch_end(path)] = '\0';
    url->path = url->src + mutt_regmatch_start(path);
    if (!url->host)
    {
      /* If host is not provided, restore the '/': this is an absolute path */
      *(--url->path) = '/';
    }
    if (url_pct_decode(url->path) < 0)
      goto err;
  }

  /* query */
  if (mutt_regmatch_end(query) != -1)
  {
    char *squery = url->src + mutt_regmatch_start(query);
    if (parse_query_string(&url->query_strings, squery) < 0)
      goto err;
  }

  return url;

err:
  url_free(&url);
  return NULL;
}

/**
 * url_check_scheme - Check the protocol of a URL
 * @param s String to check
 * @retval num Url type, e.g. #U_IMAPS
 */
enum UrlScheme url_check_scheme(const char *s)
{
  enum UrlScheme ret = U_UNKNOWN;
  parse(s, &ret);
  return ret;
}

/**
 * url_free - Free the contents of a URL
 * @param ptr Url to free
 */
void url_free(struct Url **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Url *url = *ptr;

  struct UrlQuery *np = NULL;
  struct UrlQuery *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &url->query_strings, entries, tmp)
  {
    STAILQ_REMOVE(&url->query_strings, np, UrlQuery, entries);
    // Don't free 'name', 'value': they are pointers into the 'src' string
    FREE(&np);
  }

  FREE(&url->src);
  FREE(ptr);
}

/**
 * url_new - Create a Url
 * @retval ptr New Url
 */
struct Url *url_new(void)
{
  struct Url *url = mutt_mem_calloc(1, sizeof(struct Url));

  url->scheme = U_UNKNOWN;
  STAILQ_INIT(&url->query_strings);

  return url;
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
  return parse(src, NULL);
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
    if (strchr("/:&%=", *src))
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
 * @param url    Url to turn into a string
 * @param buf    Buffer for the result
 * @param flags  Flags, e.g. #U_PATH
 * @retval  0 Success
 * @retval -1 Error
 */
int url_tobuffer(struct Url *url, struct Buffer *buf, int flags)
{
  if (!url || !buf)
    return -1;
  if (url->scheme == U_UNKNOWN)
    return -1;

  mutt_buffer_printf(buf, "%s:", mutt_map_get_name(url->scheme, UrlMap));

  if (url->host)
  {
    if (!(flags & U_PATH))
      mutt_buffer_addstr(buf, "//");

    if (url->user && (url->user[0] || !(flags & U_PATH)))
    {
      char str[256];
      url_pct_encode(str, sizeof(str), url->user);
      mutt_buffer_add_printf(buf, "%s@", str);
    }

    if (strchr(url->host, ':'))
      mutt_buffer_add_printf(buf, "[%s]", url->host);
    else
      mutt_buffer_add_printf(buf, "%s", url->host);

    if (url->port)
      mutt_buffer_add_printf(buf, ":%hu/", url->port);
    else
      mutt_buffer_addstr(buf, "/");
  }

  if (url->path)
    mutt_buffer_addstr(buf, url->path);

  if (STAILQ_FIRST(&url->query_strings))
  {
    mutt_buffer_addstr(buf, "?");

    char str[256];
    struct UrlQuery *np = NULL;
    STAILQ_FOREACH(np, &url->query_strings, entries)
    {
      url_pct_encode(str, sizeof(str), np->name);
      mutt_buffer_addstr(buf, str);
      mutt_buffer_addstr(buf, "=");
      url_pct_encode(str, sizeof(str), np->value);
      mutt_buffer_addstr(buf, str);
      if (STAILQ_NEXT(np, entries))
        mutt_buffer_addstr(buf, "&");
    }
  }

  return 0;
}

/**
 * url_tostring - Output the URL string for a given Url object
 * @param url    Url to turn into a string
 * @param dest   Buffer for the result
 * @param len    Length of buffer
 * @param flags  Flags, e.g. #U_PATH
 * @retval  0 Success
 * @retval -1 Error
 */
int url_tostring(struct Url *url, char *dest, size_t len, int flags)
{
  if (!url || !dest)
    return -1;

  struct Buffer *dest_buf = mutt_buffer_pool_get();

  int retval = url_tobuffer(url, dest_buf, flags);
  if (retval == 0)
    mutt_str_strfcpy(dest, mutt_b2s(dest_buf), len);

  mutt_buffer_pool_release(&dest_buf);

  return retval;
}
