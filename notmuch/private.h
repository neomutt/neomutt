/**
 * @file
 * Notmuch private types
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

#include <notmuch.h>
#include <stdbool.h>
#include <time.h>

struct Mailbox;

#ifndef MUTT_NOTMUCH_PRIVATE_H
#define MUTT_NOTMUCH_PRIVATE_H

#ifdef LIBNOTMUCH_CHECK_VERSION
#undef LIBNOTMUCH_CHECK_VERSION
#endif

#ifndef HAVE_NOTMUCH_DATABASE_INDEX_FILE
#define HAVE_NOTMUCH_DATABASE_INDEX_FILE 0
#endif

#ifndef HAVE_NOTMUCH_DATABASE_OPEN_WITH_CONFIG
#define HAVE_NOTMUCH_DATABASE_OPEN_WITH_CONFIG 0
#endif

/*
 * The definition in <notmuch.h> is broken.
 *
 * Corrects for libnotmuch releases with missing version bumps:
 * - libnotmuch 5.4 released with notmuch 0.32. notmuch 0.32.3 fixed version.
 * - libnotmuch 5.1 released with notmuch 0.26. notmuch 0.26.1 fixed version.
 */
#define LIBNOTMUCH_CHECK_VERSION(major, minor, micro)                              \
  (major == 5 && minor == 4 && HAVE_NOTMUCH_DATABASE_OPEN_WITH_CONFIG)          || \
  (major == 5 && minor == 1 && HAVE_NOTMUCH_DATABASE_INDEX_FILE)                || \
  ((LIBNOTMUCH_MAJOR_VERSION > (major) ||                                          \
   (LIBNOTMUCH_MAJOR_VERSION == (major) && LIBNOTMUCH_MINOR_VERSION > (minor))  || \
   (LIBNOTMUCH_MAJOR_VERSION == (major) &&                                         \
    LIBNOTMUCH_MINOR_VERSION == (minor) && LIBNOTMUCH_MICRO_VERSION >= (micro))))

extern const char NmUrlProtocol[];
extern const int NmUrlProtocolLen;

notmuch_database_t *nm_db_do_open     (const char *filename, bool writable, bool verbose);
void                nm_db_free        (notmuch_database_t *db);
const char *        nm_db_get_filename(struct Mailbox *m);
int                 nm_db_get_mtime   (struct Mailbox *m, time_t *mtime);
notmuch_database_t *nm_db_get         (struct Mailbox *m, bool writable);
bool                nm_db_is_longrun  (struct Mailbox *m);
int                 nm_db_release     (struct Mailbox *m);
int                 nm_db_trans_begin (struct Mailbox *m);
int                 nm_db_trans_end   (struct Mailbox *m);

#endif /* MUTT_NOTMUCH_PRIVATE_H */
