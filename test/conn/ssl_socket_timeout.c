/**
 * @file
 * Test OpenSSL socket timeouts and interrupted negotiation
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
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "test_common.h"

enum Operation
{
  HANDSHAKE,
  READ,
};

static volatile sig_atomic_t ReceivedSignal = 0;
static const int ClientSuccess = 42;

static void record_signal(int sig)
{
  const int saved_errno = errno;
  ReceivedSignal = sig;
  if (sig == SIGINT)
    SigInt = 1;
  errno = saved_errno;
}

// Generate a private, short-lived identity: no CLI, checked-in key, or expiry
// maintenance. The temporary certificate is trusted only by this test client.
static SSL_CTX *server_context(char *cert_path)
{
  EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  EVP_PKEY *key = NULL;
  X509 *cert = X509_new();
  SSL_CTX *ctx = NULL;
  FILE *fp = NULL;
  int fd = -1;
  bool ok = false;
  bool cert_created = false;

  if (!TEST_CHECK(key_ctx && cert && (EVP_PKEY_keygen_init(key_ctx) > 0) &&
                  (EVP_PKEY_CTX_set_rsa_keygen_bits(key_ctx, 2048) > 0) &&
                  (EVP_PKEY_keygen(key_ctx, &key) > 0)))
    goto done;

  if (!TEST_CHECK((X509_set_version(cert, 2) == 1) &&
                  (ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) == 1) &&
                  X509_gmtime_adj(X509_get_notBefore(cert), -60) &&
                  X509_gmtime_adj(X509_get_notAfter(cert), 86400) &&
                  (X509_set_pubkey(cert, key) == 1)))
    goto done;
  X509_NAME *name = X509_get_subject_name(cert);
  if (!TEST_CHECK(name &&
                  (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *) "localhost",
                                              -1, -1, 0) == 1) &&
                  (X509_set_issuer_name(cert, name) == 1) &&
                  (X509_sign(cert, key, EVP_sha256()) > 0)))
    goto done;

  ctx = SSL_CTX_new(SSLv23_server_method());
  if (!TEST_CHECK(ctx && (SSL_CTX_use_certificate(ctx, cert) == 1) &&
                  (SSL_CTX_use_PrivateKey(ctx, key) == 1)))
    goto done;
  fd = mkstemp(cert_path);
  if (!TEST_CHECK(fd >= 0))
    goto done;
  cert_created = true;
  fp = fdopen(fd, "w");
  if (!TEST_CHECK(fp != NULL))
    goto done;
  fd = -1;
  ok = TEST_CHECK(PEM_write_X509(fp, cert) == 1);
  ok &= TEST_CHECK(fclose(fp) == 0);
  fp = NULL;

done:
  if (fp)
    fclose(fp);
  if (fd >= 0)
    close(fd);
  X509_free(cert);
  EVP_PKEY_free(key);
  EVP_PKEY_CTX_free(key_ctx);
  if (!cert_created)
    cert_path[0] = '\0';
  if (!ok)
  {
    SSL_CTX_free(ctx);
    ctx = NULL;
  }
  return ctx;
}

static bool run_client(int fd, enum Operation operation, bool tls, int sig)
{
  bool ok = false;
  struct Connection *conn = NULL;
  struct sigaction action = { 0 };
  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_DFL;
  if (!TEST_CHECK(sigaction(SIGALRM, &action, NULL) == 0))
    goto done;
  sigaddset(&action.sa_mask, SIGALRM);
  if (sig)
    sigaddset(&action.sa_mask, sig);
  if (!TEST_CHECK(sigprocmask(SIG_UNBLOCK, &action.sa_mask, NULL) == 0))
    goto done;
  // A stuck operation kills only this child, including under --no-exec.
  alarm(7);
  if (sig)
  {
    sigemptyset(&action.sa_mask);
    action.sa_handler = record_signal;
    if (!TEST_CHECK(sigaction(sig, &action, NULL) == 0))
      goto done;
  }
  SigInt = 0;
  ReceivedSignal = 0;

  conn = mutt_socket_new(MUTT_CONNECTION_SIMPLE);
  if (!TEST_CHECK(conn != NULL))
    goto done;
  conn->fd = fd;
  mutt_str_copy(conn->account.host, "localhost", sizeof(conn->account.host));
  uint64_t start = mutt_date_now_ms();
  int rc = mutt_ssl_starttls(conn);
  uint64_t elapsed = mutt_date_now_ms() - start;
  if (tls)
  {
    if (!TEST_CHECK(rc == 0))
      goto done;
    // The acknowledgment prevents closing before the peer finishes TLS 1.3.
    char byte = 0;
    if (!TEST_CHECK((mutt_socket_read(conn, &byte, 1) == 1) && (byte == 'x')))
      goto done;
    if (operation == READ)
    {
      start = mutt_date_now_ms();
      rc = mutt_socket_read(conn, &byte, 1);
      elapsed = mutt_date_now_ms() - start;
    }
  }

  ok = TEST_CHECK((tls && (operation == HANDSHAKE)) ? (rc == 0) : (rc < 0));
  TEST_MSG("rc=%d elapsed=%llu ms", rc, (unsigned long long) elapsed);
  if (sig)
    ok &= TEST_CHECK(ReceivedSignal == sig);
  if (sig == SIGINT)
    ok &= TEST_CHECK(elapsed < 1800);
  else if (!tls || (operation == READ))
    ok &= TEST_CHECK(elapsed >= 1000);
  ok &= TEST_CHECK(elapsed <= 5000);
  TEST_MSG("elapsed=%llu ms", (unsigned long long) elapsed);

done:
  if (conn)
    mutt_socket_close(conn);
  FREE(&conn);
  return ok;
}

// The test process serves TLS while one isolated child exercises NeoMutt.
// Keep the peer socket open until the child exits: EOF must not end a stall.
static void check_socket(enum Operation operation, bool tls, int sig)
{
  SSL_CTX *ctx = NULL;
  SSL *peer = NULL;
  int sockets[2] = { -1, -1 };
  pid_t child = -1;
  char cert_path[] = "/tmp/neomutt-test-cert-XXXXXX";
  struct sigaction ignore = { 0 };
  struct sigaction old_pipe = { 0 };
  bool restore_pipe = false;

  if (tls && !(ctx = server_context(cert_path)))
    goto done;
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(cs_subset_str_native_set(NeoMutt->sub, "socket_timeout", 2, NULL)),
                         CSR_SUCCESS) ||
      !TEST_CHECK_NUM_EQ(CSR_RESULT(cs_subset_str_native_set(NeoMutt->sub, "ssl_use_system_certs",
                                                             false, NULL)),
                         CSR_SUCCESS) ||
      !TEST_CHECK_NUM_EQ(CSR_RESULT(cs_subset_str_string_set(NeoMutt->sub, "certificate_file",
                                                             tls ? cert_path : NULL, NULL)),
                         CSR_SUCCESS))
    goto done;

  if (!TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0))
    goto done;
  // Supply the same blocking-socket timeout used by a TCP connection, without
  // testing DNS, port allocation, or connect(). The TLS operations are real.
  struct timeval timeout = { .tv_sec = 2 };
  for (int i = 0; i < 2; i++)
  {
    if (!TEST_CHECK((setsockopt(sockets[i], SOL_SOCKET, SO_RCVTIMEO, &timeout,
                                sizeof(timeout)) == 0) &&
                    (setsockopt(sockets[i], SOL_SOCKET, SO_SNDTIMEO, &timeout,
                                sizeof(timeout)) == 0)))
      goto done;
  }
  sigemptyset(&ignore.sa_mask);
  ignore.sa_handler = SIG_IGN;
  if (!TEST_CHECK(sigaction(SIGPIPE, &ignore, &old_pipe) == 0))
    goto done;
  restore_pipe = true;
  fflush(NULL);
  child = fork();
  if (!TEST_CHECK(child >= 0))
    goto done;
  if (child == 0)
  {
    close(sockets[1]);
    bool ok = run_client(sockets[0], operation, tls, sig);
    fflush(NULL);
    _exit(ok ? ClientSuccess : 1);
  }
  close(sockets[0]);
  sockets[0] = -1;
  if (tls || sig)
  {
    char byte = 0;
    if (!TEST_CHECK(recv(sockets[1], &byte, 1, MSG_PEEK) == 1))
      goto done;
    usleep(300000);
    if (sig && !TEST_CHECK(kill(child, sig) == 0))
      goto done;
  }
  if (tls)
  {
    // Leave a gap after the signal, when the client's socket is not readable.
    usleep(300000);
    peer = SSL_new(ctx);
    if (!TEST_CHECK(peer && (SSL_set_fd(peer, sockets[1]) == 1) &&
                    (SSL_accept(peer) == 1) && (SSL_write(peer, "x", 1) == 1)))
      goto done;
  }

done:
  if (child > 0)
  {
    int status = 0;
    pid_t rc;
    do
      rc = waitpid(child, &status, 0);
    while ((rc < 0) && (errno == EINTR));
    // A distinct success status also rejects an early exit(0) in library code.
    TEST_CHECK((rc == child) && WIFEXITED(status) && (WEXITSTATUS(status) == ClientSuccess));
    TEST_MSG("client wait status=%d (SIGALRM indicates watchdog expiry)", status);
  }
  SSL_free(peer);
  SSL_CTX_free(ctx);
  for (int i = 0; i < 2; i++)
    if (sockets[i] >= 0)
      close(sockets[i]);
  if (restore_pipe)
    sigaction(SIGPIPE, &old_pipe, NULL);
  if (tls && cert_path[0])
    unlink(cert_path);
}

/** test_ssl_negotiate_timeout - A silent peer must not renew the timeout (#5005) */
void test_ssl_negotiate_timeout(void)
{
  check_socket(HANDSHAKE, false, 0);
}

/** test_ssl_negotiate_retry - Distinguish interruption, cancellation, and timeout */
void test_ssl_negotiate_retry(void)
{
  TEST_CASE("delayed successful handshake");
  check_socket(HANDSHAKE, true, 0);
  TEST_CASE("successful handshake after SIGWINCH");
  check_socket(HANDSHAKE, true, SIGWINCH);
  TEST_CASE("silent peer after SIGWINCH");
  check_socket(HANDSHAKE, false, SIGWINCH);
  TEST_CASE("cancellation by SIGINT");
  check_socket(HANDSHAKE, false, SIGINT);
}

/** test_ssl_socket_read_timeout - A silent TLS peer must not renew the timeout (#4995) */
void test_ssl_socket_read_timeout(void)
{
  check_socket(READ, true, 0);
}
#endif
