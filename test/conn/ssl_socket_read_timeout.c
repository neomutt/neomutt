/**
 * @file
 * Test the OpenSSL read timeout
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
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "ssl_timeout_common.h"
#include "test_common.h" // IWYU pragma: keep

static void run_reader(struct Connection *conn, int result_fd)
{
  struct SslTimeoutResult result = { 0 };
  char buf[16] = { 0 };
  uint64_t start = mutt_date_now_ms();
  result.rc = mutt_socket_read(conn, buf, sizeof(buf));
  result.elapsed = mutt_date_now_ms() - start;

  if (!test_ssl_timeout_write_result(result_fd, &result))
    _exit(2);
  close(result_fd);
  _exit(0);
}

/**
 * test_ssl_socket_read_timeout - Test that OpenSSL read retries honor socket_timeout
 *
 * Regression test for #4995.  The loopback TLS server completes the handshake,
 * but does not send application data.  The read must return after the socket
 * timeout instead of immediately retrying another blocking SSL_read().
 */
void test_ssl_socket_read_timeout(void)
{
  SSL_CTX *server_ctx = NULL;
  struct Connection *conn = NULL;
  int listener = -1;
  pid_t server_pid = -1;
  pid_t reader_pid = -1;
  int result_fds[2] = { -1, -1 };
  bool connection_open = false;
  char cert_path[64] = { 0 };
  unsigned short port = 0;

  if (!test_ssl_timeout_create_server_context(&server_ctx, cert_path, sizeof(cert_path)))
    goto done;
  if (!test_ssl_timeout_configure_client(cert_path))
    goto done;

  listener = test_ssl_timeout_create_listener(&port);
  if (listener < 0)
    goto done;

  pid_t parent_pid = getpid();
  server_pid = fork();
  if (!TEST_CHECK(server_pid >= 0))
    goto done;
  if (server_pid == 0)
    test_ssl_timeout_run_idle_server(listener, server_ctx, parent_pid);

  close(listener);
  listener = -1;
  SSL_CTX_free(server_ctx);
  server_ctx = NULL;

  conn = test_ssl_timeout_new_connection(port);
  if (!TEST_CHECK(conn != NULL))
    goto done;

  if (!TEST_CHECK(mutt_socket_open(conn) == 0))
    goto done;
  connection_open = true;

  // Isolate the potentially unbounded read so a regression cannot hang the
  // entire Acutest process; the pipe carries its result back to the parent.
  if (!TEST_CHECK(pipe(result_fds) == 0))
    goto done;
  reader_pid = fork();
  if (!TEST_CHECK(reader_pid >= 0))
    goto done;
  if (reader_pid == 0)
  {
    close(result_fds[0]);
    run_reader(conn, result_fds[1]);
  }
  close(result_fds[1]);
  result_fds[1] = -1;

  int reader_status = 0;
  if (!TEST_CHECK(test_ssl_timeout_wait(&reader_pid, &reader_status)))
    goto done;
  if (!TEST_CHECK(WIFEXITED(reader_status) && (WEXITSTATUS(reader_status) == 0)))
    goto done;

  struct SslTimeoutResult result = { 0 };
  if (!TEST_CHECK(test_ssl_timeout_read_result(result_fds[0], &result)))
    goto done;
  TEST_MSG("read rc=%d elapsed=%llu ms", result.rc, (unsigned long long) result.elapsed);
  TEST_CHECK(result.rc < 0);
  TEST_CHECK(result.elapsed >= 1000);
  TEST_CHECK(result.elapsed <= 5000);

  // Prove that the read ended from the client timeout, not peer closure.
  int status = 0;
  pid_t server_state = waitpid(server_pid, &status, WNOHANG);
  TEST_CHECK(server_state == 0);
  if (server_state == server_pid)
    server_pid = -1;

done:
  test_ssl_timeout_stop_child(&reader_pid);
  if (result_fds[0] >= 0)
    close(result_fds[0]);
  if (result_fds[1] >= 0)
    close(result_fds[1]);
  if (connection_open)
    mutt_socket_close(conn);
  FREE(&conn);
  test_ssl_timeout_stop_child(&server_pid);
  if (listener >= 0)
    close(listener);
  SSL_CTX_free(server_ctx);
  if (cert_path[0])
    unlink(cert_path);
}
#endif
