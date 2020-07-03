/**
 * @file
 * Miscellaneous functions for sending an email
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_SEND_SENDLIB_H
#define MUTT_SEND_SENDLIB_H

#include <stdbool.h>
#include <stdio.h>
#include "email/lib.h"

struct AddressList;
struct Mailbox;

#define MUTT_RANDTAG_LEN 16

int              mutt_bounce_message(FILE *fp, struct Email *e, struct AddressList *to);
const char *     mutt_fqdn(bool may_hide_host);
struct Content * mutt_get_content_info(const char *fname, struct Body *b);
enum ContentType mutt_lookup_mime_type(struct Body *att, const char *path);
struct Body *    mutt_make_file_attach(const char *path);
struct Body *    mutt_make_message_attach(struct Mailbox *m, struct Email *e, bool attach_msg);
void             mutt_message_to_7bit(struct Body *a, FILE *fp);
void             mutt_prepare_envelope(struct Envelope *env, bool final);
void             mutt_stamp_attachment(struct Body *a);
void             mutt_unprepare_envelope(struct Envelope *env);
void             mutt_update_encoding(struct Body *a);
int              mutt_write_fcc(const char *path, struct Email *e, const char *msgid, bool post, const char *fcc, char **finalpath);
int              mutt_write_multiple_fcc(const char *path, struct Email *e, const char *msgid, bool post, char *fcc, char **finalpath);

#endif /* MUTT_SEND_SENDLIB_H */
