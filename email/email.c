/**
 * @file
 * Representation of an email
 *
 * @authors
 * Copyright (C) 1996-2009,2012 Michael R. Elkins <me@mutt.org>
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
 * @page email_email Representation of an email
 *
 * Representation of an email
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/mutt.h"
#include "email.h"
#include "body.h"
#include "envelope.h"
#include "tags.h"

/**
 * mutt_email_free - Free an Email
 * @param e Email to free
 */
void mutt_email_free(struct Email **e)
{
  if (!e || !*e)
    return;
  mutt_env_free(&(*e)->env);
  mutt_body_free(&(*e)->content);
  FREE(&(*e)->maildir_flags);
  FREE(&(*e)->tree);
  FREE(&(*e)->path);
#ifdef MIXMASTER
  mutt_list_free(&(*e)->chain);
#endif
  driver_tags_free(&(*e)->tags);
  if ((*e)->edata && (*e)->free_edata)
    (*e)->free_edata(&(*e)->edata);
  FREE(e);
}

/**
 * mutt_email_new - Create a new Email
 * @retval ptr Newly created Email
 */
struct Email *mutt_email_new(void)
{
  struct Email *e = mutt_mem_calloc(1, sizeof(struct Email));
#ifdef MIXMASTER
  STAILQ_INIT(&e->chain);
#endif
  STAILQ_INIT(&e->tags);
  return e;
}

/**
 * mutt_email_cmp_strict - Strictly compare message emails
 * @param e1 First Email
 * @param e2 Second Email
 * @retval true Emails are strictly identical
 */
bool mutt_email_cmp_strict(const struct Email *e1, const struct Email *e2)
{
  if (e1 && e2)
  {
    if ((e1->received != e2->received) || (e1->date_sent != e2->date_sent) ||
        (e1->content->length != e2->content->length) ||
        (e1->lines != e2->lines) || (e1->zhours != e2->zhours) ||
        (e1->zminutes != e2->zminutes) || (e1->zoccident != e2->zoccident) ||
        (e1->mime != e2->mime) || !mutt_env_cmp_strict(e1->env, e2->env) ||
        !mutt_body_cmp_strict(e1->content, e2->content))
    {
      return false;
    }
    else
      return true;
  }
  else
  {
    if (!e1 && !e2)
      return true;
    else
      return false;
  }
}
