/**
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

#ifndef _URL_H
# define _URL_H

#include "mutt.h"

typedef enum url_scheme
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
}
url_scheme_t;

#define U_DECODE_PASSWD (1)
#define U_PATH          (1 << 1)

typedef struct ciss_url
{
  url_scheme_t scheme;
  char *user;
  char *pass;
  char *host;
  unsigned short port;
  char *path;
}
ciss_url_t;

url_scheme_t url_check_scheme(const char *s);
int url_parse_ciss(ciss_url_t *ciss, char *src);
int url_ciss_tostring(ciss_url_t *ciss, char *dest, size_t len, int flags);
int url_parse_mailto(ENVELOPE *e, char **body, const char *src);
int url_pct_decode(char *s);

#endif
