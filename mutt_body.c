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
 * @page neo_mutt_body Representation of the body of an email
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
#include "attach/lib.h"
#include "send/lib.h"
#include "muttlib.h"

/**
 * mutt_body_copy - Create a send-mode duplicate from a receive-mode body
 * @param[in]  fp    FILE pointer to attachments
 * @param[out] b_dst New Body will be saved here
 * @param[in]  b_src Source Body to copy
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_body_copy(FILE *fp, struct Body **b_dst, struct Body *b_src)
{
  if (!b_dst || !b_src)
    return -1;

  struct Body *b = NULL;
  bool use_disp;
  struct Buffer *tmp = buf_pool_get();

  if (b_src->filename)
  {
    use_disp = true;
    buf_strcpy(tmp, b_src->filename);
  }
  else
  {
    use_disp = false;
  }

  mutt_adv_mktemp(tmp);
  if (mutt_save_attachment(fp, b_src, buf_string(tmp), MUTT_SAVE_NO_FLAGS, NULL) == -1)
  {
    buf_pool_release(&tmp);
    return -1;
  }

  *b_dst = mutt_body_new();
  b = *b_dst;

  memcpy(b, b_src, sizeof(struct Body));
  TAILQ_INIT(&b->parameter);
  b->parts = NULL;
  b->next = NULL;

  b->filename = buf_strdup(tmp);
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
    b->d_filename = mutt_str_dup(b_src->filename);
  b->description = mutt_str_dup(b->description);

  b->language = mutt_str_dup(b->language);
  b->charset = mutt_str_dup(b->charset);

  b->content = NULL;
  b->aptr = NULL;
  b->mime_headers = NULL;

  /* we don't seem to need the Email structure currently.
   * XXX this may change in the future */
  b->email = NULL;

  /* copy parameters */
  struct Parameter *np = NULL, *new_param = NULL;
  TAILQ_FOREACH(np, &b_src->parameter, entries)
  {
    new_param = mutt_param_new();
    new_param->attribute = mutt_str_dup(np->attribute);
    new_param->value = mutt_str_dup(np->value);
    TAILQ_INSERT_HEAD(&b->parameter, new_param, entries);
  }

  mutt_stamp_attachment(b);
  buf_pool_release(&tmp);
  return 0;
}
