/*
 * Copyright (C) 1998 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#include "mutt.h"
#include "globals.h"
#include "mutt_socket.h"
#include "mutt_tunnel.h"
#ifdef USE_SSL
# include "mutt_ssl.h"
#endif

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

/* support for multiple socket connections */
static CONNECTION *Connections = NULL;

/* forward declarations */
static int socket_preconnect (void);
static int socket_connect (int fd, struct sockaddr* sa);
static CONNECTION* socket_new_conn ();

/* Wrappers */
int mutt_socket_open (CONNECTION* conn) 
{
  if (socket_preconnect ())
    return -1;

  return conn->open (conn);
}

int mutt_socket_close (CONNECTION* conn)
{
  int rc = -1;

  if (conn->fd < 0)
    dprint (1, (debugfile, "mutt_socket_close: Attempt to close closed connection.\n"));
  else
    rc = conn->close (conn);

  conn->fd = -1;

  return rc;
}

int mutt_socket_read (CONNECTION* conn, char* buf, size_t len)
{
  int rc;

  if (conn->fd < 0)
  {
    dprint (1, (debugfile, "mutt_socket_read: attempt to read from closed connection\n"));
    return -1;
  }

  rc = conn->read (conn, buf, len);
  /* EOF */
  if (rc == 0)
  {
    mutt_error (_("Connection to %s closed"), conn->account.host);
    mutt_sleep (2);
  }
  if (rc <= 0)
    mutt_socket_close (conn);

  return rc;
}

int mutt_socket_write_d (CONNECTION *conn, const char *buf, int dbg)
{
  int rc;
  int len;

  dprint (dbg, (debugfile,"> %s", buf));

  if (conn->fd < 0)
  {
    dprint (1, (debugfile, "mutt_socket_write: attempt to write to closed connection\n"));
    return -1;
  }

  len = mutt_strlen (buf);
  if ((rc = conn->write (conn, buf, len)) < 0)
  {
    dprint (1, (debugfile,
      "mutt_socket_write: error writing, closing socket\n"));
    mutt_socket_close (conn);

    return -1;
  }

  if (rc < len)
  {
    dprint (1, (debugfile,
      "mutt_socket_write: ERROR: wrote %d of %d bytes!\n", rc, len));
  }

  return rc;
}

/* simple read buffering to speed things up. */
int mutt_socket_readchar (CONNECTION *conn, char *c)
{
  if (conn->bufpos >= conn->available)
  {
    if (conn->fd >= 0)
      conn->available = conn->read (conn, conn->inbuf, sizeof (conn->inbuf));
    else
    {
      dprint (1, (debugfile, "mutt_socket_readchar: attempt to read from closed connection.\n"));
      return -1;
    }
    conn->bufpos = 0;
    if (conn->available == 0)
    {
      mutt_error (_("Connection to %s closed"), conn->account.host);
      mutt_sleep (2);
    }
    if (conn->available <= 0)
    {
      mutt_socket_close (conn);
      return -1;
    }
  }
  *c = conn->inbuf[conn->bufpos];
  conn->bufpos++;
  return 1;
}

int mutt_socket_readln_d (char* buf, size_t buflen, CONNECTION* conn, int dbg)
{
  char ch;
  int i;
  int rc;

  for (i = 0; i < buflen-1; i++)
  {
    if (mutt_socket_readchar (conn, &ch) != 1)
    {
      buf[i] = '\0';
      return -1;
    }

    if (ch == '\n')
      break;
    buf[i] = ch;
  }

  /* strip \r from \r\n termination */
  if (i && buf[i-1] == '\r')
    buf[--i] = '\0';
  else
    buf[i] = '\0';

  dprint (dbg, (debugfile, "< %s\n", buf));
  
  /* number of bytes read, not strlen */
  return i + 1;
}

CONNECTION* mutt_socket_head (void)
{
  return Connections;
}

/* mutt_socket_free: remove connection from connection list and free it */
void mutt_socket_free (CONNECTION* conn)
{
  CONNECTION* iter;
  CONNECTION* tmp;

  iter = Connections;

  /* head is special case, doesn't need prev updated */
  if (iter == conn)
  {
    Connections = iter->next;
    FREE (&iter);
    return;
  }

  while (iter->next)
  {
    if (iter->next == conn)
    {
      tmp = iter->next;
      iter->next = tmp->next;
      FREE (&tmp);
      return;
    }
    iter = iter->next;
  }
}

/* mutt_conn_find: find a connection off the list of connections whose
 *   account matches account. If start is not null, only search for
 *   connections after the given connection (allows higher level socket code
 *   to make more fine-grained searches than account info - eg in IMAP we may
 *   wish to find a connection which is not in IMAP_SELECTED state) */
CONNECTION* mutt_conn_find (const CONNECTION* start, const ACCOUNT* account)
{
  CONNECTION* conn;

  conn = start ? start->next : Connections;
  while (conn)
  {
    if (mutt_account_match (account, &(conn->account)))
      return conn;
    conn = conn->next;
  }

  conn = socket_new_conn ();
  memcpy (&conn->account, account, sizeof (ACCOUNT));

  conn->next = Connections;
  Connections = conn;

  if (Tunnel && *Tunnel)
    mutt_tunnel_socket_setup (conn);
  else if (account->flags & M_ACCT_SSL) 
  {
#ifdef USE_SSL
    ssl_socket_setup (conn);
#elif USE_NSS
    mutt_nss_socket_setup (conn);
#else
    mutt_error _("SSL is unavailable.");
    mutt_sleep (2);
    FREE (&conn);

    return NULL;
#endif
  }
  else
  {
    conn->read = raw_socket_read;
    conn->write = raw_socket_write;
    conn->open = raw_socket_open;
    conn->close = raw_socket_close;
  }

  return conn;
}

