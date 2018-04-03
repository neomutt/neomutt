/**
 * @file
 * Parse and identify different URL schemes
 *
 * @authors
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

#ifndef _MUTT_URL_H
#define _MUTT_URL_H

#include <stddef.h>
#include "mutt/mutt.h"

struct Envelope;

/**
 * enum UrlScheme - All recognised Url types
 */
enum UrlScheme
{
  U_FILE,
  U_POP,
  U_POPS,
  U_IMAP,
  U_IMAPS,
  U_NNTP,
  U_NNTPS,
  U_SMTP,
  U_SMTPS,
  U_MAILTO,
#ifdef USE_NOTMUCH
  U_NOTMUCH,
#endif
  U_UNKNOWN
};

#define U_DECODE_PASSWD (1 << 0)
#define U_PATH          (1 << 1)

/**
 * struct UrlQueryString - Parsed Query String
 *
 * The arguments in a URL are saved in a linked list.
 *
 */

STAILQ_HEAD(UrlQueryStringHead, UrlQueryString);
struct UrlQueryString
{
  char *name;
  char *value;
  STAILQ_ENTRY(UrlQueryString) entries;
};

/**
 * struct Url - A parsed URL `proto://user:password@host:port/path?a=1&b=2`
 */
struct Url
{
  enum UrlScheme scheme;
  char *user;
  char *pass;
  char *host;
  unsigned short port;
  char *path;
  struct UrlQueryStringHead query_strings;
};

enum UrlScheme url_check_scheme(const char *s);
int url_parse(struct Url *u, char *src);
void url_free(struct Url *u);
int url_tostring(struct Url *u, char *dest, size_t len, int flags);
int url_parse_mailto(struct Envelope *e, char **body, const char *src);
int url_pct_decode(char *s);
void url_pct_encode(char *dst, size_t l, const char *src);

#endif /* _MUTT_URL_H */
