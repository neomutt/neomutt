/**
 * @file
 * String processing routines to generate the mail index
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

#ifndef MUTT_HDRLINE_H
#define MUTT_HDRLINE_H

#include <stdbool.h>
#include <stdio.h>
#include "format_flags.h"

struct Address;
struct Context;
struct Email;

/* These Config Variables are only used in hdrline.c */
extern struct MbTable *C_CryptChars;
extern struct MbTable *C_FlagChars;
extern struct MbTable *C_FromChars;
extern struct MbTable *C_ToChars;

/**
 * struct HdrFormatInfo - Data passed to index_format_str()
 */
struct HdrFormatInfo
{
  struct Context *ctx;
  struct Mailbox *mailbox;
  struct Email *email;
  const char *pager_progress;
};

bool mutt_is_mail_list(const struct Address *addr);
bool mutt_is_subscribed_list(const struct Address *addr);
void mutt_make_string_flags(char *buf, size_t buflen, const char *s,
                            struct Context *ctx, struct Mailbox *m,
                            struct Email *e, MuttFormatFlags flags);
void mutt_make_string_info(char *buf, size_t buflen, int cols, const char *s,
                           struct HdrFormatInfo *hfi, MuttFormatFlags flags);

#define mutt_make_string(BUF, BUFLEN, S, CTX, M, E)                            \
  mutt_make_string_flags(BUF, BUFLEN, S, CTX, M, E, 0)

#endif /* MUTT_HDRLINE_H */
