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

#ifndef _IMAP_SOCKET_H_
#define _IMAP_SOCKET_H_ 1

typedef struct _connection
{
  IMAP_MBOX mx;
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

  /* status bits */
  
  int up : 1; /* is the connection up? */
} CONNECTION;

int mutt_socket_open (CONNECTION* conn);
int mutt_socket_close (CONNECTION* conn);
int mutt_socket_readchar (CONNECTION *conn, char *c);
#define mutt_socket_readln(A,B,C) mutt_socket_readln_d(A,B,C,IMAP_LOG_CMD)
int mutt_socket_readln_d (char *buf, size_t buflen, CONNECTION *conn, int dbg);
#define mutt_socket_write(A,B) mutt_socket_write_d(A,B,IMAP_LOG_CMD);
int mutt_socket_write_d (CONNECTION *conn, const char *buf, int dbg);

CONNECTION* mutt_socket_find (const IMAP_MBOX* mx, int newconn);

int raw_socket_read (CONNECTION *conn);
int raw_socket_write (CONNECTION *conn, const char *buf);
int raw_socket_open (CONNECTION *conn);
int raw_socket_close (CONNECTION *conn);

#endif /* _IMAP_SOCKET_H_ */
