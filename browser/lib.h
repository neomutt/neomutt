/**
 * @file
 * Select a Mailbox from a list
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
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
 * @page lib_browser Mailbox Browser
 *
 * Select a Mailbox from a list
 *
 * | File                   | Description                   |
 * | :--------------------- | :---------------------------- |
 * | browser/complete.c     | @subpage browser_complete     |
 * | browser/config.c       | @subpage browser_config       |
 * | browser/dlg_browser.c  | @subpage browser_dlg_browser  |
 * | browser/expando.c      | @subpage browser_expando      |
 * | browser/functions.c    | @subpage browser_functions    |
 * | browser/module.c       | @subpage browser_module       |
 * | browser/private_data.c | @subpage browser_private_data |
 * | browser/sort.c         | @subpage browser_sorting      |
 */

#ifndef MUTT_BROWSER_LIB_H
#define MUTT_BROWSER_LIB_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include "mutt/lib.h"

struct Mailbox;
struct Menu;
struct MuttWindow;
struct stat;

extern struct Buffer LastDir;
extern struct Buffer LastDirBackup;

typedef uint8_t SelectFileFlags;  ///< Flags for mutt_select_file(), e.g. #MUTT_SEL_MAILBOX
#define MUTT_SEL_NO_FLAGS      0  ///< No flags are set
#define MUTT_SEL_MAILBOX (1 << 0) ///< Select a mailbox
#define MUTT_SEL_MULTI   (1 << 1) ///< Multi-selection is enabled
#define MUTT_SEL_FOLDER  (1 << 2) ///< Select a local directory

extern const struct CompleteOps CompleteFileOps;
extern const struct CompleteOps CompleteMailboxOps;

/**
 * struct Folder - A folder/dir in the browser
 */
struct Folder
{
  struct FolderFile *ff; ///< File / Dir / Mailbox
  int num;               ///< Number in the index
};

/**
 * struct FolderFile - Browser entry representing a folder/dir
 */
struct FolderFile
{
  mode_t mode;             ///< File permissions
  off_t size;              ///< File size
  time_t mtime;            ///< Modification time
  uid_t uid;               ///< File's User ID
  gid_t gid;               ///< File's Group ID
  nlink_t nlink;           ///< Number of hard links

  char *name;              ///< Name of file/dir/mailbox
  char *desc;              ///< Description of mailbox

  bool has_new_mail;       ///< true if mailbox has "new mail"
  int msg_count;           ///< total number of messages
  int msg_unread;          ///< number of unread messages

  char delim;              ///< Path delimiter

  bool imap          : 1;  ///< This is an IMAP folder
  bool selectable    : 1;  ///< Folder can be selected
  bool inferiors     : 1;  ///< Folder has children
  bool has_mailbox   : 1;  ///< This is a mailbox
  bool local         : 1;  ///< Folder is on local filesystem
  bool notify_user   : 1;  ///< User will be notified of new mail
  bool poll_new_mail : 1;  ///< Check mailbox for new mail
  bool tagged        : 1;  ///< Folder is tagged
  struct NntpMboxData *nd; ///< Extra NNTP data

  int gen;                 ///< Unique id, used for (un)sorting
};

ARRAY_HEAD(BrowserEntryArray, struct FolderFile);

/**
 * ExpandoDataFolder - Expando UIDs for the File Browser
 *
 * @sa ED_FOLDER, ExpandoDomain
 */
enum ExpandoDataFolder
{
  ED_FOL_DATE = 1,             ///< FolderFile.mtime
  ED_FOL_DATE_FORMAT,          ///< FolderFile.mtime
  ED_FOL_DATE_STRF,            ///< FolderFile.mtime
  ED_FOL_DESCRIPTION,          ///< FolderFile.desc, FolderFile.name
  ED_FOL_FILENAME,             ///< FolderFile.name
  ED_FOL_FILE_GROUP,           ///< FolderFile.gid
  ED_FOL_FILE_MODE,            ///< FolderFile.move
  ED_FOL_FILE_OWNER,           ///< FolderFile.uid
  ED_FOL_FILE_SIZE,            ///< FolderFile.size
  ED_FOL_FLAGS,                ///< FolderFile.nd (NntpMboxData)
  ED_FOL_FLAGS2,               ///< FolderFile.nd (NntpMboxData)
  ED_FOL_HARD_LINKS,           ///< FolderFile.nlink
  ED_FOL_MESSAGE_COUNT,        ///< FolderFile.msg_count
  ED_FOL_NEWSGROUP,            ///< FolderFile.name
  ED_FOL_NEW_COUNT,            ///< FolderFile.nd (NntpMboxData)
  ED_FOL_NEW_MAIL,             ///< FolderFile.has_new_mail
  ED_FOL_NOTIFY,               ///< FolderFile.notify_user
  ED_FOL_NUMBER,               ///< Folder.num
  ED_FOL_POLL,                 ///< FolderFile.poll_new_mail
  ED_FOL_TAGGED,               ///< FolderFile.tagged
  ED_FOL_UNREAD_COUNT,         ///< FolderFile.msg_unread
};

/**
 * struct BrowserState - State of the file/mailbox browser
 */
struct BrowserState
{
  struct BrowserEntryArray entry; ///< Array of files / dirs / mailboxes
  bool imap_browse; ///< IMAP folder
  char *folder;     ///< Folder name
  bool is_mailbox_list; ///< Viewing mailboxes
};

void dlg_browser(struct Buffer *file, SelectFileFlags flags, struct Mailbox *m, char ***files, int *numfiles);
void mutt_browser_select_dir(const char *f);
void mutt_browser_cleanup(void);

void browser_sort(struct BrowserState *state);
void browser_add_folder(const struct Menu *menu, struct BrowserState *state, const char *name, const char *desc, const struct stat *st, struct Mailbox *m, void *data);
void browser_highlight_default(struct BrowserState *state, struct Menu *menu);
int examine_directory(struct Mailbox *m, struct Menu *menu, struct BrowserState *state, const char *d, const char *prefix);
int examine_mailboxes(struct Mailbox *m, struct Menu *menu, struct BrowserState *state);
void init_menu(struct BrowserState *state, struct Menu *menu, struct Mailbox *m, struct MuttWindow *sbar);
void init_state(struct BrowserState *state);
bool link_is_dir(const char *folder, const char *path);
void destroy_state(struct BrowserState *state);
void dump_state(struct BrowserState *state);

#endif /* MUTT_BROWSER_LIB_H */
