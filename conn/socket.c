/**
 * @file
 * Low-level socket handling
 *
 * @authors
 * Copyright (C) 1998,2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2006,2008 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>
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
 * @page conn_socket Low-level socket handling
 *
 * Low-level socket handling
 */

#include "config.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include "private.h"
#include "mutt/lib.h"
#include "socket.h"
#include "connaccount.h"
#include "connection.h"
#include "globals.h"
#include "mutt_globals.h"
#include "protos.h"
#include "ssl.h"

/**
 * socket_preconnect - Execute a command before opening a socket
 * @retval 0  Success
 * @retval >0 An errno, e.g. EPERM
 */
static int socket_preconnect(void)
{
  if (!C_Preconnect)
    return 0;

  mutt_debug(LL_DEBUG2, "Executing preconnect: %s\n", C_Preconnect);
  const int rc = mutt_system(C_Preconnect);
  mutt_debug(LL_DEBUG2, "Preconnect result: %d\n", rc);
  if (rc != 0)
  {
    const int save_errno = errno;
    mutt_perror(_("Preconnect command failed"));

    return save_errno;
  }

  return 0;
}

/**
 * mutt_socket_open - Simple wrapper
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_socket_open(struct Connection *conn)
{
  int rc;

  if (socket_preconnect())
    return -1;

  rc = conn->open(conn);

  mutt_debug(LL_DEBUG2, "Connected to %s:%d on fd=%d\n", conn->account.host,
             conn->account.port, conn->fd);

  return rc;
}

/**
 * mutt_socket_close - Close a socket
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_socket_close(struct Connection *conn)
{
  if (!conn)
    return 0;

  int rc = -1;

  if (conn->fd < 0)
    mutt_debug(LL_DEBUG1, "Attempt to close closed connection\n");
  else
    rc = conn->close(conn);

  conn->fd = -1;
  conn->ssf = 0;
  conn->bufpos = 0;
  conn->available = 0;

  return rc;
}

/**
 * mutt_socket_read - read from a Connection
 * @param conn Connection a server
 * @param buf Buffer to store read data
 * @param len length of the buffer
 * @retval >0 Success, number of bytes read
 * @retval -1 Error, see errno
 */
int mutt_socket_read(struct Connection *conn, char *buf, size_t len)
{
  return conn->read(conn, buf, len);
}

/**
 * mutt_socket_write - write to a Connection
 * @param conn Connection to a server
 * @param buf Buffer with data to write
 * @param len Length of data to write
 * @retval >0 Number of bytes written
 * @retval -1 Error
 */
int mutt_socket_write(struct Connection *conn, const char *buf, size_t len)
{
  return conn->write(conn, buf, len);
}

/**
 * mutt_socket_write_d - Write data to a socket
 * @param conn Connection to a server
 * @param buf Buffer with data to write
 * @param len Length of data to write
 * @param dbg Debug level for logging
 * @retval >0 Number of bytes written
 * @retval -1 Error
 */
int mutt_socket_write_d(struct Connection *conn, const char *buf, int len, int dbg)
{
  int sent = 0;

  mutt_debug(dbg, "%d> %s", conn->fd, buf);

  if (conn->fd < 0)
  {
    mutt_debug(LL_DEBUG1, "attempt to write to closed connection\n");
    return -1;
  }

  while (sent < len)
  {
    const int rc = conn->write(conn, buf + sent, len - sent);
    if (rc < 0)
    {
      mutt_debug(LL_DEBUG1, "error writing (%s), closing socket\n", strerror(errno));
      mutt_socket_close(conn);

      return -1;
    }

    if (rc < len - sent)
      mutt_debug(LL_DEBUG3, "short write (%d of %d bytes)\n", rc, len - sent);

    sent += rc;
  }

  return sent;
}

/**
 * mutt_socket_poll - Checks whether reads would block
 * @param conn Connection to a server
 * @param wait_secs How long to wait for a response
 * @retval >0 There is data to read
 * @retval  0 Read would block
 * @retval -1 Connection doesn't support polling
 */
int mutt_socket_poll(struct Connection *conn, time_t wait_secs)
{
  if (conn->bufpos < conn->available)
    return conn->available - conn->bufpos;

  if (conn->poll)
    return conn->poll(conn, wait_secs);

  return -1;
}

/**
 * mutt_socket_readchar - simple read buffering to speed things up
 * @param[in]  conn Connection to a server
 * @param[out] c    Character that was read
 * @retval  1 Success
 * @retval -1 Error
 */
int mutt_socket_readchar(struct Connection *conn, char *c)
{
  if (conn->bufpos >= conn->available)
  {
    if (conn->fd >= 0)
      conn->available = conn->read(conn, conn->inbuf, sizeof(conn->inbuf));
    else
    {
      mutt_debug(LL_DEBUG1, "attempt to read from closed connection\n");
      return -1;
    }
    conn->bufpos = 0;
    if (conn->available == 0)
    {
      mutt_error(_("Connection to %s closed"), conn->account.host);
    }
    if (conn->available <= 0)
    {
      mutt_socket_close(conn);
      return -1;
    }
  }
  *c = conn->inbuf[conn->bufpos];
  conn->bufpos++;
  return 1;
}

/**
 * mutt_socket_readln_d - Read a line from a socket
 * @param buf    Buffer to store the line
 * @param buflen Length of data to write
 * @param conn   Connection to a server
 * @param dbg    Debug level for logging
 * @retval >0 Success, number of bytes read
 * @retval -1 Error
 */
int mutt_socket_readln_d(char *buf, size_t buflen, struct Connection *conn, int dbg)
{
  char ch;
  int i;

  for (i = 0; i < buflen - 1; i++)
  {
    if (mutt_socket_readchar(conn, &ch) != 1)
    {
      buf[i] = '\0';
      return -1;
    }

    if (ch == '\n')
      break;
    buf[i] = ch;
  }

  /* strip \r from \r\n termination */
  if (i && (buf[i - 1] == '\r'))
    i--;
  buf[i] = '\0';

  mutt_debug(dbg, "%d< %s\n", conn->fd, buf);

  /* number of bytes read, not strlen */
  return i + 1;
}

/**
 * mutt_socket_new - allocate and initialise a new connection
 * @param type Type of the new Connection
 * @retval ptr New Connection
 */
struct Connection *mutt_socket_new(enum ConnectionType type)
{
  struct Connection *conn = mutt_mem_calloc(1, sizeof(struct Connection));
  conn->fd = -1;

  if (type == MUTT_CONNECTION_TUNNEL)
  {
    mutt_tunnel_socket_setup(conn);
  }
  else if (type == MUTT_CONNECTION_SSL)
  {
    int rc = mutt_ssl_socket_setup(conn);
    if (rc < 0)
      FREE(&conn);
  }
  else
  {
    conn->read = raw_socket_read;
    conn->write = raw_socket_write;
    conn->open = raw_socket_open;
    conn->close = raw_socket_close;
    conn->poll = raw_socket_poll;
  }

  return conn;
}

/**
 * mutt_socket_empty - Clear out any queued data
 *
 * The internal buffer is emptied and any data that has already arrived at this
 * machine (in kernel buffers) is read and dropped.
 */
void mutt_socket_empty(struct Connection *conn)
{
  if (!conn)
    return;

  char buf[1024];
  int bytes;

  while ((bytes = mutt_socket_poll(conn, 0)) > 0)
  {
    mutt_socket_read(conn, buf, MIN(bytes, sizeof(buf)));
  }
}
