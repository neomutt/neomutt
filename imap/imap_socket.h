/*
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#ifndef _IMAP_SOCKET_H_
#define _IMAP_SOCKET_H_ 1

typedef struct _connection
{
  char *server;
  int port;
  int uses;
  int fd;
  char inbuf[LONG_STRING];
  int bufpos;
  int available;
  void *data;
  struct _connection *next;
} CONNECTION;

int mutt_socket_readchar (CONNECTION *conn, char *c);
int mutt_socket_read_line (char *buf, size_t buflen, CONNECTION *conn);
int mutt_socket_read_line_d (char *buf, size_t buflen, CONNECTION *conn);
int mutt_socket_write (CONNECTION *conn, const char *buf);
CONNECTION *mutt_socket_select_connection (char *host, int port, int flags);
int mutt_socket_open_connection (CONNECTION *conn);


#endif /* _IMAP_SOCKET_H_ */
