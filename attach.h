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

/* common protos for compose / attach menus */

#ifndef _ATTACH_H_
#define _ATTACH_H_ 1

#include "mutt_menu.h"

typedef struct attachptr
{
  BODY *content;
  int parent_type;
  char *tree;
  int level;
  int num;
  unsigned int unowned : 1;   /* don't unlink on detach */
} ATTACHPTR;

ATTACHPTR **mutt_gen_attach_list (BODY *, int, ATTACHPTR **, short *, short *,
                                  int, int);
void mutt_update_tree (ATTACHPTR **, short);
int mutt_view_attachment (FILE*, BODY *, int, HEADER *, ATTACHPTR **, short);

int mutt_tag_attach (MUTTMENU *menu, int n, int m);
int mutt_attach_display_loop (MUTTMENU *menu, int op, FILE *fp, HEADER *hdr,
			      BODY *cur, ATTACHPTR ***idxp, short *idxlen, short *idxmax,
			      int recv);


void mutt_save_attachment_list (FILE *fp, int tag, BODY *top, HEADER *hdr, MUTTMENU *menu);
void mutt_pipe_attachment_list (FILE *fp, int tag, BODY *top, int filter);
void mutt_print_attachment_list (FILE *fp, int tag, BODY *top);

void mutt_attach_bounce (FILE *, HEADER *, ATTACHPTR **, short, BODY *);
void mutt_attach_resend (FILE *, HEADER *, ATTACHPTR **, short, BODY *);
void mutt_attach_forward (FILE *, HEADER *, ATTACHPTR **, short, BODY *);
void mutt_attach_reply (FILE *, HEADER *, ATTACHPTR **, short, BODY *, int);

#endif /* _ATTACH_H_ */
