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
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "test_common.h" // IWYU pragma: keep

/**
 * create_server_context - Create a self-contained TLS identity for the fixture
 * @param[out] server_ctx   OpenSSL server context
 * @param[out] cert_path    Path to the temporary certificate
 * @param[in]  cert_path_len Size of the certificate path buffer
 * @retval true  Success
 * @retval false Error
 *
 * Generate an ephemeral RSA key and self-signed X.509 certificate at runtime.
 * Writing the certificate to a temporary file lets NeoMutt trust the loopback
 * server without relying on an external certificate or network service.
 */
static bool create_server_context(SSL_CTX **server_ctx, char *cert_path, size_t cert_path_len)
{
  EVP_PKEY_CTX *key_ctx = NULL;
  EVP_PKEY *key = NULL;
  X509 *cert = NULL;
  SSL_CTX *ctx = NULL;
  FILE *fp = NULL;
  int fd = -1;
  bool result = false;

  key_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  if (!TEST_CHECK(key_ctx != NULL))
    goto done;
  if (!TEST_CHECK(EVP_PKEY_keygen_init(key_ctx) > 0))
    goto done;
  if (!TEST_CHECK(EVP_PKEY_CTX_set_rsa_keygen_bits(key_ctx, 2048) > 0))
    goto done;
  if (!TEST_CHECK(EVP_PKEY_keygen(key_ctx, &key) > 0))
    goto done;

  cert = X509_new();
  if (!TEST_CHECK(cert != NULL))
    goto done;
  if (!TEST_CHECK(X509_set_version(cert, 2) == 1))
    goto done;
  if (!TEST_CHECK(ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) == 1))
    goto done;
  if (!TEST_CHECK(X509_gmtime_adj(X509_get_notBefore(cert), -60) != NULL))
    goto done;
  if (!TEST_CHECK(X509_gmtime_adj(X509_get_notAfter(cert), 86400) != NULL))
    goto done;
  if (!TEST_CHECK(X509_set_pubkey(cert, key) == 1))
    goto done;

  X509_NAME *name = X509_get_subject_name(cert);
  if (!TEST_CHECK(name != NULL))
    goto done;
  if (!TEST_CHECK(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                             (const unsigned char *) "localhost",
                                             -1, -1, 0) == 1))
  {
    goto done;
  }
  if (!TEST_CHECK(X509_set_issuer_name(cert, name) == 1))
    goto done;
  if (!TEST_CHECK(X509_sign(cert, key, EVP_sha256()) > 0))
    goto done;

  ctx = SSL_CTX_new(SSLv23_server_method());
  if (!TEST_CHECK(ctx != NULL))
    goto done;
  if (!TEST_CHECK(SSL_CTX_use_certificate(ctx, cert) == 1))
    goto done;
  if (!TEST_CHECK(SSL_CTX_use_PrivateKey(ctx, key) == 1))
    goto done;
  if (!TEST_CHECK(SSL_CTX_check_private_key(ctx) == 1))
    goto done;

  snprintf(cert_path, cert_path_len, "/tmp/neomutt-test-cert-XXXXXX");
  fd = mkstemp(cert_path);
  if (!TEST_CHECK(fd >= 0))
    goto done;
  fp = fdopen(fd, "w");
  if (!TEST_CHECK(fp != NULL))
    goto done;
  fd = -1;
  if (!TEST_CHECK(PEM_write_X509(fp, cert) == 1))
    goto done;
  if (!TEST_CHECK(fclose(fp) == 0))
  {
    fp = NULL;
    goto done;
  }
  fp = NULL;

  *server_ctx = ctx;
  ctx = NULL;
  result = true;

done:
  if (fp)
    fclose(fp);
  if (fd >= 0)
    close(fd);
  if (!result && cert_path[0])
    unlink(cert_path);
  SSL_CTX_free(ctx);
  X509_free(cert);
  EVP_PKEY_free(key);
  EVP_PKEY_CTX_free(key_ctx);
  return result;
}

/**
 * create_listener - Open an IPv4 loopback listener on an ephemeral port
 * @param[out] port Assigned port
 * @retval num Listener file descriptor
 * @retval -1  Error
 *
 * Binding port zero asks the kernel to choose an available port, avoiding a
 * fixed-port dependency and conflicts with concurrent test runs.
 */
static int create_listener(unsigned short *port)
{
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  if (!TEST_CHECK(listener >= 0))
    return -1;

  struct sockaddr_in address = { 0 };
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;

  if (!TEST_CHECK(bind(listener, (struct sockaddr *) &address, sizeof(address)) == 0))
    goto fail;
  if (!TEST_CHECK(listen(listener, 1) == 0))
    goto fail;

  socklen_t address_len = sizeof(address);
  if (!TEST_CHECK(getsockname(listener, (struct sockaddr *) &address, &address_len) == 0))
    goto fail;

  *port = ntohs(address.sin_port);
  return listener;

fail:
  close(listener);
  return -1;
}

