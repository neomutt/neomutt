/**
 * @file
 * Handling of email attachments
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2004,2006 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page attach Handling of email attachments
 *
 * Handling of email attachments
 */

#include "config.h"
#include "attach.h"
#include "body.h"
#include "file.h"
#include "memory.h"

/**
 * mutt_actx_add_attach - Add an Attachment to an Attachment Context
 * @param actx   Attachment context
 * @param attach Attachment to add
 */
void mutt_actx_add_attach(struct AttachCtx *actx, struct AttachPtr *attach)
{
  if (actx->idxlen == actx->idxmax)
  {
    actx->idxmax += 5;
    mutt_mem_realloc(&actx->idx, sizeof(struct AttachPtr *) * actx->idxmax);
    mutt_mem_realloc(&actx->v2r, sizeof(short) * actx->idxmax);
    for (int i = actx->idxlen; i < actx->idxmax; i++)
      actx->idx[i] = NULL;
  }

  actx->idx[actx->idxlen++] = attach;
}

/**
 * mutt_actx_add_fp - Save a File handle to the Attachment Context
 * @param actx   Attachment context
 * @param new_fp File handle to save
 */
void mutt_actx_add_fp(struct AttachCtx *actx, FILE *new_fp)
{
  if (actx->fp_len == actx->fp_max)
  {
    actx->fp_max += 5;
    mutt_mem_realloc(&actx->fp_idx, sizeof(FILE *) * actx->fp_max);
    for (int i = actx->fp_len; i < actx->fp_max; i++)
      actx->fp_idx[i] = NULL;
  }

  actx->fp_idx[actx->fp_len++] = new_fp;
}

/**
 * mutt_actx_add_body - Add an email box to an Attachment Context
 * @param actx     Attachment context
 * @param new_body Email Body to add
 */
void mutt_actx_add_body(struct AttachCtx *actx, struct Body *new_body)
{
  if (actx->body_len == actx->body_max)
  {
    actx->body_max += 5;
    mutt_mem_realloc(&actx->body_idx, sizeof(struct Body *) * actx->body_max);
    for (int i = actx->body_len; i < actx->body_max; i++)
      actx->body_idx[i] = NULL;
  }

  actx->body_idx[actx->body_len++] = new_body;
}

/**
 * mutt_actx_free_entries - Free entries in an Attachment Context
 * @param actx Attachment context
 */
void mutt_actx_free_entries(struct AttachCtx *actx)
{
  int i;

  for (i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->content)
      actx->idx[i]->content->aptr = NULL;
    FREE(&actx->idx[i]->tree);
    FREE(&actx->idx[i]);
  }
  actx->idxlen = 0;
  actx->vcount = 0;

  for (i = 0; i < actx->fp_len; i++)
    mutt_file_fclose(&actx->fp_idx[i]);
  actx->fp_len = 0;

  for (i = 0; i < actx->body_len; i++)
    mutt_body_free(&actx->body_idx[i]);
  actx->body_len = 0;
}

/**
 * mutt_actx_free - Free an Attachment Context
 * @param pactx Attachment context
 */
void mutt_actx_free(struct AttachCtx **pactx)
{
  struct AttachCtx *actx = NULL;

  if (!pactx || !*pactx)
    return;

  actx = *pactx;
  mutt_actx_free_entries(actx);
  FREE(&actx->idx);
  FREE(&actx->v2r);
  FREE(&actx->fp_idx);
  FREE(&actx->body_idx);
  FREE(pactx);
}