static int socket_preconnect (void)
{
  int rc;
  int save_errno;

  if (mutt_strlen (Preconnect))
  {
    dprint (2, (debugfile, "Executing preconnect: %s\n", Preconnect));
    rc = mutt_system (Preconnect);
    dprint (2, (debugfile, "Preconnect result: %d\n", rc));
    if (rc)
    {
      save_errno = errno;
      mutt_perror (_("Preconnect command failed."));
      mutt_sleep (1);
      
      return save_errno;
    }
  }

  return 0;
}

/* socket_connect: set up to connect to a socket fd. */
static int socket_connect (int fd, struct sockaddr* sa)
{
  int sa_size;
  int save_errno;

  if (sa->sa_family == AF_INET)
    sa_size = sizeof (struct sockaddr_in);
#ifdef HAVE_GETADDRINFO
  else if (sa->sa_family == AF_INET6)
    sa_size = sizeof (struct sockaddr_in6);
#endif
  else
  {
    dprint (1, (debugfile, "Unknown address family!\n"));
    return -1;
  }
  
  if (ConnectTimeout > 0)
      alarm (ConnectTimeout);

  mutt_allow_interrupt (1);

  save_errno = 0;

  if (connect (fd, sa, sa_size) < 0)
  {
      save_errno = errno;
      dprint (2, (debugfile, "Connection failed. errno: %d...\n", errno));
      SigInt = 0;	/* reset in case we caught SIGINTR while in connect() */
  }

  if (ConnectTimeout > 0)
      alarm (0);
  mutt_allow_interrupt (0);

  return save_errno;
}

/* socket_new_conn: allocate and initialise a new connection. */
static CONNECTION* socket_new_conn ()
{
  CONNECTION* conn;

  conn = (CONNECTION *) safe_calloc (1, sizeof (CONNECTION));
  conn->fd = -1;

  return conn;
}

int raw_socket_close (CONNECTION *conn)
{
  int ret = close (conn->fd);
  if (ret == 0) conn->fd = -1;
  return ret;
}

int raw_socket_read (CONNECTION* conn, char* buf, size_t len)
{
  int rc;

  if ((rc = read (conn->fd, buf, len)) == -1)
  {
    mutt_error (_("Error talking to %s (%s)"), conn->account.host,
		strerror (errno));
    mutt_sleep (2);
  }

  return rc;
}

int raw_socket_write (CONNECTION* conn, const char* buf, size_t count)
{
  int rc;

  if ((rc = write (conn->fd, buf, count)) == -1)
  {
    mutt_error (_("Error talking to %s (%s)"), conn->account.host,
		strerror (errno));
    mutt_sleep (2);
  }

  return rc;
}

int raw_socket_open (CONNECTION* conn)
{
  int rc;
  int fd;

#ifdef HAVE_GETADDRINFO
/* --- IPv4/6 --- */

  /* "65536\0" */
  char port[6];
  struct addrinfo hints;
  struct addrinfo* res;
  struct addrinfo* cur;

  /* we accept v4 or v6 STREAM sockets */
  memset (&hints, 0, sizeof (hints));

  if (option (OPTUSEIPV6))
    hints.ai_family = AF_UNSPEC;
  else
    hints.ai_family = AF_INET;

  hints.ai_socktype = SOCK_STREAM;

  snprintf (port, sizeof (port), "%d", conn->account.port);
  
  mutt_message (_("Looking up %s..."), conn->account.host);

  rc = getaddrinfo (conn->account.host, port, &hints, &res);
  if (rc)
  {
    mutt_error (_("Could not find the host \"%s\""), conn->account.host);
    return -1;
  }

  mutt_message (_("Connecting to %s..."), conn->account.host); 

  rc = -1;
  for (cur = res; cur != NULL; cur = cur->ai_next)
  {
    fd = socket (cur->ai_family, cur->ai_socktype, cur->ai_protocol);
    fcntl(fd,F_SETFD,FD_CLOEXEC);
    if (fd >= 0)
    {
      if ((rc = socket_connect (fd, cur->ai_addr)) == 0)
      {
	conn->fd = fd;
	break;
      }
      else
	close (fd);
    }
  }

  freeaddrinfo (res);

#else
  /* --- IPv4 only --- */

  struct sockaddr_in sin;
  struct hostent* he;
  int i;

  memset (&sin, 0, sizeof (sin));
  sin.sin_port = htons (conn->account.port);
  sin.sin_family = AF_INET;

  mutt_message (_("Looking up %s..."), conn->account.host);

  if ((he = gethostbyname (conn->account.host)) == NULL)
  {
    mutt_error (_("Could not find the host \"%s\""), conn->account.host);
	
    return -1;
  }

  mutt_message (_("Connecting to %s..."), conn->account.host); 

  rc = -1;
  for (i = 0; he->h_addr_list[i] != NULL; i++)
  {
    memcpy (&sin.sin_addr, he->h_addr_list[i], he->h_length);
    fd = socket (PF_INET, SOCK_STREAM, IPPROTO_IP);

    if (fd >= 0)
    {
      if ((rc = socket_connect (fd, (struct sockaddr*) &sin)) == 0)
      {
	conn->fd = fd;
	break;
      }
      else
	close (fd);
    }
  }

#endif
  if (rc)
  {
    mutt_error (_("Could not connect to %s (%s)."), conn->account.host,
	    (rc > 0) ? strerror (rc) : _("unknown error"));
    mutt_sleep (2);
    return -1;
  }
  
  return 0;
}