static void run_server(int listener, SSL_CTX *server_ctx, pid_t parent_pid)
{
  int client = accept(listener, NULL, NULL);
  close(listener);
  if (client < 0)
    _exit(2);

  SSL *ssl = SSL_new(server_ctx);
  if (!ssl)
    _exit(2);
  SSL_set_fd(ssl, client);
  if (SSL_accept(ssl) != 1)
    _exit(2);

  // Complete TLS, then withhold application data while the parent reads.
  for (int i = 0; (i < 100) && (getppid() == parent_pid); i++)
    usleep(100000);

  SSL_free(ssl);
  SSL_CTX_free(server_ctx);
  close(client);
  _exit(0);
}

/**
 * configure_client - Configure NeoMutt for the private TLS fixture
 * @param[in] cert_path Path to the fixture's temporary certificate
 * @retval true  Success
 * @retval false Error
 *
 * Use a two-second socket timeout and trust only the generated certificate.
 * Host verification is disabled because the fixture connects to its private
 * loopback address rather than the certificate's hostname.
 */
static bool configure_client(const char *cert_path)
{
  int rc = cs_subset_str_native_set(NeoMutt->sub, "socket_timeout", 2, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;

  rc = cs_subset_str_native_set(NeoMutt->sub, "ssl_use_system_certs", false, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;

  rc = cs_subset_str_native_set(NeoMutt->sub, "ssl_verify_host", false, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;

  rc = cs_subset_str_string_set(NeoMutt->sub, "certificate_file", cert_path, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;

#ifdef HAVE_GETADDRINFO
  rc = cs_subset_str_native_set(NeoMutt->sub, "use_ipv6", false, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;
#endif

  return true;
}

struct ReadResult
{
  int rc;
  uint64_t elapsed;
};

static void run_reader(struct Connection *conn, int result_fd)
{
  struct ReadResult result = { 0 };
  char buf[16] = { 0 };
  uint64_t start = mutt_date_now_ms();
  result.rc = mutt_socket_read(conn, buf, sizeof(buf));
  result.elapsed = mutt_date_now_ms() - start;

  const char *data = (const char *) &result;
  size_t remaining = sizeof(result);
  while (remaining > 0)
  {
    ssize_t written = write(result_fd, data, remaining);
    if ((written < 0) && (errno == EINTR))
      continue;
    if (written <= 0)
      _exit(2);
    data += written;
    remaining -= written;
  }

  close(result_fd);
  _exit(0);
}

static bool read_result(int fd, struct ReadResult *result)
{
  char *data = (char *) result;
  size_t remaining = sizeof(*result);
  while (remaining > 0)
  {
    ssize_t received = read(fd, data, remaining);
    if ((received < 0) && (errno == EINTR))
      continue;
    if (received <= 0)
      return false;
    data += received;
    remaining -= received;
  }

  return true;
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

  conn = mutt_socket_new(MUTT_CONNECTION_SSL);
  if (!TEST_CHECK(conn != NULL))
    goto done;
  if (!create_server_context(&server_ctx, cert_path, sizeof(cert_path)))
    goto done;
  if (!configure_client(cert_path))
    goto done;

  listener = create_listener(&port);
  if (listener < 0)
    goto done;

  pid_t parent_pid = getpid();
  server_pid = fork();
  if (!TEST_CHECK(server_pid >= 0))
    goto done;
  if (server_pid == 0)
    run_server(listener, server_ctx, parent_pid);

  close(listener);
  listener = -1;
  SSL_CTX_free(server_ctx);
  server_ctx = NULL;

  mutt_str_copy(conn->account.host, "127.0.0.1", sizeof(conn->account.host));
  conn->account.port = port;
  conn->account.flags = MUTT_ACCT_PORT | MUTT_ACCT_SSL;

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
  bool reader_finished = false;
  // Allow scheduling margin around socket_timeout=2, but independently bound
  // the operation at seven seconds if the read retry loop regresses.
  for (int i = 0; i < 70; i++)
  {
    pid_t state = waitpid(reader_pid, &reader_status, WNOHANG);
    if (state == reader_pid)
    {
      reader_finished = true;
      reader_pid = -1;
      break;
    }
    if ((state < 0) && (errno != EINTR))
      break;
    usleep(100000);
  }

  if (!TEST_CHECK(reader_finished))
    goto done;
  if (!TEST_CHECK(WIFEXITED(reader_status) && (WEXITSTATUS(reader_status) == 0)))
    goto done;

  struct ReadResult result = { 0 };
  if (!TEST_CHECK(read_result(result_fds[0], &result)))
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
  if (reader_pid > 0)
  {
    kill(reader_pid, SIGTERM);
    while ((waitpid(reader_pid, NULL, 0) < 0) && (errno == EINTR))
    {
    }
  }
  if (result_fds[0] >= 0)
    close(result_fds[0]);
  if (result_fds[1] >= 0)
    close(result_fds[1]);
  if (connection_open)
    mutt_socket_close(conn);
  FREE(&conn);
  if (server_pid > 0)
  {
    kill(server_pid, SIGTERM);
    while ((waitpid(server_pid, NULL, 0) < 0) && (errno == EINTR))
    {
    }
  }
  if (listener >= 0)
    close(listener);
  SSL_CTX_free(server_ctx);
  if (cert_path[0])
    unlink(cert_path);
}
#endif
