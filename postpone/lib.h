/**
 * @file
 * Postponed Emails
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page lib_postpone Postponed Emails
 *
 * Postponed Emails
 *
 * | File                     | Description                |
 * | :----------------------- | :------------------------- |
 * | postpone/dlg_postpone.c  | @subpage neo_dlg_postpone  |
 * | postpone/postpone.c      | @subpage neo_postpone      |
 */

#ifndef MUTT_POSTPONE_LIB_H
#define MUTT_POSTPONE_LIB_H

#include <stdbool.h>
#include "ncrypt/lib.h"

struct Email;
struct Mailbox;

struct Email *dlg_select_postponed_email(struct Mailbox *m);
int           mutt_get_postponed        (struct Mailbox *m_cur, struct Email *hdr, struct Email **cur, struct Buffer *fcc);
int           mutt_num_postponed        (struct Mailbox *m, bool force);
SecurityFlags mutt_parse_crypt_hdr      (const char *p, bool set_empty_signas, SecurityFlags crypt_app);
int           mutt_prepare_template     (FILE *fp, struct Mailbox *m, struct Email *e_new, struct Email *e, bool resend);
void          mutt_update_num_postponed (void);

#endif /* MUTT_POSTPONE_LIB_H */
