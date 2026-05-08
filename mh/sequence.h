/**
 * @file
 * MH Mailbox Sequences
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MH_SEQUENCE_H
#define MUTT_MH_SEQUENCE_H

#include <stdbool.h>
#include <stdint.h>

struct Mailbox;

/**
 * enum MhSeqFlag - Flags, e.g. #MH_SEQ_UNSEEN
 */
enum MhSeqFlag
{
  MH_SEQ_NONE    =       0,  ///< No flags are set
  MH_SEQ_UNSEEN  = 1U << 0,  ///< Email hasn't been read
  MH_SEQ_REPLIED = 1U << 1,  ///< Email has been replied to
  MH_SEQ_FLAGGED = 1U << 2,  ///< Email is flagged
};
typedef uint8_t MhSeqFlags;

/**
 * struct MhSequences - Set of MH sequence numbers
 */
struct MhSequences
{
  int max;           ///< Number of flags stored
  MhSeqFlags *flags; ///< Flags for each email
};

void       mh_seq_add_one(struct Mailbox *m, int n, bool unseen, bool flagged, bool replied);
int        mh_seq_changed(struct Mailbox *m);
MhSeqFlags mh_seq_check  (struct MhSequences *mhs, int i);
void       mh_seq_free   (struct MhSequences *mhs);
int        mh_seq_read   (struct MhSequences *mhs, const char *path);
void       mh_seq_update (struct Mailbox *m);

#endif /* MUTT_MH_SEQUENCE_H */
