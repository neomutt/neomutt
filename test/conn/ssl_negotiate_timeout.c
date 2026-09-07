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
#include <errno.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "conn/private.h"
#include "ssl_timeout_common.h"
#include "test_common.h" // IWYU pragma: keep

static volatile sig_atomic_t ReceivedSignal = 0;

static void record_signal(int sig)
{
  const int saved_errno = errno;
  ReceivedSignal = sig;
  if (sig == SIGINT)
    SigInt = 1;
  errno = saved_errno;
}

static void run_peer(int listener, SSL_CTX *server_ctx, int sig, pid_t opener_pid, pid_t parent_pid)
{
  int client = accept(listener, NULL, NULL);
  close(listener);
  if (client < 0)
    _exit(2);

  SSL *ssl = NULL;
  if (server_ctx || sig)
  {
    // Wait for ClientHello without consuming it, then let the client block.
    char byte = 0;
    if (recv(client, &byte, 1, MSG_PEEK) != 1)
      _exit(2);
    usleep(300000);
    if (sig && (kill(opener_pid, sig) != 0))
      _exit(2);
  }

  if (server_ctx)
  {
    // Readability must not be required immediately after the signal.
    usleep(300000);
    ssl = SSL_new(server_ctx);
    if (!ssl || (SSL_set_fd(ssl, client) != 1) || (SSL_accept(ssl) != 1))
      _exit(2);
    if (SSL_write(ssl, "x", 1) != 1)
      _exit(2);
  }

  // Keep the peer open so its closure cannot explain the client's result.
  for (int i = 0; (i < 100) && (getppid() == parent_pid); i++)
    usleep(100000);

  SSL_free(ssl);
  close(client);
  _exit(0);
}

static void run_opener(unsigned short port, int result_fd, int sig)
{
  struct Connection *conn = test_ssl_timeout_new_connection(port);
  if (!conn)
    _exit(2);

  if (sig)
  {
    if (raw_socket_open(conn) != 0)
      _exit(2);

    // Connect changes SIGINT handling. Install non-restarting handlers after it
    // and exercise the same negotiation function through STARTTLS.
    struct sigaction action = { 0 };
    action.sa_handler = record_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(sig, &action, NULL) != 0)
      _exit(2);
    SigInt = 0;
    ReceivedSignal = 0;
  }

  struct SslTimeoutResult result = { 0 };
  uint64_t start = mutt_date_now_ms();
  result.rc = sig ? mutt_ssl_starttls(conn) : mutt_socket_open(conn);
  result.elapsed = mutt_date_now_ms() - start;

  if (sig && (ReceivedSignal != sig))
    _exit(2);
  if (result.rc == 0)
  {
    // Do not close while the server is still finishing the TLS handshake.
    char byte = 0;
    if ((mutt_socket_read(conn, &byte, 1) != 1) || (byte != 'x'))
      _exit(2);
  }
  if (sig || (result.rc == 0))
    mutt_socket_close(conn);
  FREE(&conn);

  if (!test_ssl_timeout_write_result(result_fd, &result))
    _exit(2);
  close(result_fd);
  _exit(0);
}

static void check_handshake(SSL_CTX *server_ctx, int sig)
{
  int listener = -1;
  pid_t peer_pid = -1;
  pid_t opener_pid = -1;
  int result_fds[2] = { -1, -1 };
  unsigned short port = 0;

  listener = test_ssl_timeout_create_listener(&port);
  if (listener < 0)
    goto done;

  pid_t parent_pid = getpid();
  if (!TEST_CHECK(pipe(result_fds) == 0))
    goto done;
  opener_pid = fork();
  if (!TEST_CHECK(opener_pid >= 0))
    goto done;
  if (opener_pid == 0)
  {
    close(listener);
    close(result_fds[0]);
    run_opener(port, result_fds[1], sig);
  }
  close(result_fds[1]);
  result_fds[1] = -1;

  peer_pid = fork();
  if (!TEST_CHECK(peer_pid >= 0))
    goto done;
  if (peer_pid == 0)
  {
    close(result_fds[0]);
    run_peer(listener, server_ctx, sig, opener_pid, parent_pid);
  }
  close(listener);
  listener = -1;

  int opener_status = 0;
  if (!TEST_CHECK(test_ssl_timeout_wait(&opener_pid, &opener_status)))
    goto done;
  if (!TEST_CHECK(WIFEXITED(opener_status) && (WEXITSTATUS(opener_status) == 0)))
    goto done;

  struct SslTimeoutResult result = { 0 };
  if (!TEST_CHECK(test_ssl_timeout_read_result(result_fds[0], &result)))
    goto done;
  TEST_MSG("open rc=%d elapsed=%llu ms", result.rc, (unsigned long long) result.elapsed);
  if (server_ctx)
    TEST_CHECK(result.rc == 0);
  else
    TEST_CHECK(result.rc < 0);
  if (sig == SIGINT)
    TEST_CHECK(result.elapsed < 1800);
  else if (!server_ctx)
    TEST_CHECK(result.elapsed >= 1000);
  TEST_CHECK(result.elapsed <= 5000);

  // Prove that negotiation ended without waiting for peer closure.
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

/**
 * test_ssl_negotiate_timeout - Test that OpenSSL negotiation honors socket_timeout
 *
 * Regression test for #5005. The loopback peer accepts TCP but sends no TLS
 * response. Negotiation must not renew the socket timeout indefinitely.
 */
void test_ssl_negotiate_timeout(void)
{
  if (test_ssl_timeout_configure_client(NULL))
    check_handshake(NULL, 0);
}

/**
 * test_ssl_negotiate_retry - Distinguish interrupted negotiation from a timeout
 *
 * A delayed healthy peer must still succeed after a harmless signal, whereas a
 * silent peer must still time out. SIGINT must cancel rather than restart the
 * operation. Each client runs in a child so signal handlers stay isolated.
 */
void test_ssl_negotiate_retry(void)
{
  SSL_CTX *server_ctx = NULL;
  char cert_path[128] = { 0 };

  if (!test_ssl_timeout_create_server_context(&server_ctx, cert_path, sizeof(cert_path)))
    goto done;
  if (!test_ssl_timeout_configure_client(cert_path))
    goto done;

  TEST_CASE("delayed successful handshake");
  check_handshake(server_ctx, 0);
  TEST_CASE("successful handshake after SIGWINCH");
  check_handshake(server_ctx, SIGWINCH);
  TEST_CASE("silent peer after SIGWINCH");
  check_handshake(NULL, SIGWINCH);
  TEST_CASE("cancellation by SIGINT");
  check_handshake(NULL, SIGINT);

done:
  SSL_CTX_free(server_ctx);
  if (cert_path[0])
    unlink(cert_path);
}
#endif
