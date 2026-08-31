/**
 * @file
 * Shared fixture for OpenSSL socket-timeout tests
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
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "ssl_timeout_common.h"
#include "test_common.h" // IWYU pragma: keep

/**
 * test_ssl_timeout_create_server_context - Create a private TLS identity
 * @param[out] server_ctx    OpenSSL server context
 * @param[out] cert_path     Path to the temporary certificate
 * @param[in]  cert_path_len Size of the certificate path buffer
 * @retval true  Success
 * @retval false Error
 *
 * Generate an ephemeral RSA key and self-signed X.509 certificate at runtime.
 * Writing the certificate to a temporary file lets NeoMutt trust the loopback
 * server without relying on an external certificate or network service.
 */
bool test_ssl_timeout_create_server_context(SSL_CTX **server_ctx, char *cert_path, size_t cert_path_len)
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
 * test_ssl_timeout_create_listener - Open a private loopback listener
 * @param[out] port Assigned port
 * @retval num Listener file descriptor
 * @retval -1  Error
 *
 * Binding port zero asks the kernel to choose an available port, avoiding a
 * fixed-port dependency and conflicts with concurrent test runs.
 */
int test_ssl_timeout_create_listener(unsigned short *port)
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

/**
 * test_ssl_timeout_configure_client - Configure a private TLS client
 * @param[in] cert_path Trusted fixture certificate, or NULL before negotiation
 * @retval true  Success
 * @retval false Error
 */
bool test_ssl_timeout_configure_client(const char *cert_path)
{
  int rc = cs_subset_str_native_set(NeoMutt->sub, "socket_timeout", 2, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;

  rc = cs_subset_str_native_set(NeoMutt->sub, "ssl_use_system_certs", false, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;

  if (cert_path)
  {
    rc = cs_subset_str_native_set(NeoMutt->sub, "ssl_verify_host", false, NULL);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
      return false;

    rc = cs_subset_str_string_set(NeoMutt->sub, "certificate_file", cert_path, NULL);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
      return false;
  }

#ifdef HAVE_GETADDRINFO
  rc = cs_subset_str_native_set(NeoMutt->sub, "use_ipv6", false, NULL);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    return false;
#endif

  return true;
}

/**
 * test_ssl_timeout_new_connection - Create a TLS connection to the loopback fixture
 * @param[in] port Fixture port
 * @retval ptr New connection
 * @retval NULL Error
 */
struct Connection *test_ssl_timeout_new_connection(unsigned short port)
{
  struct Connection *conn = mutt_socket_new(MUTT_CONNECTION_SSL);
  if (!conn)
    return NULL;

  mutt_str_copy(conn->account.host, "127.0.0.1", sizeof(conn->account.host));
  conn->account.port = port;
  conn->account.flags = MUTT_ACCT_PORT | MUTT_ACCT_SSL;
  return conn;
}

/**
 * test_ssl_timeout_run_idle_server - Complete TLS, then perform no application I/O
 * @param[in] listener   Listening socket
 * @param[in] server_ctx OpenSSL server context
 * @param[in] parent_pid Parent test process
 *
 * After the handshake, keep the connection open without sending or consuming
 * application data.
 */
void test_ssl_timeout_run_idle_server(int listener, SSL_CTX *server_ctx, pid_t parent_pid)
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

  for (int i = 0; (i < 100) && (getppid() == parent_pid); i++)
    usleep(100000);

  SSL_free(ssl);
  SSL_CTX_free(server_ctx);
  close(client);
  _exit(0);
}

/**
 * test_ssl_timeout_write_result - Send a child operation result through a pipe
 * @param[in] fd     Pipe file descriptor
 * @param[in] result Result to send
 * @retval true  Success
 * @retval false Error
 */
bool test_ssl_timeout_write_result(int fd, const struct SslTimeoutResult *result)
{
  const char *data = (const char *) result;
  size_t remaining = sizeof(*result);

  while (remaining > 0)
  {
    ssize_t written = write(fd, data, remaining);
    if ((written < 0) && (errno == EINTR))
      continue;
    if (written <= 0)
      return false;
    data += written;
    remaining -= written;
  }

  return true;
}

/**
 * test_ssl_timeout_read_result - Receive a child operation result from a pipe
 * @param[in]  fd     Pipe file descriptor
 * @param[out] result Received result
 * @retval true  Success
 * @retval false Error
 */
bool test_ssl_timeout_read_result(int fd, struct SslTimeoutResult *result)
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
 * test_ssl_timeout_wait - Wait up to seven seconds for an operation child
 * @param[in,out] child_pid Child process, set to -1 after it is reaped
 * @param[out]    status    Child wait status
 * @retval true  Child finished
 * @retval false Watchdog expired or wait failed
 */
bool test_ssl_timeout_wait(pid_t *child_pid, int *status)
{
  for (int i = 0; i < 70; i++)
  {
    pid_t state = waitpid(*child_pid, status, WNOHANG);
    if (state == *child_pid)
    {
      *child_pid = -1;
      return true;
    }
    if (state < 0)
    {
      if (errno == EINTR)
        continue;
      return false;
    }
    usleep(100000);
  }

  return false;
}

/**
 * test_ssl_timeout_stop_child - Terminate and reap a fixture child
 * @param[in,out] child_pid Child process, set to -1 after cleanup
 */
void test_ssl_timeout_stop_child(pid_t *child_pid)
{
  if (!child_pid || (*child_pid <= 0))
    return;

  kill(*child_pid, SIGTERM);
  while ((waitpid(*child_pid, NULL, 0) < 0) && (errno == EINTR))
  {
  }
  *child_pid = -1;
}
#endif
