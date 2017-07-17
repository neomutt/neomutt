/**
 * @file
 * Low-level socket handling
 *
 * @authors
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2005 Brendan Cully <brendan@kublai.com>
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

#ifndef _MUTT_SOCKET_H
#define _MUTT_SOCKET_H

#include <stddef.h>
#include "account.h"
#include "lib.h"

/* logging levels */
#define MUTT_SOCK_LOG_CMD  2
#define MUTT_SOCK_LOG_HDR  3
#define MUTT_SOCK_LOG_FULL 4

/**
 * struct Connection - An open network connection (socket)
 */
struct Connection
{
  struct Account account;
  unsigned int ssf; /**< security strength factor, in bits */
  void *data;

  char inbuf[LONG_STRING];
  int bufpos;

  int fd;
  int available;

  struct Connection *next;

  void *sockdata;
  int (*conn_read)(struct Connection *conn, char *buf, size_t len);
  int (*conn_write)(struct Connection *conn, const char *buf, size_t count);
  int (*conn_open)(struct Connection *conn);
  int (*conn_close)(struct Connection *conn);
  int (*conn_poll)(struct Connection *conn);
};

int mutt_socket_open(struct Connection *conn);
int mutt_socket_close(struct Connection *conn);
int mutt_socket_poll(struct Connection *conn);
int mutt_socket_readchar(struct Connection *conn, char *c);
#define mutt_socket_readln(A, B, C) mutt_socket_readln_d(A, B, C, MUTT_SOCK_LOG_CMD)
int mutt_socket_readln_d(char *buf, size_t buflen, struct Connection *conn, int dbg);
#define mutt_socket_write(A, B) mutt_socket_write_d(A, B, -1, MUTT_SOCK_LOG_CMD)
#define mutt_socket_write_n(A, B, C) mutt_socket_write_d(A, B, C, MUTT_SOCK_LOG_CMD)
int mutt_socket_write_d(struct Connection *conn, const char *buf, int len, int dbg);

/* stupid hack for imap_logout_all */
struct Connection *mutt_socket_head(void);
void mutt_socket_free(struct Connection *conn);
struct Connection *mutt_conn_find(const struct Connection *start, const struct Account *account);

int raw_socket_read(struct Connection *conn, char *buf, size_t len);
int raw_socket_write(struct Connection *conn, const char *buf, size_t count);
int raw_socket_open(struct Connection *conn);
int raw_socket_close(struct Connection *conn);
int raw_socket_poll(struct Connection *conn);

#endif /* _MUTT_SOCKET_H */
