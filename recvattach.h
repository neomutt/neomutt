/**
 * @file
 * Routines for managing attachments
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2006 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef MUTT_RECVATTACH_H
#define MUTT_RECVATTACH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "format_flags.h"

struct AttachCtx;
struct Body;
struct Email;

/* These Config Variables are only used in recvattach.c */
extern char *C_AttachSaveDir;
extern char *C_AttachSaveWithoutPrompting;
extern char *C_AttachSep;
extern bool  C_AttachSplit;
extern bool  C_DigestCollapse;
extern char *C_MessageFormat;

void mutt_attach_init(struct AttachCtx *actx);
void mutt_update_tree(struct AttachCtx *actx);

const char *attach_format_str(char *buf, size_t buflen, size_t col, int cols, char op, const char *src, const char *prec, const char *if_str, const char *else_str, unsigned long data, MuttFormatFlags flags);
void mutt_view_attachments(struct Email *e);

void mutt_generate_recvattach_list(struct AttachCtx *actx, struct Email *e, struct Body *parts, FILE *fp, int parent_type, int level, bool decrypted);

#endif /* MUTT_RECVATTACH_H */
