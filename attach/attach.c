/**
 * @file
 * Handling of email attachments
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 David Purton <dcpurton@marshwiggle.net>
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

/**
 * @page attach_attach Email attachments
 *
 * Handling of email attachments
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "attach.h"

/// Step to grow attachment arrays
const int AttachArrayGrow = 5;

/**
 * mutt_aptr_new - Create a new Attachment Pointer
 * @retval ptr New Attachment Pointer
 */
struct AttachPtr *mutt_aptr_new(void)
{
  return MUTT_MEM_CALLOC(1, struct AttachPtr);
}

/**
 * mutt_aptr_free - Free an Attachment Pointer
 * @param[out] ptr Attachment Pointer
 */
void mutt_aptr_free(struct AttachPtr **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AttachPtr *aptr = *ptr;
  FREE(&aptr->tree);

  FREE(ptr);
}

/**
 * mutt_actx_add_attach - Add an Attachment to an Attachment Context
 * @param actx   Attachment context
 * @param attach Attachment to add
 */
void mutt_actx_add_attach(struct AttachCtx *actx, struct AttachPtr *attach)
{
  if (!actx || !attach)
    return;

  if (actx->idxlen == actx->idxmax)
  {
    size_t cur_size = actx->idxmax * sizeof(struct AttachPtr *);
    size_t new_size = (actx->idxmax + AttachArrayGrow) * sizeof(struct AttachPtr *);
    mutt_mem_recalloc(&actx->idx, cur_size, new_size);

    cur_size = actx->idxmax * sizeof(short);
    new_size = (actx->idxmax + AttachArrayGrow) * sizeof(short);
    mutt_mem_recalloc(&actx->v2r, cur_size, new_size);

    actx->idxmax += AttachArrayGrow;
  }

  actx->idx[actx->idxlen++] = attach;
}

/**
 * mutt_actx_ins_attach - Insert an Attachment into an Attachment Context at Specified Index
 * @param actx   Attachment context
 * @param attach Attachment to insert
 * @param aidx   Index to insert attachment at
 */
void mutt_actx_ins_attach(struct AttachCtx *actx, struct AttachPtr *attach, int aidx)
{
  if (!actx || !attach)
    return;

  if ((aidx < 0) || (aidx > actx->idxmax))
    return;

  if (actx->idxlen == actx->idxmax)
  {
    size_t cur_size = actx->idxmax * sizeof(struct AttachPtr *);
    size_t new_size = (actx->idxmax + AttachArrayGrow) * sizeof(struct AttachPtr *);
    mutt_mem_recalloc(&actx->idx, cur_size, new_size);

    cur_size = actx->idxmax * sizeof(short);
    new_size = (actx->idxmax + AttachArrayGrow) * sizeof(short);
    mutt_mem_recalloc(&actx->v2r, cur_size, new_size);

    actx->idxmax += AttachArrayGrow;
  }

  actx->idxlen++;

  for (int i = actx->idxlen - 1; i > aidx; i--)
    actx->idx[i] = actx->idx[i - 1];

  actx->idx[aidx] = attach;
}

/**
 * mutt_actx_add_fp - Save a File handle to the Attachment Context
 * @param actx   Attachment context
 * @param fp_new File handle to save
 */
void mutt_actx_add_fp(struct AttachCtx *actx, FILE *fp_new)
{
  if (!actx || !fp_new)
    return;

  if (actx->fp_len == actx->fp_max)
  {
    const size_t cur_size = actx->fp_max * sizeof(FILE *);
    const size_t new_size = (actx->fp_max + AttachArrayGrow) * sizeof(FILE *);
    mutt_mem_recalloc(&actx->fp_idx, cur_size, new_size);

    actx->fp_max += AttachArrayGrow;
  }

  actx->fp_idx[actx->fp_len++] = fp_new;
}

/**
 * mutt_actx_add_body - Add an email body to an Attachment Context
 * @param actx Attachment context
 * @param b    Email Body to add
 */
void mutt_actx_add_body(struct AttachCtx *actx, struct Body *b)
{
  if (!actx || !b)
    return;

  if (actx->body_len == actx->body_max)
  {
    const size_t cur_size = actx->body_max * sizeof(struct Body *);
    const size_t new_size = (actx->body_max + AttachArrayGrow) * sizeof(struct Body *);
    mutt_mem_recalloc(&actx->body_idx, cur_size, new_size);

    actx->body_max += AttachArrayGrow;
  }

  actx->body_idx[actx->body_len++] = b;
}

/**
 * mutt_actx_entries_free - Free entries in an Attachment Context
 * @param actx Attachment context
 */
void mutt_actx_entries_free(struct AttachCtx *actx)
{
  if (!actx)
    return;

  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->body)
      actx->idx[i]->body->aptr = NULL;
    mutt_aptr_free(&actx->idx[i]);
  }
  actx->idxlen = 0;
  actx->vcount = 0;

  for (int i = 0; i < actx->fp_len; i++)
    mutt_file_fclose(&actx->fp_idx[i]);
  actx->fp_len = 0;

  for (int i = 0; i < actx->body_len; i++)
    mutt_body_free(&actx->body_idx[i]);
  actx->body_len = 0;
}

/**
 * mutt_actx_new - Create a new Attachment Context
 * @retval ptr New Attachment Context
 */
struct AttachCtx *mutt_actx_new(void)
{
  return MUTT_MEM_CALLOC(1, struct AttachCtx);
}

/**
 * mutt_actx_free - Free an Attachment Context
 * @param[out] ptr Attachment context
 */
void mutt_actx_free(struct AttachCtx **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AttachCtx *actx = *ptr;

  mutt_actx_entries_free(actx);
  FREE(&actx->idx);
  FREE(&actx->v2r);
  FREE(&actx->fp_idx);
  FREE(&actx->body_idx);
  FREE(ptr);
}
