/*
 * Copyright (C) 2000 Manoj Kasichainula <manoj@io.com>
 * Copyright (C) 2001 Brendan Cully <brendan@kublai.com>
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
#include "mutt_socket.h"
#include "mutt_tunnel.h"

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

/* -- data types -- */
typedef struct
{
  pid_t pid;
} TUNNEL_DATA;

/* forward declarations */
static int tunnel_socket_open (CONNECTION*);
static int tunnel_socket_close (CONNECTION*);

/* -- public functions -- */
int mutt_tunnel_socket_setup (CONNECTION *conn)
{
  conn->open = tunnel_socket_open;
  conn->read = raw_socket_read;
  conn->write = raw_socket_write;
  conn->close = tunnel_socket_close;

  return 0;
}

static int tunnel_socket_open (CONNECTION *conn)
{
  TUNNEL_DATA* tunnel;
  int pid;
  int rc;
  int sv[2];

  mutt_message (_("Connecting with \"%s\"..."), Tunnel);

  rc = socketpair (PF_UNIX, SOCK_STREAM, IPPROTO_IP, sv);
  if (rc == -1)
  {
    mutt_perror ("socketpair");
    return -1;
  }

  if ((pid = fork ()) == 0)
  {
    mutt_block_signals_system ();
    if (close (sv[0]) < 0 ||
	dup2 (sv[1], STDIN_FILENO) < 0 ||
	dup2 (sv[1], STDOUT_FILENO) < 0 ||
	close (sv[1]) < 0)
      _exit (127);

    execl (EXECSHELL, "sh", "-c", Tunnel, NULL);
    _exit (127);
  }
  if (pid == -1)
  {
    close (sv[0]);
    close (sv[1]);
    mutt_perror ("fork");
    return -1;
  }
  if (close(sv[1]) < 0)
    mutt_perror ("close");

  conn->fd = sv[0];
  tunnel = (TUNNEL_DATA*) safe_malloc (sizeof (TUNNEL_DATA));
  tunnel->pid = pid;

  return 0;
}

static int tunnel_socket_close (CONNECTION* conn)
{
  TUNNEL_DATA* tunnel = (TUNNEL_DATA*) conn->data;

  if (!tunnel)
    return 0;

  waitpid (tunnel->pid, NULL, 0);
  close (conn->fd);
  conn->fd = -1;
  FREE (&conn->data);

  return 0;
}
