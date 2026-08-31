/**
 * @file
 * Test the OpenSSL handshake timeout
 *
 * @authors
 * Copyright (C) 2026 Todd A. Gibson <tgibson@augustcouncil.com>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#ifdef USE_SSL_OPENSSL
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "ssl_timeout_common.h"
#include "test_common.h" // IWYU pragma: keep

static void run_silent_peer(int listener, pid_t parent_pid)
{
  int client = accept(listener, NULL, NULL);
  close(listener);
  if (client < 0)
    _exit(2);

  // Keep TCP open without sending a TLS response while the client negotiates.
  for (int i = 0; (i < 100) && (getppid() == parent_pid); i++)
    usleep(100000);

  close(client);
  _exit(0);
}

static void run_opener(unsigned short port, int result_fd)
{
  struct Connection *conn = test_ssl_timeout_new_connection(port);
  if (!conn)
    _exit(2);

  struct SslTimeoutResult result = { 0 };
  uint64_t start = mutt_date_now_ms();
  result.rc = mutt_socket_open(conn);
  result.elapsed = mutt_date_now_ms() - start;

  if (result.rc == 0)
    mutt_socket_close(conn);
  FREE(&conn);

  if (!test_ssl_timeout_write_result(result_fd, &result))
    _exit(2);
  close(result_fd);
  _exit(0);
}

/**
 * test_ssl_negotiate_timeout - Test that OpenSSL negotiation honors socket_timeout
 *
 * Regression test for #5005.  The loopback peer accepts TCP but does not send a
 * TLS response.  Negotiation must return after the socket timeout instead of
 * immediately retrying another blocking SSL_connect().
 */
void test_ssl_negotiate_timeout(void)
{
  int listener = -1;
  pid_t peer_pid = -1;
  pid_t opener_pid = -1;
  int result_fds[2] = { -1, -1 };
  unsigned short port = 0;

  if (!test_ssl_timeout_configure_client(NULL))
    goto done;

  listener = test_ssl_timeout_create_listener(&port);
  if (listener < 0)
    goto done;

  pid_t parent_pid = getpid();
  peer_pid = fork();
  if (!TEST_CHECK(peer_pid >= 0))
    goto done;
  if (peer_pid == 0)
    run_silent_peer(listener, parent_pid);

  close(listener);
  listener = -1;

  if (!TEST_CHECK(pipe(result_fds) == 0))
    goto done;
  opener_pid = fork();
  if (!TEST_CHECK(opener_pid >= 0))
    goto done;
  if (opener_pid == 0)
  {
    close(result_fds[0]);
    run_opener(port, result_fds[1]);
  }
  close(result_fds[1]);
  result_fds[1] = -1;

  int opener_status = 0;
  if (!TEST_CHECK(test_ssl_timeout_wait(&opener_pid, &opener_status)))
    goto done;
  if (!TEST_CHECK(WIFEXITED(opener_status) && (WEXITSTATUS(opener_status) == 0)))
    goto done;

  struct SslTimeoutResult result = { 0 };
  if (!TEST_CHECK(test_ssl_timeout_read_result(result_fds[0], &result)))
    goto done;
  TEST_MSG("open rc=%d elapsed=%llu ms", result.rc, (unsigned long long) result.elapsed);
  TEST_CHECK(result.rc < 0);
  TEST_CHECK(result.elapsed >= 1000);
  TEST_CHECK(result.elapsed <= 5000);

  // Prove that negotiation ended from the client timeout, not peer closure.
  int status = 0;
  pid_t peer_state = waitpid(peer_pid, &status, WNOHANG);
  TEST_CHECK(peer_state == 0);
  if (peer_state == peer_pid)
    peer_pid = -1;

done:
  test_ssl_timeout_stop_child(&opener_pid);
  if (result_fds[0] >= 0)
    close(result_fds[0]);
  if (result_fds[1] >= 0)
    close(result_fds[1]);
  test_ssl_timeout_stop_child(&peer_pid);
  if (listener >= 0)
    close(listener);
}
#endif
