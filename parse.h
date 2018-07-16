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

#ifndef MUTT_PARSE_H
#define MUTT_PARSE_H

#include <stdbool.h>
#include <stdio.h>

struct Body;
struct Context;
struct Envelope;
struct Header;

/* These Config Variables are only used in parse.c */
extern char *SpamSeparator;

#define MUTT_PARTS_TOPLEVEL (1 << 0) /* is the top-level part */

int              mutt_check_encoding(const char *c);
int              mutt_check_mime_type(const char *s);
int              mutt_count_body_parts(struct Context *ctx, struct Header *hdr);
char *           mutt_extract_message_id(const char *s, const char **saveptr);
void             mutt_parse_content_type(char *s, struct Body *ct);
void             mutt_parse_mime_message(struct Context *ctx, struct Header *cur);
struct Body *    mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, bool digest);
void             mutt_parse_part(FILE *fp, struct Body *b);
struct Body *    mutt_read_mime_header(FILE *fp, bool digest);
int              mutt_rfc822_parse_line(struct Envelope *e, struct Header *hdr, char *line, char *p, short user_hdrs, short weed, short do_2047);
struct Body *    mutt_rfc822_parse_message(FILE *fp, struct Body *parent);
struct Envelope *mutt_rfc822_read_header(FILE *f, struct Header *hdr, short user_hdrs, short weed);
char *           mutt_rfc822_read_line(FILE *f, char *line, size_t *linelen);

#endif /* MUTT_PARSE_H */
