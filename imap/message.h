/*
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
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

/* message.c data structures */

#ifndef MESSAGE_H
#define MESSAGE_H 1

/* Data from the FLAGS response */
typedef struct imap_flags
{
  unsigned int read : 1;
  unsigned int old : 1;
  unsigned int deleted : 1;
  unsigned int flagged : 1;
  unsigned int replied : 1;
  unsigned int changed : 1;

  LIST* server_flags;	/* flags mutt doesn't use, but the server does */
} IMAP_FLAGS;

/* Linked list to hold header information while downloading message
 * headers */
typedef struct imap_header_info
{
  IMAP_FLAGS flags;
  unsigned int number;

  time_t received;
  long content_length;
  struct imap_header_info *next;
} IMAP_HEADER_INFO;

#endif
