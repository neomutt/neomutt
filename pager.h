/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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

typedef struct
{
  CONTEXT *ctx;	/* current mailbox */
  HEADER *hdr;	/* current message */
  BODY *bdy;	/* current attachment */
  FILE *fp;	/* source stream */
} pager_t;

int mutt_do_pager (const char *, const char *, int, pager_t *);
int mutt_pager (const char *, const char *, int, pager_t *);
