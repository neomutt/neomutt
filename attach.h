/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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

/* common protos for compose / attach menus */

int mutt_tag_attach (MUTTMENU *menu, int n);

void mutt_save_attachment_list (FILE *fp, int tag, BODY *top, HEADER *hdr);
void mutt_pipe_attachment_list (FILE *fp, int tag, BODY *top, int filter);
void mutt_print_attachment_list (FILE *fp, int tag, BODY *top);
void mutt_attach_display_loop (MUTTMENU *menu, int op, FILE *fp, HEADER *hdr,
			       BODY *cur, ATTACHPTR **idx, short *idxlen, short *idxmax);

void mutt_attach_bounce (FILE *, HEADER *, ATTACHPTR **, short, BODY *);
void mutt_attach_resend (FILE *, HEADER *, ATTACHPTR **, short, BODY *);
void mutt_attach_forward (FILE *, HEADER *, ATTACHPTR **, short, BODY *);
void mutt_attach_reply (FILE *, HEADER *, ATTACHPTR **, short, BODY *, int);
