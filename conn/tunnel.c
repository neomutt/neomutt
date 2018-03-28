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
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "tunnel.h"
#include "account.h"
#include "conn_globals.h"
#include "connection.h"
#include "protos.h"
#include "socket.h"

/**
 * struct TunnelData - A network tunnel (pair of sockets)
 */
struct TunnelData
{
  pid_t pid;
  int readfd;
  int writefd;
};

/**
 * tunnel_socket_open - Open a tunnel socket
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error
 */
static int tunnel_socket_open(struct Connection *conn)
{
  int pin[2], pout[2];

  struct TunnelData *tunnel = mutt_mem_malloc(sizeof(struct TunnelData));
  conn->sockdata = tunnel;

  mutt_message(_("Connecting with \"%s\"..."), Tunnel);

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
    mutt_sig_unblock_system(0);
    const int devnull = open("/dev/null", O_RDWR);
    if (devnull < 0 || dup2(pout[0], STDIN_FILENO) < 0 ||
        dup2(pin[1], STDOUT_FILENO) < 0 || dup2(devnull, STDERR_FILENO) < 0)
    {
      _exit(127);
    }
    close(pin[0]);
    close(pin[1]);
    close(pout[0]);
    close(pout[1]);
    close(devnull);

    /* Don't let the subprocess think it can use the controlling tty */
    setsid();

    execle(EXECSHELL, "sh", "-c", Tunnel, NULL, mutt_envlist_getlist());
    _exit(127);
  }
  mutt_sig_unblock_system(1);

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
  if (close(pin[1]) < 0 || close(pout[0]) < 0)
    mutt_perror("close");

  fcntl(pin[0], F_SETFD, FD_CLOEXEC);
  fcntl(pout[1], F_SETFD, FD_CLOEXEC);

  tunnel->readfd = pin[0];
  tunnel->writefd = pout[1];
  tunnel->pid = pid;

  conn->fd = 42; /* stupid hack */

  return 0;
}

/**
 * tunnel_socket_close - Close a tunnel socket
 * @param conn Connection to a server
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
static int tunnel_socket_close(struct Connection *conn)
{
  struct TunnelData *tunnel = (struct TunnelData *) conn->sockdata;
  int status;

  close(tunnel->readfd);
  close(tunnel->writefd);
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
 * tunnel_socket_read - Read data from a tunnel socket
 * @param conn Connection to a server
 * @param buf Buffer to store the data
 * @param len Number of bytes to read
 * @retval >0 Success, number of bytes read
 * @retval -1 Error, see errno
 */
static int tunnel_socket_read(struct Connection *conn, char *buf, size_t len)
{
  struct TunnelData *tunnel = (struct TunnelData *) conn->sockdata;
  int rc;

  rc = read(tunnel->readfd, buf, len);
  if (rc == -1)
  {
    mutt_error(_("Tunnel error talking to %s: %s"), conn->account.host, strerror(errno));
  }

  return rc;
}

/**
 * tunnel_socket_write - Write data to a tunnel socket
 * @param conn Connection to a server
 * @param buf  Buffer to read into
 * @param len  Number of bytes to read
 * @retval >0 Success, number of bytes written
 * @retval -1 Error, see errno
 */
static int tunnel_socket_write(struct Connection *conn, const char *buf, size_t len)
{
  struct TunnelData *tunnel = (struct TunnelData *) conn->sockdata;
  int rc;

  rc = write(tunnel->writefd, buf, len);
  if (rc == -1)
  {
    mutt_error(_("Tunnel error talking to %s: %s"), conn->account.host, strerror(errno));
  }

  return rc;
}

/**
 * tunnel_socket_poll - Checks whether tunnel reads would block
 * @param conn Connection to a server
 * @param wait_secs How long to wait for a response
 * @retval >0 There is data to read
 * @retval  0 Read would block
 * @retval -1 Connection doesn't support polling
 */
static int tunnel_socket_poll(struct Connection *conn, time_t wait_secs)
{
  struct TunnelData *tunnel = (struct TunnelData *) conn->sockdata;
  int ofd;
  int rc;

  ofd = conn->fd;
  conn->fd = tunnel->readfd;
  rc = raw_socket_poll(conn, wait_secs);
  conn->fd = ofd;

  return rc;
}

/**
 * mutt_tunnel_socket_setup - setups tunnel connection functions.
 * @param conn Connection to assign functions to
 *
 * Assign tunnel socket functions to the Connection conn.
 */
void mutt_tunnel_socket_setup(struct Connection *conn)
{
  conn->conn_open = tunnel_socket_open;
  conn->conn_close = tunnel_socket_close;
  conn->conn_read = tunnel_socket_read;
  conn->conn_write = tunnel_socket_write;
  conn->conn_poll = tunnel_socket_poll;
}
