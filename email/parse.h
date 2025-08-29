/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020-2023 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_EMAIL_PARSE_H
#define MUTT_EMAIL_PARSE_H

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mime.h"

struct Body;
struct Buffer;
struct Email;
struct Envelope;

void             mutt_auto_subscribe      (const char *mailto);
int              mutt_check_encoding      (const char *c);
enum ContentType mutt_check_mime_type     (const char *s);
char *           mutt_extract_message_id  (const char *s, size_t *len);
bool             mutt_is_message_type     (int type, const char *subtype);
bool             mutt_matches_ignore      (const char *s);
void             mutt_parse_content_type  (const char *s, struct Body *b);
bool             mutt_parse_mailto        (struct Envelope *env, char **body, const char *src);
struct Body *    mutt_parse_multipart     (FILE *fp, const char *boundary, LOFF_T end_off, bool digest);
void             mutt_parse_part          (FILE *fp, struct Body *b);
struct Body *    mutt_read_mime_header    (FILE *fp, bool digest);
int              mutt_rfc822_parse_line   (struct Envelope *env, struct Email *e, const char *name, size_t name_len, const char *body, bool user_hdrs, bool weed, bool do_2047);
struct Body *    mutt_rfc822_parse_message(FILE *fp, struct Body *b);
struct Envelope *mutt_rfc822_read_header  (FILE *fp, struct Email *e, bool user_hdrs, bool weed);
size_t           mutt_rfc822_read_line    (FILE *fp, struct Buffer *out);

void mutt_filter_commandline_header_tag  (char *header);
void mutt_filter_commandline_header_value(char *header);

#endif /* MUTT_EMAIL_PARSE_H */
