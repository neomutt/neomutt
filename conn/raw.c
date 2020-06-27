/**
 * @file
 * Low-level socket handling
 *
 * @authors
 * Copyright (C) 1998,2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2006,2008 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>
 * Copyright (C) 2017 Damien Riegel <damien.riegel@gmail.com>
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
 * @page conn_raw Low-level socket handling
 *
 * Low-level socket handling
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "gui/lib.h"
#include "connaccount.h"
#include "connection.h"
#include "globals.h"
#include "mutt_globals.h"
#include "options.h"
#include "socket.h"
#ifdef HAVE_LIBIDN
#include "address/lib.h"
#endif
#ifdef HAVE_GETADDRINFO
#include <stdbool.h>
#endif

/* These Config Variables are only used in conn/conn_raw.c */
#ifdef HAVE_GETADDRINFO
bool C_UseIpv6; ///< Config: Lookup IPv6 addresses when making connections
#endif

/**
 * socket_connect - set up to connect to a socket fd
 * @param fd File descriptor to connect with
 * @param sa Address info
 * @retval  0 Success
 * @retval >0 An errno, e.g. EPERM
 * @retval -1 Error
 */
static int socket_connect(int fd, struct sockaddr *sa)
{
  int sa_size;
  int save_errno;
  sigset_t set;

  if (sa->sa_family == AF_INET)
    sa_size = sizeof(struct sockaddr_in);
#ifdef HAVE_GETADDRINFO
  else if (sa->sa_family == AF_INET6)
    sa_size = sizeof(struct sockaddr_in6);
#endif
  else
  {
    mutt_debug(LL_DEBUG1, "Unknown address family!\n");
    return -1;
  }

  if (C_ConnectTimeout > 0)
    alarm(C_ConnectTimeout);

  mutt_sig_allow_interrupt(true);

  /* FreeBSD's connect() does not respect SA_RESTART, meaning
   * a SIGWINCH will cause the connect to fail. */
  sigemptyset(&set);
  sigaddset(&set, SIGWINCH);
  sigprocmask(SIG_BLOCK, &set, NULL);

  save_errno = 0;

  if (connect(fd, sa, sa_size) < 0)
  {
    save_errno = errno;
    mutt_debug(LL_DEBUG2, "Connection failed. errno: %d\n", errno);
    SigInt = 0; /* reset in case we caught SIGINTR while in connect() */
  }

  if (C_ConnectTimeout > 0)
    alarm(0);
  mutt_sig_allow_interrupt(false);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  return save_errno;
}

/**
 * raw_socket_open - Open a socket - Implements Connection::open()
 */
int raw_socket_open(struct Connection *conn)
{
  int rc;

  char *host_idna = NULL;

#ifdef HAVE_GETADDRINFO
  /* --- IPv4/6 --- */

  /* "65536\0" */
  char port[6];
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  struct addrinfo *cur = NULL;

  /* we accept v4 or v6 STREAM sockets */
  memset(&hints, 0, sizeof(hints));

  if (C_UseIpv6)
    hints.ai_family = AF_UNSPEC;
  else
    hints.ai_family = AF_INET;

  hints.ai_socktype = SOCK_STREAM;

  snprintf(port, sizeof(port), "%d", conn->account.port);

#ifdef HAVE_LIBIDN
  if (mutt_idna_to_ascii_lz(conn->account.host, &host_idna, 1) != 0)
  {
    mutt_error(_("Bad IDN: '%s'"), conn->account.host);
    return -1;
  }
#else
  host_idna = conn->account.host;
#endif

  if (!OptNoCurses)
    mutt_message(_("Looking up %s..."), conn->account.host);

  rc = getaddrinfo(host_idna, port, &hints, &res);

#ifdef HAVE_LIBIDN
  FREE(&host_idna);
#endif

  if (rc)
  {
    mutt_error(_("Could not find the host \"%s\""), conn->account.host);
    return -1;
  }

  if (!OptNoCurses)
    mutt_message(_("Connecting to %s..."), conn->account.host);

  rc = -1;
  for (cur = res; cur; cur = cur->ai_next)
  {
    int fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
    if (fd >= 0)
    {
      rc = socket_connect(fd, cur->ai_addr);
      if (rc == 0)
      {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        conn->fd = fd;
        break;
      }
      else
        close(fd);
    }
  }

  freeaddrinfo(res);
#else
  /* --- IPv4 only --- */

  struct sockaddr_in sin;
  struct hostent *he = NULL;

  memset(&sin, 0, sizeof(sin));
  sin.sin_port = htons(conn->account.port);
  sin.sin_family = AF_INET;

#ifdef HAVE_LIBIDN
  if (mutt_idna_to_ascii_lz(conn->account.host, &host_idna, 1) != 0)
  {
    mutt_error(_("Bad IDN: '%s'"), conn->account.host);
    return -1;
  }
#else
  host_idna = conn->account.host;
#endif

  if (!OptNoCurses)
    mutt_message(_("Looking up %s..."), conn->account.host);

  he = gethostbyname(host_idna);

#ifdef HAVE_LIBIDN
  FREE(&host_idna);
#endif

  if (!he)
  {
    mutt_error(_("Could not find the host \"%s\""), conn->account.host);

    return -1;
  }

  if (!OptNoCurses)
    mutt_message(_("Connecting to %s..."), conn->account.host);

  rc = -1;
  for (int i = 0; he->h_addr_list[i]; i++)
  {
    memcpy(&sin.sin_addr, he->h_addr_list[i], he->h_length);
    int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);

    if (fd >= 0)
    {
      rc = socket_connect(fd, (struct sockaddr *) &sin);
      if (rc == 0)
      {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        conn->fd = fd;
        break;
      }
      else
        close(fd);
    }
  }
