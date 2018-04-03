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
  FILE *fp; /**< used in the recvattach menu. */
  int parent_type;
  char *tree;
  int level;
  int num;
  bool unowned : 1;           /**< don't unlink on detach */
  unsigned int decrypted : 1; /**< not part of message as stored in the hdr->content. */
};

/**
 * struct AttachCtx - A set of attachments
 */
struct AttachCtx
{
  struct Header *hdr; /**< used by recvattach for updating */
  FILE *root_fp;      /**< used by recvattach for updating */

  struct AttachPtr **idx;
  short idxlen;
  short idxmax;

  short *v2r;   /**< mapping from virtual to real attachment */
  short vcount; /**< the number of virtual attachments */

  FILE **fp_idx; /**< Extra FILE* used for decryption */
  short fp_len;
  short fp_max;

  struct Body **body_idx; /**< Extra struct Body* used for decryption */
  short body_len;
  short body_max;
};

void mutt_attach_init(struct AttachCtx *actx);
void mutt_update_tree(struct AttachCtx *actx);
int mutt_view_attachment(FILE *fp, struct Body *a, int flag, struct Header *hdr,
                         struct AttachCtx *actx);

int mutt_tag_attach(struct Menu *menu, int n, int m);
int mutt_attach_display_loop(struct Menu *menu, int op, struct Header *hdr,
                             struct AttachCtx *actx, bool recv);

void mutt_save_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                               struct Body *top, struct Header *hdr, struct Menu *menu);
void mutt_pipe_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                               struct Body *top, bool filter);
void mutt_print_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                                struct Body *top);

void mutt_attach_bounce(FILE *fp, struct AttachCtx *actx, struct Body *cur);
void mutt_attach_resend(FILE *fp, struct AttachCtx *actx, struct Body *cur);
void mutt_attach_forward(FILE *fp, struct Header *hdr, struct AttachCtx *actx,
                         struct Body *cur, int flags);
void mutt_attach_reply(FILE *fp, struct Header *hdr, struct AttachCtx *actx,
                       struct Body *cur, int flags);

void mutt_actx_add_attach(struct AttachCtx *actx, struct AttachPtr *attach);
void mutt_actx_add_fp(struct AttachCtx *actx, FILE *new_fp);
void mutt_actx_add_body(struct AttachCtx *actx, struct Body *new_body);
void mutt_actx_free_entries(struct AttachCtx *actx);
void mutt_free_attach_context(struct AttachCtx **pactx);

#endif /* _MUTT_ATTACH_H */
