/*
 * Copyright (C) 1998 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
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
#ifdef USE_SSL
# include "imap_ssl.h"
#endif

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

/* support for multiple socket connections */
static CONNECTION *Connections = NULL;

/* forward declarations */
static int socket_connect (CONNECTION* conn, struct sockaddr_in sin,
  int verbose);
static CONNECTION* socket_new_conn ();

/* Wrappers */
int mutt_socket_open (CONNECTION* conn) 
{
  return conn->open (conn);
}

int mutt_socket_close (CONNECTION* conn)
{
  int rc;

  if (conn->fd < 0)
    dprint (1, (debugfile, "mutt_socket_close: Attempt to close closed connection.\n"));
  else
    rc = conn->close (conn);

  conn->fd = -1;

  return rc;
}

int mutt_socket_write_d (CONNECTION *conn, const char *buf, int dbg)
{
  int rc;

  dprint (dbg, (debugfile,"> %s", buf));

  if (conn->fd < 0)
  {
    dprint (1, (debugfile, "mutt_socket_write: attempt to write to closed connection\n"));
    return -1;
  }

  if ((rc = conn->write (conn, buf, mutt_strlen (buf))) < 0)
  {
    dprint (1, (debugfile, "mutt_socket_write: error writing, closing socket\n"));
    mutt_socket_close (conn);

    return -1;
  }

  return rc;
}

/* simple read buffering to speed things up. */
int mutt_socket_readchar (CONNECTION *conn, char *c)
{
  if (conn->bufpos >= conn->available)
  {
    if (conn->fd >= 0)
      conn->available = conn->read (conn);
    else
    {
      dprint (1, (debugfile, "mutt_socket_readchar: attempt to read closed from connection.\n"));
      return -1;
    }
    conn->bufpos = 0;
    if (conn->available <= 0)
      return conn->available; /* returns 0 for EOF or -1 for other error */
  }
  *c = conn->inbuf[conn->bufpos];
  conn->bufpos++;
  return 1;
}

int mutt_socket_readln_d (char* buf, size_t buflen, CONNECTION* conn, int dbg)
{
  char ch;
  int i;

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

#ifdef USE_SSL
  if (account->flags & M_ACCT_SSL) 
    ssl_socket_setup (conn);
  else
#endif
  {
    conn->read = raw_socket_read;
    conn->write = raw_socket_write;
    conn->open = raw_socket_open;
    conn->close = raw_socket_close;
  }

  return conn;
}

/* socket_connect: attempt to bind a socket and connect to it */
static int socket_connect (CONNECTION* conn, struct sockaddr_in sin,
  int verbose)
{
  if ((conn->fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
  {
    if (verbose) 
      mutt_perror ("socket");

    return -1;
  }

  if (connect (conn->fd, (struct sockaddr *) &sin, sizeof (sin)) < 0)
  {
    raw_socket_close (conn);
    if (verbose) 
    {
      mutt_perror ("connect");
      sleep (1);
    }

    return -1;
  }

  return 0;
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

int raw_socket_read (CONNECTION *conn)
{
  return read (conn->fd, conn->inbuf, LONG_STRING);
}

int raw_socket_write (CONNECTION* conn, const char* buf, size_t count)
{
  return write (conn->fd, buf, count);
}

int raw_socket_open (CONNECTION* conn)
{
  struct sockaddr_in sin;
  struct hostent *he;
  int    verbose;
  int  do_preconnect = mutt_strlen (Preconnect) > 0;
  /* This might be a config variable */
  int first_try_without_preconnect = TRUE; 

  mutt_message (_("Looking up %s..."), conn->account.host);
  
  memset (&sin, 0, sizeof (sin));
  sin.sin_port = htons (conn->account.port);
  sin.sin_family = AF_INET;
  if ((he = gethostbyname (conn->account.host)) == NULL)
  {
    mutt_error (_("Could not find the host \"%s\""), conn->account.host);
	
    return -1;
  }
  memcpy (&sin.sin_addr, he->h_addr_list[0], he->h_length);

  mutt_message (_("Connecting to %s..."), conn->account.host); 

  if (do_preconnect && first_try_without_preconnect)
  {
    verbose = FALSE;
    if (socket_connect (conn, sin, verbose) == 0)
      return 0;
  }
  
  if (do_preconnect)
  {
    int ret;

    dprint (1, (debugfile, "Preconnect to server %s:\n", conn->account.host));
    dprint (1, (debugfile, "\t%s\n", Preconnect));
    /* Execute preconnect command */
    ret = mutt_system (Preconnect) < 0;
    dprint (1, (debugfile, "\t%s: %d\n", "Exit status", ret));
    if (ret < 0)
    {
      mutt_perror (_("Preconnect command failed."));
      sleep (1);

      return ret;
    }
  }
  
  verbose = TRUE;

  return socket_connect (conn, sin, verbose);
}
