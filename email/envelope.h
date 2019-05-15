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

#ifndef MUTT_EMAIL_ENVELOPE_H
#define MUTT_EMAIL_ENVELOPE_H

#include "config.h"
#include <stdbool.h>
#include "mutt/mutt.h"
#include "address/address.h"

#define MUTT_ENV_CHANGED_IRT     (1<<0)  ///< In-Reply-To changed to link/break threads
#define MUTT_ENV_CHANGED_REFS    (1<<1)  ///< References changed to break thread
#define MUTT_ENV_CHANGED_XLABEL  (1<<2)  ///< X-Label edited
#define MUTT_ENV_CHANGED_SUBJECT (1<<3)  ///< Protected header update

/**
 * struct Envelope - The header of an email
 */
struct Envelope
{
  struct AddressList return_path;
  struct AddressList from;
  struct AddressList to;
  struct AddressList cc;
  struct AddressList bcc;
  struct AddressList sender;
  struct AddressList reply_to;
  struct AddressList mail_followup_to;
  struct AddressList x_original_to;
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

  unsigned char changed;       /* The MUTT_ENV_CHANGED_* flags specify which
                                * fields are modified */
};

bool             mutt_env_cmp_strict(const struct Envelope *e1, const struct Envelope *e2);
void             mutt_env_free(struct Envelope **p);
void             mutt_env_merge(struct Envelope *base, struct Envelope **extra);
struct Envelope *mutt_env_new(void);
int              mutt_env_to_intl(struct Envelope *env, const char **tag, char **err);
void             mutt_env_to_local(struct Envelope *e);

#endif /* MUTT_EMAIL_ENVELOPE_H */
