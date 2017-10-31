/**
 * @file
 * Representation of an email header (envelope)
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

#ifndef _MUTT_ENVELOPE_H
#define _MUTT_ENVELOPE_H

#include <stdbool.h>
#include "lib/list.h"

/**
 * struct Envelope - The header of an email
 */
struct Envelope
{
  struct Address *return_path;
  struct Address *from;
  struct Address *to;
  struct Address *cc;
  struct Address *bcc;
  struct Address *sender;
  struct Address *reply_to;
  struct Address *mail_followup_to;
  struct Address *x_original_to;
  char *list_post; /**< this stores a mailto URL, or nothing */
  char *subject;
  char *real_subj; /**< offset of the real subject */
  char *disp_subj; /**< display subject (modified copy of subject) */
  char *message_id;
  char *supersedes;
  char *date;
  char *x_label;
  char *organization;
#ifdef USE_NNTP
  char *newsgroups;
  char *xref;
  char *followup_to;
  char *x_comment_to;
#endif
  struct Buffer *spam;
  struct ListHead references;  /**< message references (in reverse order) */
  struct ListHead in_reply_to; /**< in-reply-to header content */
  struct ListHead userhdrs;    /**< user defined headers */

  bool irt_changed : 1;  /**< In-Reply-To changed to link/break threads */
  bool refs_changed : 1; /**< References changed to break thread */
};

struct Envelope *mutt_new_envelope(void);
void mutt_free_envelope(struct Envelope **p);
void mutt_merge_envelopes(struct Envelope *base, struct Envelope **extra);

#endif /* _MUTT_ENVELOPE_H */
