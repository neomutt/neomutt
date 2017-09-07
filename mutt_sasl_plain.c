/**
 * @file
 * SASL plain authentication support
 *
 * @authors
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
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

#include "config.h"
#include <stdio.h>
#include "lib/lib.h"
#include "mutt_sasl_plain.h"
#include "protos.h"

size_t mutt_sasl_plain_msg(char *buf, size_t buflen, const char *cmd,
                           const char *authz, const char *user, const char *pass)
{
  /* authz, user, and pass can each be up to 255 bytes, making up for a 765
   * bytes string. Add the two NULL bytes in between plus one at the end and we
   * get 768. */
  char tmp[768];
  size_t len;
  size_t tmplen;

  if (!user || !*user || !pass || !*pass)
    return 0;

  tmplen = snprintf(tmp, sizeof(tmp), "%s%c%s%c%s", NONULL(authz), '\0', user, '\0', pass);

  len = snprintf(buf, buflen, "%s ", cmd);
  len += mutt_to_base64(buf + len, tmp, tmplen, buflen - len);
  return len;
}
