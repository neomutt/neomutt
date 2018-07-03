/**
 * @file
 * Handling of email attachments
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

/* common protos for compose / attach menus */

#ifndef _MUTT_ATTACH2_H
#define _MUTT_ATTACH2_H

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/mutt.h"

struct Menu;
struct Header;
struct Body;

int mutt_tag_attach(struct Menu *menu, int n, int m);
int mutt_attach_display_loop(struct Menu *menu, int op, struct Header *hdr,
                             struct AttachCtx *actx, bool recv);

void mutt_save_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                               struct Body *top, struct Header *hdr, struct Menu *menu);
void mutt_pipe_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                               struct Body *top, bool filter);
void mutt_print_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                                struct Body *top);

int mutt_view_attachment(FILE *fp, struct Body *a, int flag, struct Header *hdr, struct AttachCtx *actx);

#endif /* _MUTT_ATTACH2_H */
