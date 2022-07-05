/**
 * @file
 * GUI manage the main index (list of emails)
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

/**
 * @page lib_index Index
 *
 * Display a list of Emails
 *
 * | File                 | Description                 |
 * | :------------------- | :-------------------------- |
 * | index/config.c       | @subpage index_config       |
 * | index/dlg_index.c    | @subpage index_dlg_index    |
 * | index/functions.c    | @subpage index_functions    |
 * | index/ibar.c         | @subpage index_ibar         |
 * | index/index.c        | @subpage index_index        |
 * | index/ipanel.c       | @subpage index_ipanel       |
 * | index/private_data.c | @subpage index_private_data |
 * | index/shared_data.c  | @subpage index_shared_data  |
 */

#ifndef MUTT_INDEX_LIB_H
#define MUTT_INDEX_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "functions.h"   // IWYU pragma: keep
#include "mx.h"          // IWYU pragma: keep
#include "shared_data.h" // IWYU pragma: keep

struct Email;
struct MailboxView;
struct Menu;
struct MuttWindow;

// Observers of #NT_INDEX will be passed an #IndexSharedData.
typedef uint8_t NotifyIndex;         ///< Flags, e.g. #NT_INDEX_ACCOUNT
#define NT_INDEX_NO_FLAGS        0   ///< No flags are set
#define NT_INDEX_ADD       (1 << 0)  ///< New Index Shared Data has been created
#define NT_INDEX_DELETE    (1 << 1)  ///< Index Shared Data is about to be freed
#define NT_INDEX_SUBSET    (1 << 2)  ///< Config Subset has changed
#define NT_INDEX_ACCOUNT   (1 << 3)  ///< Account has changed
#define NT_INDEX_MVIEW     (1 << 4)  ///< MailboxView has changed
#define NT_INDEX_MAILBOX   (1 << 5)  ///< Mailbox has changed
#define NT_INDEX_EMAIL     (1 << 6)  ///< Email has changed

typedef uint8_t CheckFlags;       ///< Flags, e.g. #CHECK_IN_MAILBOX
#define CHECK_NO_FLAGS         0  ///< No flags are set
#define CHECK_IN_MAILBOX (1 << 0) ///< Is there a mailbox open?
#define CHECK_MSGCOUNT   (1 << 1) ///< Are there any messages?
#define CHECK_VISIBLE    (1 << 2) ///< Is the selected message visible in the index?
#define CHECK_READONLY   (1 << 3) ///< Is the mailbox readonly?
#define CHECK_ATTACH     (1 << 4) ///< Is the user in message-attach mode?

struct AttrColor *index_color(struct Menu *menu, int line);
void index_make_entry(struct Menu *menu, char *buf, size_t buflen, int line);
void mutt_draw_statusline(struct MuttWindow *win, int cols, const char *buf, size_t buflen);
struct Mailbox *mutt_index_menu(struct MuttWindow *dlg, struct Mailbox *m);
void mutt_set_header_color(struct Mailbox *m, struct Email *e);
void mutt_update_index(struct Menu *menu, struct MailboxView *mv, enum MxStatus check, int oldcount, struct IndexSharedData *shared);
struct MuttWindow *index_pager_init(void);
int mutt_dlgindex_observer(struct NotifyCallback *nc);
bool check_acl(struct Mailbox *m, AclFlags acl, const char *msg);
int ci_next_undeleted(struct Mailbox *m, int msgno, bool uncollapse);
void update_index(struct Menu *menu, struct MailboxView *mv, enum MxStatus check, int oldcount, const struct IndexSharedData *shared);
void change_folder_mailbox(struct Menu *menu, struct Mailbox *m, int *oldcount, struct IndexSharedData *shared, bool read_only);
void collapse_all(struct MailboxView *mv, struct Menu *menu, int toggle);
void change_folder_string(struct Menu *menu, char *buf, size_t buflen, int *oldcount, struct IndexSharedData *shared, bool read_only);
int ci_previous_undeleted(struct Mailbox *m, int msgno, bool uncollapse);
int ci_first_message(struct Mailbox *m);
void resort_index(struct MailboxView *mv, struct Menu *menu);
int mx_toggle_write(struct Mailbox *m);
extern const struct Mapping IndexNewsHelp[];
struct Mailbox *change_folder_notmuch(struct Menu *menu, char *buf, int buflen, int *oldcount, struct IndexSharedData *shared, bool read_only);
struct Mailbox *get_current_mailbox(void);
struct Menu *get_current_menu(void);
void dlg_change_folder(struct MuttWindow *dlg, struct Mailbox *m);

#endif /* MUTT_INDEX_LIB_H */