#endif
  if (rc)
  {
    mutt_error(_("Could not connect to %s (%s)"), conn->account.host,
               (rc > 0) ? strerror(rc) : _("unknown error"));
    return -1;
  }

  return 0;
}

/**
 * raw_socket_read - Read data from a socket - Implements Connection::read()
 */
int raw_socket_read(struct Connection *conn, char *buf, size_t count)
{
  int rc;

  mutt_sig_allow_interrupt(true);
  do
  {
    rc = read(conn->fd, buf, count);
  } while (rc < 0 && (errno == EINTR));

  if (rc < 0)
  {
    mutt_error(_("Error talking to %s (%s)"), conn->account.host, strerror(errno));
    SigInt = 0;
  }
  mutt_sig_allow_interrupt(false);

  if (SigInt)
  {
    mutt_error(_("Connection to %s has been aborted"), conn->account.host);
    SigInt = 0;
    rc = -1;
  }

  return rc;
}

/**
 * raw_socket_write - Write data to a socket - Implements Connection::write()
 */
int raw_socket_write(struct Connection *conn, const char *buf, size_t count)
{
  int rc;
  size_t sent = 0;

  mutt_sig_allow_interrupt(true);
  do
  {
    do
    {
      rc = write(conn->fd, buf + sent, count - sent);
    } while (rc < 0 && (errno == EINTR));

    if (rc < 0)
    {
      mutt_error(_("Error talking to %s (%s)"), conn->account.host, strerror(errno));
      mutt_sig_allow_interrupt(false);
      return -1;
    }

    sent += rc;
  } while ((sent < count) && (SigInt == 0));

  mutt_sig_allow_interrupt(false);
  return sent;
}

/**
 * raw_socket_poll - Checks whether reads would block - Implements Connection::poll()
 */
int raw_socket_poll(struct Connection *conn, time_t wait_secs)
{
  if (conn->fd < 0)
    return -1;

  fd_set rfds;
  struct timeval tv;

  uint64_t wait_millis = wait_secs * 1000UL;

  while (true)
  {
    tv.tv_sec = wait_millis / 1000;
    tv.tv_usec = (wait_millis % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_SET(conn->fd, &rfds);

    uint64_t pre_t = mutt_date_epoch_ms();
    const int rc = select(conn->fd + 1, &rfds, NULL, NULL, &tv);
    uint64_t post_t = mutt_date_epoch_ms();

    if ((rc > 0) || ((rc < 0) && (errno != EINTR)))
      return rc;

    if (SigInt)
      mutt_query_exit();

    wait_millis += pre_t;
    if (wait_millis <= post_t)
      return 0;
    wait_millis -= post_t;
  }
}

/**
 * raw_socket_close - Close a socket - Implements Connection::close()
 */
int raw_socket_close(struct Connection *conn)
{
  return close(conn->fd);
}
