/**
 * @file
 * Send/reply with an attachment
 *
 * @authors
 * Copyright (C) 1999-2004 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef _MUTT_RECVCMD_H
#define _MUTT_RECVCMD_H

#include <stdio.h>

struct AttachCtx;
struct Body;
struct Header;

void mutt_attach_bounce(FILE *fp, struct AttachCtx *actx, struct Body *cur);
void mutt_attach_resend(FILE *fp, struct AttachCtx *actx, struct Body *cur);
void mutt_attach_forward(FILE *fp, struct Header *hdr, struct AttachCtx *actx, struct Body *cur, int flags);
void mutt_attach_reply(FILE *fp, struct Header *hdr, struct AttachCtx *actx, struct Body *cur, int flags);

#endif /* _MUTT_RECVCMD_H */
