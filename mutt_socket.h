/*
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#ifndef _MUTT_SOCKET_H_
#define _MUTT_SOCKET_H_ 1

#include "account.h"
#include "lib.h"

/* logging levels */
#define M_SOCK_LOG_CMD  2
#define M_SOCK_LOG_HDR  3
#define M_SOCK_LOG_FULL 4

typedef struct _connection
{
  ACCOUNT account;
  char inbuf[LONG_STRING];
  int bufpos;

  int fd;
  int available;
  void *data;

  struct _connection *next;

  void *sockdata;
  int (*read) (struct _connection *conn);
  int (*write) (struct _connection *conn, const char *buf);
  int (*open) (struct _connection *conn);
  int (*close) (struct _connection *conn);
} CONNECTION;

int mutt_socket_open (CONNECTION* conn);
int mutt_socket_close (CONNECTION* conn);
int mutt_socket_readchar (CONNECTION *conn, char *c);
#define mutt_socket_readln(A,B,C) mutt_socket_readln_d(A,B,C,M_SOCK_LOG_CMD)
int mutt_socket_readln_d (char *buf, size_t buflen, CONNECTION *conn, int dbg);
#define mutt_socket_write(A,B) mutt_socket_write_d(A,B,M_SOCK_LOG_CMD);
int mutt_socket_write_d (CONNECTION *conn, const char *buf, int dbg);

/* stupid hack for imap_logout_all */
CONNECTION* mutt_socket_head (void);
void mutt_socket_free (CONNECTION* conn);
CONNECTION* mutt_conn_find (const CONNECTION* start, const ACCOUNT* account);

int raw_socket_read (CONNECTION *conn);
int raw_socket_write (CONNECTION *conn, const char *buf);
int raw_socket_open (CONNECTION *conn);
int raw_socket_close (CONNECTION *conn);

#endif /* _MUTT_SOCKET_H_ */
