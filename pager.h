/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#include "attach.h"

/* dynamic internal flags */
#define M_SHOWFLAT	(1<<0)
#define M_SHOWCOLOR	(1<<1)
#define M_HIDE		(1<<2)
#define M_SEARCH	(1<<3)
#define M_TYPES		(1<<4)
#define M_SHOW		(M_SHOWCOLOR | M_SHOWFLAT)

/* exported flags for mutt_(do_)?pager */
#define M_PAGER_NSKIP		(1<<5)	/* preserve whitespace with smartwrap */
#define M_PAGER_MARKER		(1<<6)	/* use markers if option is set */
#define M_PAGER_RETWINCH	(1<<7)	/* need reformatting on SIGWINCH */
#define M_PAGER_MESSAGE		(M_SHOWCOLOR | M_PAGER_MARKER)
#define M_PAGER_ATTACHMENT	(1<<8)
#define M_PAGER_NOWRAP		(1<<9)	/* format for term width, ignore $wrap */

#define M_DISPLAYFLAGS	(M_SHOW | M_PAGER_NSKIP | M_PAGER_MARKER)

typedef struct
{
  CONTEXT *ctx;	/* current mailbox */
  HEADER *hdr;	/* current message */
  BODY *bdy;	/* current attachment */
  FILE *fp;	/* source stream */
  ATTACHPTR **idx;	/* attachment information */
  short idxlen;
} pager_t;

int mutt_do_pager (const char *, const char *, int, pager_t *);
int mutt_pager (const char *, const char *, int, pager_t *);
