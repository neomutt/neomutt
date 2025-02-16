/**
 * @file
 * Low-level socket handling
 *
 * @authors
 * Copyright (C) 2017 Damien Riegel <damien.riegel@gmail.com>
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2023 Pietro Cerutti <gahr@gahr.ch>
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
 * @page conn_raw Low-level socket code
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
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "connaccount.h"
#include "connection.h"
#include "globals.h"
#ifdef HAVE_LIBIDN
#include "address/lib.h"
#endif
#ifdef HAVE_GETADDRINFO
#include <stdbool.h>
#endif

/**
 * socket_connect - Set up to connect to a socket fd
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
  struct sigaction oldalrm = { 0 };
  struct sigaction act = { 0 };

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

  /* Batch mode does not call mutt_signal_init(), so ensure the alarm
   * interrupts the connect call */
  const short c_socket_timeout = cs_subset_number(NeoMutt->sub, "socket_timeout");
  if (c_socket_timeout > 0)
  {
    sigemptyset(&act.sa_mask);
    act.sa_handler = mutt_sig_empty_handler;
#ifdef SA_INTERRUPT
    act.sa_flags = SA_INTERRUPT;
#else
    act.sa_flags = 0;
#endif
    sigaction(SIGALRM, &act, &oldalrm);
    alarm(c_socket_timeout);
  }

  mutt_sig_allow_interrupt(true);

  /* FreeBSD's connect() does not respect SA_RESTART, meaning
   * a SIGWINCH will cause the connect to fail. */
  sigemptyset(&set);
  sigaddset(&set, SIGWINCH);
  sigprocmask(SIG_BLOCK, &set, NULL);

  save_errno = 0;

  if (c_socket_timeout > 0)
  {
    const struct timeval tv = { c_socket_timeout, 0 };
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
      mutt_debug(LL_DEBUG2, "Cannot set socket receive timeout. errno: %d\n", errno);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
    {
      mutt_debug(LL_DEBUG2, "Cannot set socket send timeout. errno: %d\n", errno);
    }
  }

  if (connect(fd, sa, sa_size) < 0)
  {
    save_errno = errno;
    mutt_debug(LL_DEBUG2, "Connection failed. errno: %d\n", errno);
    SigInt = false; /* reset in case we caught SIGINTR while in connect() */
  }

  if (c_socket_timeout > 0)
  {
    alarm(0);
    sigaction(SIGALRM, &oldalrm, NULL);
  }
  mutt_sig_allow_interrupt(false);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  return save_errno;
}

/**
 * raw_socket_open - Open a socket - Implements Connection::open() - @ingroup connection_open
 */
int raw_socket_open(struct Connection *conn)
{
  int rc;

  char *host_idna = NULL;

#ifdef HAVE_GETADDRINFO
  /* --- IPv4/6 --- */

  /* "65536\0" */
  char port[6] = { 0 };
  struct addrinfo hints = { 0 };
  struct addrinfo *res = NULL;
  struct addrinfo *cur = NULL;

  /* we accept v4 or v6 STREAM sockets */
  const bool c_use_ipv6 = cs_subset_bool(NeoMutt->sub, "use_ipv6");
  if (c_use_ipv6)
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

  if (OptGui)
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

  if (OptGui)
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
        (void) fcntl(fd, F_SETFD, FD_CLOEXEC);
        conn->fd = fd;
        break;
      }
      else
      {
        close(fd);
      }
    }
  }

  freeaddrinfo(res);
#else
  /* --- IPv4 only --- */

  struct hostent *he = NULL;
  struct sockaddr_in sin = { 0 };
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

  if (OptGui)
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

  if (OptGui)
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
      {
        close(fd);
      }
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
 * raw_socket_read - Read data from a socket - Implements Connection::read() - @ingroup connection_read
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
    SigInt = false;
  }
  mutt_sig_allow_interrupt(false);

  if (SigInt)
  {
    mutt_error(_("Connection to %s has been aborted"), conn->account.host);
    SigInt = false;
    rc = -1;
  }

  return rc;
}

/**
 * raw_socket_write - Write data to a socket - Implements Connection::write() - @ingroup connection_write
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
  } while ((sent < count) && !SigInt);

  mutt_sig_allow_interrupt(false);
  return sent;
}

/**
 * raw_socket_poll - Check if any data is waiting on a socket - Implements Connection::poll() - @ingroup connection_poll
 */
int raw_socket_poll(struct Connection *conn, time_t wait_secs)
{
  if (conn->fd < 0)
    return -1;

  fd_set rfds = { 0 };
  struct timeval tv = { 0 };

  uint64_t wait_millis = wait_secs * 1000UL;

  while (true)
  {
    tv.tv_sec = wait_millis / 1000;
    tv.tv_usec = (wait_millis % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_SET(conn->fd, &rfds);

    uint64_t pre_t = mutt_date_now_ms();
    const int rc = select(conn->fd + 1, &rfds, NULL, NULL, &tv);
    uint64_t post_t = mutt_date_now_ms();

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
 * raw_socket_close - Close a socket - Implements Connection::close() - @ingroup connection_close
 */
int raw_socket_close(struct Connection *conn)
{
  return close(conn->fd);
}
