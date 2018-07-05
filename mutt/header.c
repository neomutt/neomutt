/**
 * @file
 * Representation of the email's header
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
 * @page header Representation of the email's header
 *
 * Representation of the email's header
 */

#include "config.h"
#include <stdbool.h>
#include "header.h"
#include "body.h"
#include "envelope.h"
#include "list.h"
#include "memory.h"
#include "queue.h"
#include "tags.h"

/**
 * mutt_header_free - Free an email Header
 * @param h Header to free
 */
void mutt_header_free(struct Header **h)
{
  if (!h || !*h)
    return;
  mutt_env_free(&(*h)->env);
  mutt_body_free(&(*h)->content);
  FREE(&(*h)->maildir_flags);
  FREE(&(*h)->tree);
  FREE(&(*h)->path);
#ifdef MIXMASTER
  mutt_list_free(&(*h)->chain);
#endif
  driver_tags_free(&(*h)->tags);
#if defined(USE_POP) || defined(USE_IMAP) || defined(USE_NNTP) || defined(USE_NOTMUCH)
  if ((*h)->free_cb)
    (*h)->free_cb(*h);
  FREE(&(*h)->data);
#endif
  FREE(h);
}

/**
 * mutt_header_new - Create a new email Header
 * @retval ptr Newly created Header
 */
struct Header *mutt_header_new(void)
{
  struct Header *h = mutt_mem_calloc(1, sizeof(struct Header));
#ifdef MIXMASTER
  STAILQ_INIT(&h->chain);
#endif
  STAILQ_INIT(&h->tags);
  return h;
}

/**
 * mutt_header_cmp_strict - Strictly compare message headers
 * @param h1 First Header
 * @param h2 Second Header
 * @retval true Headers are strictly identical
 */
bool mutt_header_cmp_strict(const struct Header *h1, const struct Header *h2)
{
  if (h1 && h2)
  {
    if ((h1->received != h2->received) || (h1->date_sent != h2->date_sent) ||
        (h1->content->length != h2->content->length) ||
        (h1->lines != h2->lines) || (h1->zhours != h2->zhours) ||
        (h1->zminutes != h2->zminutes) || (h1->zoccident != h2->zoccident) ||
        (h1->mime != h2->mime) || !mutt_env_cmp_strict(h1->env, h2->env) ||
        !mutt_body_cmp_strict(h1->content, h2->content))
    {
      return false;
    }
    else
      return true;
  }
  else
  {
    if (!h1 && !h2)
      return true;
    else
      return false;
  }
}
