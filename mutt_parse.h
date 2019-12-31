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

#ifndef MUTT_MUTT_PARSE_H
#define MUTT_MUTT_PARSE_H

#include <regex.h>
#include "mutt/mutt.h"
#include "email/lib.h"

struct Mailbox;

/**
 * struct AttachMatch - An attachment matching a regex for attachment counter
 */
struct AttachMatch
{
  const char *major;
  enum ContentType major_int;
  const char *minor;
  regex_t minor_regex;
};

extern struct ListHead AttachAllow;
extern struct ListHead AttachExclude;
extern struct ListHead InlineAllow;
extern struct ListHead InlineExclude;

int  mutt_count_body_parts(struct Mailbox *m, struct Email *e);
void mutt_parse_mime_message(struct Mailbox *m, struct Email *e);
void mutt_attachmatch_free(struct AttachMatch **ptr);

#endif /* MUTT_MUTT_PARSE_H */
