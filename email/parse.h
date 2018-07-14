/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef _EMAIL_PARSE_H
#define _EMAIL_PARSE_H

#include <stdio.h>

struct Body;
struct Envelope;
struct Header;

struct Envelope *mutt_rfc822_read_header(FILE *f, struct Header *hdr, short user_hdrs, short weed);
char *           mutt_rfc822_read_line(FILE *f, char *line, size_t *linelen);
int              mutt_rfc822_parse_line(struct Envelope *e, struct Header *hdr, char *line, char *p, short user_hdrs, short weed, short do_2047);
int              mutt_check_encoding(const char *c);
int              mutt_check_mime_type(const char *s);
char *           mutt_extract_message_id(const char *s, const char **saveptr);
void             mutt_parse_content_type(char *s, struct Body *ct);
struct Body *    mutt_read_mime_header(FILE *fp, bool digest);

bool mutt_is_message_type(int type, const char *subtype);
void             mutt_parse_part(FILE *fp, struct Body *b);
struct Body *    mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, bool digest);
struct Body *    mutt_rfc822_parse_message(FILE *fp, struct Body *parent);

#endif /* _EMAIL_PARSE_H */
