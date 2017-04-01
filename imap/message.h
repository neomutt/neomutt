/**
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000,2005 Brendan Cully <brendan@kublai.com>
 *
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

/* message.c data structures */

#ifndef _MUTT_IMAP_MESSAGE_H
#define _MUTT_IMAP_MESSAGE_H 1

/* -- data structures -- */
/* IMAP-specific header data, stored as HEADER->data */
typedef struct imap_header_data
{
  /* server-side flags */
  bool read : 1;
  bool old : 1;
  bool deleted : 1;
  bool flagged : 1;
  bool replied : 1;
  bool changed : 1;

  bool parsed : 1;

  unsigned int uid;	/* 32-bit Message UID */
  LIST *keywords;
} IMAP_HEADER_DATA;

typedef struct
{
  unsigned int sid;

  IMAP_HEADER_DATA* data;

  time_t received;
  long content_length;
} IMAP_HEADER;

/* -- macros -- */
#define HEADER_DATA(ph) ((IMAP_HEADER_DATA*) ((ph)->data))

#endif /* _MUTT_IMAP_MESSAGE_H */
