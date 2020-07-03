/**
 * @file
 * Convenience wrapper for the send headers
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SEND_HEADER_H
#define MUTT_SEND_HEADER_H

#include "copy.h"
#include <stdbool.h>
#include <stdio.h>

struct Body;
struct ConfigSubset;
struct Envelope;
struct ListHead;

/**
 * enum MuttWriteHeaderMode - Modes for mutt_rfc822_write_header()
 */
enum MuttWriteHeaderMode
{
  MUTT_WRITE_HEADER_NORMAL,   ///< A normal Email, write full header + MIME headers
  MUTT_WRITE_HEADER_FCC,      ///< fcc mode, like normal mode but for Bcc header
  MUTT_WRITE_HEADER_POSTPONE, ///< A postponed Email, just the envelope info
  MUTT_WRITE_HEADER_EDITHDRS, ///< "light" mode (used for edit_hdrs)
  MUTT_WRITE_HEADER_MIME,     ///< Write protected headers
};

int  mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach, enum MuttWriteHeaderMode mode, bool privacy, bool hide_protected_subject, struct ConfigSubset *sub);
int  mutt_write_mime_header(struct Body *a, FILE *fp, struct ConfigSubset *sub);
int  mutt_write_one_header(FILE *fp, const char *tag, const char *value, const char *pfx, int wraplen, CopyHeaderFlags chflags, struct ConfigSubset *sub);
void mutt_write_references(const struct ListHead *r, FILE *fp, size_t trim, struct ConfigSubset *sub);

#endif /* MUTT_SEND_HEADER_H */
