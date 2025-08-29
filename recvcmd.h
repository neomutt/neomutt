/**
 * @file
 * Send/reply with an attachment
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_RECVCMD_H
#define MUTT_RECVCMD_H

#include <stdio.h>
#include "send/lib.h"

struct AttachCtx;
struct Body;
struct Email;
struct Mailbox;

void attach_bounce_message  (struct Mailbox *m, FILE *fp, struct AttachCtx *actx, struct Body *b);
void mutt_attach_resend     (FILE *fp, struct Mailbox *m, struct AttachCtx *actx, struct Body *b);
void mutt_attach_forward    (FILE *fp, struct Email *e, struct AttachCtx *actx, struct Body *b, SendFlags flags);
void mutt_attach_reply      (FILE *fp, struct Mailbox *m, struct Email *e, struct AttachCtx *actx, struct Body *b, SendFlags flags);
void mutt_attach_mail_sender(struct AttachCtx *actx, struct Body *b);

#endif /* MUTT_RECVCMD_H */
