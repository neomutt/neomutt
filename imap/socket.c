/*
 * Copyright (C) 1998 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "globals.h"
#include "imap_socket.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

/* support for multiple socket connections */

static CONNECTION *Connections = NULL;

/* simple read buffering to speed things up. */
int mutt_socket_readchar (CONNECTION *conn, char *c)
{
  if (conn->bufpos >= conn->available)
  {
    conn->available = read (conn->fd, conn->inbuf, LONG_STRING);
    conn->bufpos = 0;
    if (conn->available <= 0)
      return conn->available; /* returns 0 for EOF or -1 for other error */
  }
  *c = conn->inbuf[conn->bufpos];
  conn->bufpos++;
  return 1;
}

int mutt_socket_read_line (char *buf, size_t buflen, CONNECTION *conn)
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
  return (i + 1);
}

int mutt_socket_read_line_d (char *buf, size_t buflen, CONNECTION *conn)
{
  int r = mutt_socket_read_line (buf, buflen, conn);
  dprint (1,(debugfile,"< %s\n", buf));
  return r;
}

int mutt_socket_write (CONNECTION *conn, const char *buf)
{
  dprint (1,(debugfile,"> %s", buf));
  return (write (conn->fd, buf, mutt_strlen (buf)));
}

CONNECTION *mutt_socket_select_connection (char *host, int port, int flags)
{
  CONNECTION *conn;

  if (flags != M_NEW_SOCKET)
  {
    conn = Connections;
    while (conn)
    {
      if (!mutt_strcmp (host, conn->server) && (port == conn->port))
	return conn;
      conn = conn->next;
    }
  }
  conn = (CONNECTION *) safe_calloc (1, sizeof (CONNECTION));
  conn->bufpos = 0;
  conn->available = 0;
  conn->uses = 0;
  conn->server = safe_strdup (host);
  conn->preconnect = safe_strdup (ImapPreconnect); 
  conn->port = port;
  conn->next = Connections;
  Connections = conn;

  return conn;
}

static int try_socket_and_connect (CONNECTION *conn, struct sockaddr_in sin,
			    int verbose)
{
  if ((conn->fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
  {
    if (verbose) 
      mutt_perror ("socket");
    return (-1);
  }

  if (connect (conn->fd, (struct sockaddr *) &sin, sizeof (sin)) < 0)
  {
    close (conn->fd);
    if (verbose) 
    {
      mutt_perror ("connect");
      sleep (1);
    }
    return (-1);
  }

  return 0;
}

int mutt_socket_open_connection (CONNECTION *conn)
{
  struct sockaddr_in sin;
  struct hostent *he;
  int    verbose;
  char *pc = conn->preconnect;
  int  do_preconnect = (pc && strlen (pc) > 0);
  /* This might be a config variable */
  int first_try_without_preconnect = TRUE; 

  memset (&sin, 0, sizeof (sin));
  sin.sin_port = htons (conn->port);
  sin.sin_family = AF_INET;
  if ((he = gethostbyname (conn->server)) == NULL)
  {
    mutt_perror (conn->server);
    return (-1);
  }
  memcpy (&sin.sin_addr, he->h_addr_list[0], he->h_length);

  mutt_message (_("Connecting to %s..."), conn->server); 

  if (do_preconnect && first_try_without_preconnect)
  {
    verbose = FALSE;
    if (try_socket_and_connect (conn, sin, verbose) == 0)
      return 0;
  }
  
  if (do_preconnect)
  {
    int ret;

    dprint (1,(debugfile,"Preconnect to server %s:\n", conn->server));
    dprint (1,(debugfile,"\t%s\n", conn->preconnect));
    /* Execute preconnect command */
    ret = mutt_system (conn->preconnect) < 0;
    dprint (1,(debugfile,"\t%s: %d\n", "Exit status", ret));
    if (ret < 0)
    {
      mutt_perror(_("preconnect command failed"));
      sleep (1);
      return ret;
    }
  }
  
  verbose = TRUE;
  return try_socket_and_connect (conn, sin, verbose);
}
