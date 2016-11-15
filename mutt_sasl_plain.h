/*
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 *
 *     This program is free software; you can redistribute it and/or modify it
 *     under the terms of the GNU General Public License as published by the
 *     Free Software Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 *
 *     This program is distributed in the hope that it will be useful, but
 *     WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *     General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License along
 *     with this program; if not, write to the Free Software Foundation, Inc.,
 *     51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MUTT_SASL_PLAIN_H_
#define _MUTT_SASL_PLAIN_H_

#include <stdlib.h> /* for size_t */

/**
 * mutt_sasl_plain_msg - construct a base64 encoded SASL PLAIN message
 * @param buf    Destination buffer.
 * @param buflen Available space in the destination buffer.
 * @param cmd    Protocol-specific string the prepend to the PLAIN message.
 * @param authz  Authorization identity.
 * @param user   Authentication identity (username).
 * @param pass   Password.
 *
 * @return The number of bytes written to buf.
 *
 * This function can be used to build a protocol-specific SASL Response message
 * using the PLAIN mechanism. The protocol specific command is given in the cmd
 * parameter. The function appends a space, encodes the string derived from
 * authz\0user\0pass using base64 encoding, and stores the result in buf.
 *
 * Example usages for IMAP and SMTP, respectively:
 *
 * mutt_sasl_plain_msg(buf, sizeof(buf), "AUTHENTICATE PLAIN", user, user, pass);
 * mutt_sasl_plain_msg(buf, sizeof(buf), "AUTH PLAIN", NULL, user, pass);
 */
size_t mutt_sasl_plain_msg(char *buf, size_t buflen, const char *cmd,
                           const char *authz, const char *user,
                           const char *pass);

#endif /* _MUTT_SASL_PLAIN_H_ */
