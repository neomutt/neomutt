/**
 * @file
 * SASL plain authentication support
 *
 * @authors
 * Copyright (C) 2016-2018 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2017-2022 Richard Russon <rich@flatcap.org>
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
 * @page conn_sasl_plain SASL plain authentication
 *
 * SASL plain authentication support
 */

#include "config.h"
#include "mutt/lib.h"
#include "sasl_plain.h"

/**
 * mutt_sasl_plain_msg - Construct a base64 encoded SASL PLAIN message
 * @param buf    Destination buffer
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
 */
size_t mutt_sasl_plain_msg(struct Buffer *buf, const char *cmd,
                           const char *authz, const char *user, const char *pass)
{
  if (!user || (*user == '\0') || !pass || (*pass == '\0'))
    return 0;

  if (cmd && (*cmd != '\0'))
    buf_printf(buf, "%s ", cmd);

  struct Buffer *user_pass = buf_pool_get();
  struct Buffer *encoded = buf_pool_get();

  buf_printf(user_pass, "%s%c%s%c%s", NONULL(authz), '\0', user, '\0', pass);
  mutt_b64_buffer_encode(encoded, buf_string(user_pass), buf_len(user_pass));
  buf_addstr(buf, buf_string(encoded));

  buf_pool_release(&user_pass);
  buf_pool_release(&encoded);

  return buf_len(buf);
}
