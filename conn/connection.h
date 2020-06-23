/**
 * @file
 * An open network connection (socket)
 *
 * @authors
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2005 Brendan Cully <brendan@kublai.com>
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

#ifndef MUTT_CONN_CONNECTION_H
#define MUTT_CONN_CONNECTION_H

#include <stdio.h>
#include <time.h>
#include "connaccount.h"

/**
 * struct Connection - An open network connection (socket)
 */
struct Connection
{
  struct ConnAccount account; ///< Account details: username, password, etc
  unsigned int ssf;           ///< Security strength factor, in bits (see below)
  char inbuf[1024];           ///< Buffer for incoming traffic
  int bufpos;                 ///< Current position in the buffer
  int fd;                     ///< Socket file descriptor
  int available;              ///< Amount of data waiting to be read
  void *sockdata;             ///< Backend-specific socket data

  /**
   * Note about ssf: in actuality, NeoMutt uses this as a boolean to determine
   * if the connection is "secure" using TLS or $tunnel if $tunnel_is_secure is
   * set.
   *
   * The value is passed to SASL, but since no min_ssf is also passed to SASL
   * I don't believe it makes any difference.
   *
   * The GnuTLS code currently even puts bytes in here, so I doubt the exact
   * value has significance for NeoMutt purposes.
   */

  /**
   * open - Open a socket Connection
   * @param conn Connection to a server
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*open)(struct Connection *conn);

  /**
   * read - Read from a socket Connection
   * @param conn  Connection to a server
   * @param buf   Buffer to store the data
   * @param count Number of bytes to read
   * @retval >0 Success, number of bytes read
   * @retval -1 Error, see errno
   */
  int (*read)(struct Connection *conn, char *buf, size_t count);

  /**
   * write - Write to a socket Connection
   * @param conn  Connection to a server
   * @param buf   Buffer to read into
   * @param count Number of bytes to read
   * @retval >0 Success, number of bytes written
   * @retval -1 Error, see errno
   */
  int (*write)(struct Connection *conn, const char *buf, size_t count);

  /**
   * poll - Check whether a socket read would block
   * @param conn Connection to a server
   * @param wait_secs How long to wait for a response
   * @retval >0 There is data to read
   * @retval  0 Read would block
   * @retval -1 Connection doesn't support polling
   */
  int (*poll)(struct Connection *conn, time_t wait_secs);

  /**
   * close - Close a socket Connection
   * @param conn Connection to a server
   * @retval  0 Success
   * @retval -1 Error, see errno
   */
  int (*close)(struct Connection *conn);
};

#endif /* MUTT_CONN_CONNECTION_H */
