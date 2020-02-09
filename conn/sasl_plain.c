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

/**
 * @page conn_sasl_plain SASL plain authentication support
 *
 * SASL plain authentication support
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "sasl_plain.h"

/**
 * mutt_sasl_plain_msg - Construct a base64 encoded SASL PLAIN message
 * @param buf    Destination buffer
 * @param buflen Available space in the destination buffer
 * @param cmd    Protocol-specific string the prepend to the PLAIN message
 * @param authz  Authorization identity
 * @param user   Authentication identity (username)
 * @param pass   Password
 * @retval >0 Success, number of chars in the command string
 * @retval  0 Error
 *
 * This function can be used to build a protocol-specific SASL Response message
 * using the PLAIN mechanism. The protocol specific command is given in the cmd
 * parameter. The function appends a space, encodes the string derived from
 * authz\0user\0pass using base64 encoding, and stores the result in buf. If
 * cmd is either NULL or the empty string, the initial space is skipped.
 *
 * authz, user, and pass can each be up to 255 bytes, making up for a 765 bytes
 * string. Add the two NULL bytes in between plus one at the end and we get
 * 768.
 */
size_t mutt_sasl_plain_msg(char *buf, size_t buflen, const char *cmd,
                           const char *authz, const char *user, const char *pass)
{
  char tmp[768];
  size_t len = 0;
  size_t tmplen;

  if (!user || !*user || !pass || !*pass)
    return 0;

  tmplen = snprintf(tmp, sizeof(tmp), "%s%c%s%c%s", NONULL(authz), '\0', user, '\0', pass);

  if (cmd && *cmd)
  {
    len = snprintf(buf, buflen, "%s ", cmd);
  }
  len += mutt_b64_encode(tmp, tmplen, buf + len, buflen - len);
  return len;
}
