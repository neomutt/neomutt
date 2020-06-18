/**
 * @file
 * Support for network tunnelling
 *
 * @authors
 * Copyright (C) 2000 Manoj Kasichainula <manoj@io.com>
 * Copyright (C) 2001,2005 Brendan Cully <brendan@kublai.com>
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
 * @page conn_tunnel Support for network tunnelling
 *
 * Support for network tunnelling
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "connaccount.h"
#include "connection.h"
#include "globals.h"
#include "mutt_globals.h"

/**
 * struct TunnelSockData - A network tunnel (pair of sockets)
 */
struct TunnelSockData
{
  pid_t pid;    ///< Process ID of tunnel program
  int fd_read;  ///< File descriptor to read from
  int fd_write; ///< File descriptor to write to
};

/**
 * tunnel_socket_open - Open a tunnel socket - Implements Connection::open()
 */
static int tunnel_socket_open(struct Connection *conn)
{
  int pin[2], pout[2];

  struct TunnelSockData *tunnel = mutt_mem_malloc(sizeof(struct TunnelSockData));
  conn->sockdata = tunnel;

  mutt_message(_("Connecting with \"%s\"..."), C_Tunnel);

  int rc = pipe(pin);
  if (rc == -1)
  {
    mutt_perror("pipe");
    FREE(&conn->sockdata);
    return -1;
  }
  rc = pipe(pout);
  if (rc == -1)
  {
    mutt_perror("pipe");
    close(pin[0]);
    close(pin[1]);
    FREE(&conn->sockdata);
    return -1;
  }

  mutt_sig_block_system();
  int pid = fork();
  if (pid == 0)
  {
    mutt_sig_unblock_system(false);
    const int fd_null = open("/dev/null", O_RDWR);
    if ((fd_null < 0) || (dup2(pout[0], STDIN_FILENO) < 0) ||
        (dup2(pin[1], STDOUT_FILENO) < 0) || (dup2(fd_null, STDERR_FILENO) < 0))
    {
      _exit(127);
    }
    close(pin[0]);
    close(pin[1]);
    close(pout[0]);
    close(pout[1]);
    close(fd_null);

    /* Don't let the subprocess think it can use the controlling tty */
    setsid();

    execle(EXEC_SHELL, "sh", "-c", C_Tunnel, NULL, mutt_envlist_getlist());
    _exit(127);
  }
  mutt_sig_unblock_system(true);

  if (pid == -1)
  {
    mutt_perror("fork");
    close(pin[0]);
    close(pin[1]);
    close(pout[0]);
    close(pout[1]);
    FREE(&conn->sockdata);
    return -1;
  }
  if ((close(pin[1]) < 0) || (close(pout[0]) < 0))
    mutt_perror("close");

  fcntl(pin[0], F_SETFD, FD_CLOEXEC);
  fcntl(pout[1], F_SETFD, FD_CLOEXEC);

  tunnel->fd_read = pin[0];
  tunnel->fd_write = pout[1];
  tunnel->pid = pid;

  conn->fd = 42; /* stupid hack */

  return 0;
}

/**
 * tunnel_socket_read - Read data from a tunnel socket - Implements Connection::read()
 */
static int tunnel_socket_read(struct Connection *conn, char *buf, size_t count)
{
  struct TunnelSockData *tunnel = conn->sockdata;
  int rc;

  do
  {
    rc = read(tunnel->fd_read, buf, count);
  } while (rc < 0 && errno == EINTR);

  if (rc < 0)
  {
    mutt_error(_("Tunnel error talking to %s: %s"), conn->account.host, strerror(errno));
    return -1;
  }

  return rc;
}

/**
 * tunnel_socket_write - Write data to a tunnel socket - Implements Connection::write()
 */
static int tunnel_socket_write(struct Connection *conn, const char *buf, size_t count)
{
  struct TunnelSockData *tunnel = conn->sockdata;
  int rc;
  size_t sent = 0;

  do
  {
    do
    {
      rc = write(tunnel->fd_write, buf + sent, count - sent);
    } while (rc < 0 && errno == EINTR);

    if (rc < 0)
    {
      mutt_error(_("Tunnel error talking to %s: %s"), conn->account.host, strerror(errno));
      return -1;
    }

    sent += rc;
  } while (sent < count);

  return sent;
}

/**
 * tunnel_socket_poll - Checks whether tunnel reads would block - Implements Connection::poll()
 */
static int tunnel_socket_poll(struct Connection *conn, time_t wait_secs)
{
  struct TunnelSockData *tunnel = conn->sockdata;
  int ofd;
  int rc;

  ofd = conn->fd;
  conn->fd = tunnel->fd_read;
  rc = raw_socket_poll(conn, wait_secs);
  conn->fd = ofd;

  return rc;
}

/**
 * tunnel_socket_close - Close a tunnel socket - Implements Connection::close()
 */
static int tunnel_socket_close(struct Connection *conn)
{
  struct TunnelSockData *tunnel = conn->sockdata;
  if (!tunnel)
  {
    return 0;
  }

  int status;

  close(tunnel->fd_read);
  close(tunnel->fd_write);
  waitpid(tunnel->pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status))
  {
    mutt_error(_("Tunnel to %s returned error %d (%s)"), conn->account.host,
               WEXITSTATUS(status), NONULL(mutt_str_sysexit(WEXITSTATUS(status))));
  }
  FREE(&conn->sockdata);

  return 0;
}

/**
 * mutt_tunnel_socket_setup - sets up tunnel connection functions
 * @param conn Connection to assign functions to
 *
 * Assign tunnel socket functions to the Connection conn.
 */
void mutt_tunnel_socket_setup(struct Connection *conn)
{
  conn->open = tunnel_socket_open;
  conn->close = tunnel_socket_close;
  conn->read = tunnel_socket_read;
  conn->write = tunnel_socket_write;
  conn->poll = tunnel_socket_poll;
  /* Note we are using ssf as a boolean in this case.  See the notes in
   * conn/connection.h */
  if (C_TunnelIsSecure)
    conn->ssf = 1;
}
