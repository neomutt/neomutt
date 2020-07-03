/**
 * @file
 * Representation of the body of an email
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page mutt_body Representation of the body of an email
 *
 * Representation of the body of an email
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "mutt_body.h"
#include "mutt_attach.h"
#include "muttlib.h"
#include "send/lib.h"

/**
 * mutt_body_copy - Create a send-mode duplicate from a receive-mode body
 * @param[in]  fp  FILE pointer to attachments
 * @param[out] tgt New Body will be saved here
 * @param[in]  src Source Body to copy
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_body_copy(FILE *fp, struct Body **tgt, struct Body *src)
{
  if (!tgt || !src)
    return -1;

  struct Body *b = NULL;
  bool use_disp;
  struct Buffer *tmp = mutt_buffer_pool_get();

  if (src->filename)
  {
    use_disp = true;
    mutt_buffer_strcpy(tmp, src->filename);
  }
  else
  {
    use_disp = false;
  }

  mutt_adv_mktemp(tmp);
  if (mutt_save_attachment(fp, src, mutt_b2s(tmp), MUTT_SAVE_NO_FLAGS, NULL) == -1)
  {
    mutt_buffer_pool_release(&tmp);
    return -1;
  }

  *tgt = mutt_body_new();
  b = *tgt;

  memcpy(b, src, sizeof(struct Body));
  TAILQ_INIT(&b->parameter);
  b->parts = NULL;
  b->next = NULL;

  b->filename = mutt_buffer_strdup(tmp);
  b->use_disp = use_disp;
  b->unlink = true;

  if (mutt_is_text_part(b))
    b->noconv = true;

  b->xtype = mutt_str_dup(b->xtype);
  b->subtype = mutt_str_dup(b->subtype);
  b->form_name = mutt_str_dup(b->form_name);
  b->d_filename = mutt_str_dup(b->d_filename);
  /* mutt_adv_mktemp() will mangle the filename in tmp,
   * so preserve it in d_filename */
  if (!b->d_filename && use_disp)
    b->d_filename = mutt_str_dup(src->filename);
  b->description = mutt_str_dup(b->description);

  /* we don't seem to need the Email structure currently.
   * XXX this may change in the future */

  if (b->email)
    b->email = NULL;

  /* copy parameters */
  struct Parameter *np = NULL, *new_param = NULL;
  TAILQ_FOREACH(np, &src->parameter, entries)
  {
    new_param = mutt_param_new();
    new_param->attribute = mutt_str_dup(np->attribute);
    new_param->value = mutt_str_dup(np->value);
    TAILQ_INSERT_HEAD(&b->parameter, new_param, entries);
  }

  mutt_stamp_attachment(b);
  mutt_buffer_pool_release(&tmp);
  return 0;
}
