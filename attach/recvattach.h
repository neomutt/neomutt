/**
 * @file
 * Routines for managing attachments
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

#ifndef MUTT_ATTACH_RECVATTACH_H
#define MUTT_ATTACH_RECVATTACH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "expando/lib.h"

struct AttachCtx;
struct Body;
struct BodyArray;
struct ConfigSubset;
struct Email;
struct MailboxView;
struct Menu;

void mutt_attach_init(struct AttachCtx *actx);
void mutt_update_tree(struct AttachCtx *actx);

const char *attach_format_str(char *buf, size_t buflen, size_t col, int cols, char op, const char *src, const char *prec, const char *if_str, const char *else_str, intptr_t data, MuttFormatFlags flags);
void dlg_attachment(struct ConfigSubset *sub, struct MailboxView *mv, struct Email *e, FILE *fp, bool attach_msg);

void mutt_generate_recvattach_list(struct AttachCtx *actx, struct Email *e, struct Body *parts, FILE *fp, int parent_type, int level, bool decrypted);
struct AttachPtr *current_attachment(struct AttachCtx *actx, struct Menu *menu);
void mutt_update_recvattach_menu(struct AttachCtx *actx, struct Menu *menu, bool init);
void recvattach_edit_content_type(struct AttachCtx *actx, struct Menu *menu, struct Email *e);

int ba_add_tagged(struct BodyArray *ba, struct AttachCtx *actx, struct Menu *menu);

#endif /* MUTT_ATTACH_RECVATTACH_H */
