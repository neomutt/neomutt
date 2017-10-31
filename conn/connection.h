/**
 * @file
 * An open network connection (socket)
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

#ifndef _CONN_CONNECTION_H
#define _CONN_CONNECTION_H

#include <stdio.h>
#include <time.h>
#include "lib/queue.h"
#include "account.h"

#define LONG_STRING 1024

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

  TAILQ_ENTRY(Connection) entries;

  void *sockdata;
  int (*conn_read)(struct Connection *conn, char *buf, size_t len);
  int (*conn_write)(struct Connection *conn, const char *buf, size_t count);
  int (*conn_open)(struct Connection *conn);
  int (*conn_close)(struct Connection *conn);
  int (*conn_poll)(struct Connection *conn, time_t wait_secs);
};

#endif /* _CONN_CONNECTION_H */
