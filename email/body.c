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
 * @page email_body Representation of the body of an email
 *
 * Representation of the body of an email
 */

#include "config.h"
#include <stdbool.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "body.h"
#include "email.h"
#include "envelope.h"
#include "mime.h"
#include "parameter.h"

/**
 * mutt_body_new - Create a new Body
 * @retval ptr Newly allocated Body
 */
struct Body *mutt_body_new(void)
{
  struct Body *p = mutt_mem_calloc(1, sizeof(struct Body));

  p->disposition = DISP_ATTACH;
  p->use_disp = true;
  TAILQ_INIT(&p->parameter);
  return p;
}

/**
 * mutt_body_free - Free a Body
 * @param[out] ptr Body to free
 */
void mutt_body_free(struct Body **ptr)
{
  if (!ptr)
    return;

  struct Body *a = *ptr, *b = NULL;

  while (a)
  {
    b = a;
    a = a->next;

    mutt_param_free(&b->parameter);
    if (b->filename)
    {
      if (b->unlink)
        unlink(b->filename);
      mutt_debug(LL_DEBUG1, "%sunlinking %s\n", b->unlink ? "" : "not ", b->filename);
    }

    FREE(&b->filename);
    FREE(&b->d_filename);
    FREE(&b->charset);
    FREE(&b->content);
    FREE(&b->xtype);
    FREE(&b->subtype);
    FREE(&b->language);
    FREE(&b->description);
    FREE(&b->form_name);

    if (b->email)
    {
      /* Don't free twice (b->email->content = b->parts) */
      b->email->content = NULL;
      email_free(&b->email);
    }

    mutt_env_free(&b->mime_headers);
    mutt_body_free(&b->parts);
    FREE(&b);
  }

  *ptr = NULL;
}

/**
 * mutt_body_cmp_strict - Strictly compare two email Body's
 * @param b1 First Body
 * @param b2 Second Body
 * @retval true Body's are strictly identical
 */
bool mutt_body_cmp_strict(const struct Body *b1, const struct Body *b2)
{
  if (!b1 || !b2)
    return false;

  if ((b1->type != b2->type) || (b1->encoding != b2->encoding) ||
      !mutt_str_equal(b1->subtype, b2->subtype) ||
      !mutt_str_equal(b1->description, b2->description) ||
      !mutt_param_cmp_strict(&b1->parameter, &b2->parameter) || (b1->length != b2->length))
  {
    return false;
  }
  return true;
}

/**
 * mutt_body_get_charset - Get a body's character set
 * @param b      Body to examine
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @retval ptr  Buffer containing character set
 * @retval NULL On error, or if not a text type
 */
char *mutt_body_get_charset(struct Body *b, char *buf, size_t buflen)
{
  char *p = NULL;

  if (b && (b->type != TYPE_TEXT))
    return NULL;

  if (b)
    p = mutt_param_get(&b->parameter, "charset");

  if (p)
    mutt_ch_canonical_charset(buf, buflen, p);
  else
    mutt_str_copy(buf, "us-ascii", buflen);

  return buf;
}
