/* $Id$ */
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

/* flags to _mutt_copy_message */
#define M_CM_NOHEADER	1	/* don't copy the message header */
#define M_CM_PREFIX	(1<<1)	/* quote the message */
#define M_CM_DECODE	(1<<2)	/* decode the message body into text/plain */
#define M_CM_DISPLAY	(1<<3)	/* output is displayed to the user */
#define M_CM_UPDATE	(1<<4)  /* update structs on sync */



#ifdef _PGPPATH
#define M_CM_DECODE_PGP	(1<<5)	/* used for decoding PGP messages */
#define M_CM_VERIFY	(1<<6)	/* do signature verification */
#endif



int mutt_copy_hdr (FILE *, FILE *, long, long, int, const char *);

int mutt_copy_header (FILE *, HEADER *, FILE *, int, const char *);

int _mutt_copy_message (FILE *fpout,
			FILE *fpin,
			HEADER *hdr,
			BODY *body,
			int flags,
			int chflags);

int mutt_copy_message (FILE *fpout,
		       CONTEXT *src,
		       HEADER *hdr,
		       int flags,
		       int chflags);

int _mutt_append_message (CONTEXT *dest,
			  FILE *fpin,
			  CONTEXT *src,
			  HEADER *hdr,
			  BODY *body,
			  int flags,
			  int chflags);

int mutt_append_message (CONTEXT *dest,
			 CONTEXT *src,
			 HEADER *hdr,
			 int cmflags,
			 int chflags);
