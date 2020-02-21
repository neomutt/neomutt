/**
 * @file
 * Notmuch virtual mailbox type
 *
 * @authors
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
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

/**
 * @page notmuch NOTMUCH: Virtual mailbox type
 *
 * Notmuch virtual mailbox type
 *
 * | File                   | Description         |
 * | :--------------------- | :------------------ |
 * | notmuch/mutt_notmuch.c | @subpage nm_notmuch |
 * | notmuch/nm_db.c        | @subpage nm_db      |
 * | notmuch/path.c         | @subpage nm_path    |
 */

#ifndef MUTT_NOTMUCH_LIB_H
#define MUTT_NOTMUCH_LIB_H

#include <stddef.h>
#include <stdbool.h>
#include "core/lib.h"
#include "mx.h"
#include "path.h"

struct Email;
struct NmMboxData;
struct stat;

/* These Config Variables are only used in notmuch/mutt_notmuch.c */
extern int   C_NmDbLimit;
extern char *C_NmDefaultUrl;
extern char *C_NmExcludeTags;
extern int   C_NmOpenTimeout;
extern char *C_NmQueryType;
extern int   C_NmQueryWindowCurrentPosition;
extern char *C_NmQueryWindowTimebase;
extern char *C_NmRecordTags;
extern char *C_NmUnreadTag;
extern char *C_NmFlaggedTag;
extern char *C_NmRepliedTag;

extern struct MxOps MxNotmuchOps;

void  nm_db_debug_check          (struct Mailbox *m);
void  nm_db_longrun_done         (struct Mailbox *m);
void  nm_db_longrun_init         (struct Mailbox *m, bool writable);
char *nm_email_get_folder        (struct Email *e);
int   nm_get_all_tags            (struct Mailbox *m, char **tag_list, int *tag_count);
bool  nm_message_is_still_queried(struct Mailbox *m, struct Email *e);
void  nm_parse_type_from_query   (struct NmMboxData *mdata, char *buf);
enum MailboxType nm_path_probe   (const char *path, const struct stat *st);
void  nm_query_window_backward   (void);
void  nm_query_window_forward    (void);
int   nm_read_entire_thread      (struct Mailbox *m, struct Email *e);
int   nm_record_message          (struct Mailbox *m, char *path, struct Email *e);
int   nm_update_filename         (struct Mailbox *m, const char *old_file, const char *new_file, struct Email *e);
char *nm_url_from_query          (struct Mailbox *m, char *buf, size_t buflen);

#endif /* MUTT_NOTMUCH_LIB_H */
