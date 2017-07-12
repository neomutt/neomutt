/**
 * @file
 * Handling of email attachments
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 *
 * @copyright
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

/* common protos for compose / attach menus */

#ifndef _MUTT_ATTACH_H
#define _MUTT_ATTACH_H

#include <stdbool.h>
#include <stdio.h>

struct Menu;
struct Header;
struct Body;

/**
 * struct AttachPtr - An email to which things will be attached
 */
struct AttachPtr
{
  struct Body *content;
  int parent_type;
  char *tree;
  int level;
  int num;
  bool unowned : 1; /**< don't unlink on detach */
};

struct AttachPtr **mutt_gen_attach_list(struct Body *m, int parent_type, struct AttachPtr **idx,
                                 short *idxlen, short *idxmax, int level, int compose);
void mutt_update_tree(struct AttachPtr **idx, short idxlen);
int mutt_view_attachment(FILE *fp, struct Body *a, int flag, struct Header *hdr,
                         struct AttachPtr **idx, short idxlen);

int mutt_tag_attach(struct Menu *menu, int n, int m);
int mutt_attach_display_loop(struct Menu *menu, int op, FILE *fp, struct Header *hdr, struct Body *cur,
                             struct AttachPtr ***idxp, short *idxlen, short *idxmax, int recv);

void mutt_save_attachment_list(FILE *fp, int tag, struct Body *top, struct Header *hdr, struct Menu *menu);
void mutt_pipe_attachment_list(FILE *fp, int tag, struct Body *top, int filter);
void mutt_print_attachment_list(FILE *fp, int tag, struct Body *top);

void mutt_attach_bounce(FILE *fp, struct Header *hdr, struct AttachPtr **idx, short idxlen, struct Body *cur);
void mutt_attach_resend(FILE *fp, struct Header *hdr, struct AttachPtr **idx, short idxlen, struct Body *cur);
void mutt_attach_forward(FILE *fp, struct Header *hdr, struct AttachPtr **idx, short idxlen,
                         struct Body *cur, int flags);
void mutt_attach_reply(FILE *fp, struct Header *hdr, struct AttachPtr **idx, short idxlen,
                       struct Body *cur, int flags);

#endif /* _MUTT_ATTACH_H */
