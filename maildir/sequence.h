/**
 * @file
 * MH Mailbox Sequences
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

#ifndef MUTT_MAILDIR_SEQUENCE_H
#define MUTT_MAILDIR_SEQUENCE_H

#include <stdbool.h>
#include <stdint.h>

struct Mailbox;

typedef uint8_t MhSeqFlags;     ///< Flags, e.g. #MH_SEQ_UNSEEN
#define MH_SEQ_NO_FLAGS      0  ///< No flags are set
#define MH_SEQ_UNSEEN  (1 << 0) ///< Email hasn't been read
#define MH_SEQ_REPLIED (1 << 1) ///< Email has been replied to
#define MH_SEQ_FLAGGED (1 << 2) ///< Email is flagged

/**
 * struct MhSequences - Set of MH sequence numbers
 */
struct MhSequences
{
  int max;           ///< Number of flags stored
  MhSeqFlags *flags; ///< Flags for each email
};

int        mh_seq_read   (struct MhSequences *mhs, const char *path);
void       mh_seq_add_one(struct Mailbox *m, int n, bool unseen, bool flagged, bool replied);
int        mh_seq_changed(struct Mailbox *m);
void       mh_seq_update (struct Mailbox *m);
MhSeqFlags mh_seq_check  (struct MhSequences *mhs, int i);
void       mh_seq_free   (struct MhSequences *mhs);

#endif /* MUTT_MAILDIR_SEQUENCE_H */
