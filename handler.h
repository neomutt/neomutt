/**
 * @file
 * Decide how to display email content
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2010,2013 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_HANDLER_H
#define MUTT_HANDLER_H

#include <stddef.h>
#include <iconv.h>
#include <stdbool.h>

struct Body;
struct State;

/* These Config Variables are only used in handler.c */
extern bool  C_HonorDisposition;
extern bool  C_ImplicitAutoview;
extern bool  C_IncludeEncrypted;
extern bool  C_IncludeOnlyfirst;
extern struct Slist *C_PreferredLanguages;
extern bool  C_ReflowText;
extern char *C_ShowMultipartAlternative;

int  mutt_body_handler(struct Body *b, struct State *s);
bool mutt_can_decode(struct Body *a);
void mutt_decode_attachment(struct Body *b, struct State *s);
void mutt_decode_base64(struct State *s, size_t len, bool istext, iconv_t cd);

#endif /* MUTT_HANDLER_H */
