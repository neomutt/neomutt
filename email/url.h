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

#ifndef MUTT_EMAIL_URL_H
#define MUTT_EMAIL_URL_H

#include <stddef.h>
#include "mutt/mutt.h"

/**
 * enum UrlScheme - All recognised Url types
 */
enum UrlScheme
{
  U_UNKNOWN, ///< Url wasn't recognised
  U_FILE,    ///< Url is file://
  U_POP,     ///< Url is pop://
  U_POPS,    ///< Url is pops://
  U_IMAP,    ///< Url is imap://
  U_IMAPS,   ///< Url is imaps://
  U_NNTP,    ///< Url is nntp://
  U_NNTPS,   ///< Url is nntps://
  U_SMTP,    ///< Url is smtp://
  U_SMTPS,   ///< Url is smtps://
  U_MAILTO,  ///< Url is mailto://
  U_NOTMUCH, ///< Url is notmuch://
  U_HELP,    ///< Url is help://
};

#define U_PATH          (1 << 1)

/**
 * struct UrlQueryString - Parsed Query String
 *
 * The arguments in a URL are saved in a linked list.
 */
struct UrlQueryString
{
  char *name;
  char *value;
  STAILQ_ENTRY(UrlQueryString) entries;
};
STAILQ_HEAD(UrlQueryStringHead, UrlQueryString);

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
  char *src;
};

enum UrlScheme url_check_scheme(const char *s);
void           url_free(struct Url **u);
struct Url    *url_parse(const char *src);
int            url_pct_decode(char *s);
void           url_pct_encode(char *buf, size_t buflen, const char *src);
int            url_tobuffer(struct Url *u, struct Buffer *dest, int flags);
int            url_tostring(struct Url *u, char *buf, size_t buflen, int flags);

#endif /* MUTT_EMAIL_URL_H */
