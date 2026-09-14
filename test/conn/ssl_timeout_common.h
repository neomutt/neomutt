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

#ifndef MUTT_TEST_CONN_SSL_TIMEOUT_COMMON_H
#define MUTT_TEST_CONN_SSL_TIMEOUT_COMMON_H

#include <openssl/ssl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

struct Connection;

/** Result returned by an isolated socket operation. */
struct SslTimeoutResult
{
  int rc;
  uint64_t elapsed;
};

bool test_ssl_timeout_configure_client(const char *cert_path);
bool test_ssl_timeout_create_server_context(SSL_CTX **server_ctx, char *cert_path,
                                            size_t cert_path_len);
int test_ssl_timeout_create_listener(unsigned short *port);
struct Connection *test_ssl_timeout_new_connection(unsigned short port);
void test_ssl_timeout_run_idle_server(int listener, SSL_CTX *server_ctx, pid_t parent_pid);
bool test_ssl_timeout_write_result(int fd, const struct SslTimeoutResult *result);
bool test_ssl_timeout_read_result(int fd, struct SslTimeoutResult *result);
bool test_ssl_timeout_wait(pid_t *child_pid, int *status);
void test_ssl_timeout_stop_child(pid_t *child_pid);

#endif /* MUTT_TEST_CONN_SSL_TIMEOUT_COMMON_H */
