/**
 * @file
 * GUI manage the main index (list of emails)
 *
 * @authors
 * Copyright (C) 2018-2026 Richard Russon <rich@flatcap.org>
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
 * | File                   | Description                   |
 * | :--------------------- | :---------------------------- |
 * | index/commands.c       | @subpage index_commands       |
 * | index/config.c         | @subpage index_config         |
 * | index/dlg_index.c      | @subpage index_dlg_index      |
 * | index/dlg_list.c       | Mailing-list action dialog    |
 * | index/expando_index.c  | @subpage index_expando_index  |
 * | index/expando_status.c | @subpage index_expando_status |
 * | index/functions.c      | @subpage index_functions      |
 * | index/ibar.c           | @subpage index_ibar           |
 * | index/index.c          | @subpage index_index          |
 * | index/ipanel.c         | @subpage index_ipanel         |
 * | index/module.c         | @subpage index_module         |
 * | index/private_data.c   | @subpage index_private_data   |
 * | index/shared_data.c    | @subpage index_shared_data    |
 * | index/status.c         | @subpage index_status         |
 * | index/subjectrx.c      | @subpage index_subjectrx      |
 */

#ifndef MUTT_INDEX_LIB_H
#define MUTT_INDEX_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "expando/lib.h"   // IWYU pragma: keep
#include "expando_index.h" // IWYU pragma: keep
#include "functions.h"     // IWYU pragma: keep
#include "shared_data.h"   // IWYU pragma: keep
#include "status.h"        // IWYU pragma: keep
#include "subjectrx.h"     // IWYU pragma: keep

struct Email;
struct MailboxView;
struct Menu;
struct MuttWindow;
struct SubMenu;

// Observers of #NT_INDEX will be passed an #IndexSharedData.
/**
 * enum NotifyIndexFlag - Flags, e.g. #NT_INDEX_ACCOUNT
 */
enum NotifyIndexFlag
{
  NT_INDEX_NONE    =       0,  ///< No flags are set
  NT_INDEX_ADD     = 1U << 0,  ///< New Index Shared Data has been created
  NT_INDEX_DELETE  = 1U << 1,  ///< Index Shared Data is about to be freed
  NT_INDEX_SUBSET  = 1U << 2,  ///< Config Subset has changed
  NT_INDEX_ACCOUNT = 1U << 3,  ///< Account has changed
  NT_INDEX_MVIEW   = 1U << 4,  ///< MailboxView has changed
  NT_INDEX_MAILBOX = 1U << 5,  ///< Mailbox has changed
  NT_INDEX_EMAIL   = 1U << 6,  ///< Email has changed
};
typedef uint8_t NotifyIndex;

/**
 * enum CheckFlag - Flags, e.g. #CHECK_IN_MAILBOX
 */
enum CheckFlag
{
  CHECK_NONE       =       0,  ///< No flags are set
  CHECK_IN_MAILBOX = 1U << 0,  ///< Is there a mailbox open?
  CHECK_MSGCOUNT   = 1U << 1,  ///< Are there any messages?
  CHECK_VISIBLE    = 1U << 2,  ///< Is the selected message visible in the index?
  CHECK_READONLY   = 1U << 3,  ///< Is the mailbox readonly?
  CHECK_ATTACH     = 1U << 4,  ///< Is the user in message-attach mode?
};
typedef uint8_t CheckFlags;

/**
 * enum CollapseMode - Action to perform on a Thread
 */
enum CollapseMode
{
  COLLAPSE_MODE_TOGGLE, ///< Toggle collapsed state
  COLLAPSE_MODE_CLOSE,  ///< Collapse all threads
  COLLAPSE_MODE_OPEN,   ///< Open all threads
};

extern const struct Mapping IndexNewsHelp[];

extern const struct ExpandoDefinition StatusFormatDef[];

void index_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic);

void                    change_folder_mailbox   (struct Menu *menu, struct Mailbox *m, int *oldcount, struct IndexSharedData *shared, bool read_only);
struct Mailbox *        change_folder_notmuch   (struct Menu *menu, char *buf, int buflen, int *oldcount, struct IndexSharedData *shared, bool read_only);
void                    change_folder_string    (struct Menu *menu, struct Buffer *buf, int *oldcount, struct IndexSharedData *shared, bool read_only);
bool                    check_acl               (struct Mailbox *m, AclFlags acl, const char *msg);
void                    collapse_all            (struct MailboxView *mv, struct Menu *menu, enum CollapseMode mode);
void                    dlg_list                (struct Mailbox *m, struct Email *e);
struct Mailbox *        dlg_index               (struct MuttWindow *dlg, struct Mailbox *m);
int                     find_first_message      (struct MailboxView *mv);
int                     find_next_undeleted     (struct MailboxView *mv, int msgno, bool uncollapse);
int                     find_previous_undeleted (struct MailboxView *mv, int msgno, bool uncollapse);
struct Mailbox *        get_current_mailbox     (void);
struct MailboxView *    get_current_mailbox_view(void);
void                    index_change_folder     (struct MuttWindow *dlg, struct Mailbox *m);
const struct AttrColor *index_color             (struct Menu *menu, int line);
int                     index_make_entry        (struct Menu *menu, int line, int max_cols, struct Buffer *buf);
struct MuttWindow *     index_pager_init        (void);
int                     mutt_dlgindex_observer  (struct NotifyCallback *nc);
void                    mutt_draw_statusline    (struct MuttWindow *win, int max_cols, const char *buf, size_t buflen);
void                    email_set_color         (struct Mailbox *m, struct Email *e);
void                    resort_index            (struct MailboxView *mv, struct Menu *menu);
void                    update_index            (struct Menu *menu, struct MailboxView *mv, enum MxStatus check, int oldcount, const struct IndexSharedData *shared);

int mutt_make_string(struct Buffer *buf, size_t max_cols, const struct Expando *exp,
                     struct Mailbox *m, int inpgr, struct Email *e,
                     MuttFormatFlags flags, const char *progress);

#endif /* MUTT_INDEX_LIB_H */
