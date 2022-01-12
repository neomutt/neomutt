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

#ifndef MUTT_EMAIL_ATTACH_H
#define MUTT_EMAIL_ATTACH_H

#include <stdbool.h>
#include <stdio.h>

struct Body;

/**
 * struct AttachPtr - An email to which things will be attached
 */
struct AttachPtr
{
  struct Body *body;    ///< Attachment
  FILE *fp;             ///< Used in the recvattach menu
  int parent_type;      ///< Type of parent attachment, e.g. #TYPE_MULTIPART
  char *tree;           ///< Tree characters to display
  int level;            ///< Nesting depth of attachment
  int num;              ///< Attachment index number
  bool unowned : 1;     ///< Don't unlink on detach
  bool decrypted : 1;   ///< Not part of message as stored in the email->body
};

/**
 * struct AttachCtx - A set of attachments
 */
struct AttachCtx
{
  struct Email *email;    ///< Used by recvattach for updating
  FILE *fp_root;          ///< Used by recvattach for updating

  struct AttachPtr **idx; ///< Array of attachments
  short idxlen;           ///< Number of attachmentes
  short idxmax;           ///< Size of attachment array

  short *v2r;             ///< Mapping from virtual to real attachment
  short vcount;           ///< The number of virtual attachments

  FILE **fp_idx;          ///< Extra FILE* used for decryption
  short fp_len;           ///< Number of FILE handles
  short fp_max;           ///< Size of FILE array

  struct Body **body_idx; ///< Extra struct Body* used for decryption
  short body_len;         ///< Number of Body parts
  short body_max;         ///< Size of Body array
};

void              mutt_actx_add_attach  (struct AttachCtx *actx, struct AttachPtr *attach);
void              mutt_actx_ins_attach  (struct AttachCtx *actx, struct AttachPtr *attach, int aidx);
void              mutt_actx_add_body    (struct AttachCtx *actx, struct Body *new_body);
void              mutt_actx_add_fp      (struct AttachCtx *actx, FILE *fp_new);
void              mutt_actx_free        (struct AttachCtx **ptr);
void              mutt_actx_entries_free(struct AttachCtx *actx);
struct AttachCtx *mutt_actx_new         (void);

#endif /* MUTT_EMAIL_ATTACH_H */
