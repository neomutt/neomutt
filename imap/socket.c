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
#include "imap.h"
#include "imap_socket.h"
#include "imap_private.h"
#ifdef USE_SSL
#include "imap_ssl.h"
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
  int rc;
  
  rc = conn->open (conn);
  if (!rc)
    conn->up = 1;

  return rc;
}

int mutt_socket_close (CONNECTION* conn)
{
  conn->up = 0;

  return conn->close (conn);
}

int mutt_socket_write_d (CONNECTION *conn, const char *buf, int dbg)
{
  dprint (dbg, (debugfile,"> %s", buf));

  return conn->write (conn, buf);
}

/* simple read buffering to speed things up. */
int mutt_socket_readchar (CONNECTION *conn, char *c)
{
  if (conn->bufpos >= conn->available)
  {
    conn->available = conn->read (conn);
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

  for (i = 0; i < buflen; i++)
  {
    if (mutt_socket_readchar (conn, &ch) != 1)
      return (-1);
    if (ch == '\n')
      break;
    buf[i] = ch;
  }
  if (i)
    buf[i-1] = '\0';
  else
    buf[i] = '\0';

  dprint (dbg, (debugfile, "< %s\n", buf));
  
  return (i + 1);
}

CONNECTION* mutt_socket_find (const IMAP_MBOX* mx, int newconn)
{
  CONNECTION* conn;

  if (! newconn)
  {
    conn = Connections;
    while (conn)
    {
      if (imap_account_match (mx, &conn->mx))
	return conn;
      conn = conn->next;
    }
  }

  conn = socket_new_conn ();
  memcpy (&conn->mx, mx, sizeof (conn->mx));
  conn->mx.mbox = 0;

  conn->next = Connections;
  Connections = conn;

#ifdef USE_SSL
  if (mx->socktype == M_NEW_SSL_SOCKET) 
  {
    ssl_socket_setup (conn);
  }
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

/* imap_logout_all: close all open connections. Quick and dirty until we can
 *   make sure we've got all the context we need. */
void imap_logout_all (void) 
{
  CONNECTION* conn;
  
  conn = Connections;

  while (conn)
  {
    if (conn->up)
    {
      mutt_message (_("Closing connection to %s..."),
		    conn->mx.host);
      
      imap_logout (conn);
      
      mutt_clear_error ();

      mutt_socket_close (conn);
    }
    
    Connections = conn->next;

    if (conn->data) {
      dprint (2, (debugfile,
        "imap_logout_all: Connection still has valid CONTEXT?!\n"));
    }

    free (conn);

    conn = Connections;
  }
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

int raw_socket_write (CONNECTION *conn, const char *buf)
{
  return write (conn->fd, buf, mutt_strlen (buf));
}

int raw_socket_open (CONNECTION *conn)
{
  struct sockaddr_in sin;
  struct hostent *he;
  int    verbose;
  int  do_preconnect = mutt_strlen (ImapPreconnect) > 0;
  /* This might be a config variable */
  int first_try_without_preconnect = TRUE; 

  memset (&sin, 0, sizeof (sin));
  sin.sin_port = htons (conn->mx.port);
  sin.sin_family = AF_INET;
  if ((he = gethostbyname (conn->mx.host)) == NULL)
  {
    mutt_error (_("Could not find the host \"%s\""), conn->mx.host);
	
    return -1;
  }
  memcpy (&sin.sin_addr, he->h_addr_list[0], he->h_length);

  mutt_message (_("Connecting to %s..."), conn->mx.host); 

  if (do_preconnect && first_try_without_preconnect)
  {
    verbose = FALSE;
    if (socket_connect (conn, sin, verbose) == 0)
      return 0;
  }
  
  if (do_preconnect)
  {
    int ret;

    dprint (1, (debugfile, "Preconnect to server %s:\n", conn->mx.host));
    dprint (1, (debugfile, "\t%s\n", ImapPreconnect));
    /* Execute preconnect command */
    ret = mutt_system (ImapPreconnect) < 0;
    dprint (1, (debugfile, "\t%s: %d\n", "Exit status", ret));
    if (ret < 0)
    {
      mutt_perror (_("IMAP Preconnect command failed"));
      sleep (1);

      return ret;
    }
  }
  
  verbose = TRUE;

  return socket_connect (conn, sin, verbose);
}
