/**
 * @file
 * GUI component for displaying/selecting items from a list
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_BROWSER_H
#define MUTT_BROWSER_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct Buffer;

/* These Config Variables are only used in browser.c */
extern bool  C_BrowserAbbreviateMailboxes;
extern char *C_FolderFormat;
extern short C_SortBrowser;

typedef uint8_t SelectFileFlags;  ///< Flags for mutt_select_file(), e.g. #MUTT_SEL_MAILBOX
#define MUTT_SEL_NO_FLAGS      0  ///< No flags are set
#define MUTT_SEL_MAILBOX (1 << 0) ///< Select a mailbox
#define MUTT_SEL_MULTI   (1 << 1) ///< Multi-selection is enabled
#define MUTT_SEL_FOLDER  (1 << 2) ///< Select a local directory

/**
 * struct Folder - A folder/dir in the browser
 */
struct Folder
{
  struct FolderFile *ff;
  int num;
};

/**
 * struct FolderFile - Browser entry representing a folder/dir
 */
struct FolderFile
{
  mode_t mode;
  off_t size;
  time_t mtime;
  uid_t uid;
  gid_t gid;
  nlink_t nlink;

  char *name;
  char *desc;

  bool has_new_mail; ///< true if mailbox has "new mail"
  int msg_count;     ///< total number of messages
  int msg_unread;    ///< number of unread messages

#ifdef USE_IMAP
  char delim;

  bool imap : 1;
  bool selectable : 1;
  bool inferiors : 1;
#endif
  bool has_mailbox : 1;
  bool local : 1; ///< folder is on local filesystem
  bool tagged : 1;
#ifdef USE_NNTP
  struct NntpMboxData *nd;
#endif
};

ARRAY_HEAD(BrowserStateEntry, struct FolderFile);

/**
 * struct BrowserState - State of the file/mailbox browser
 */
struct BrowserState
{
  struct BrowserStateEntry entry;
#ifdef USE_IMAP
  bool imap_browse;
  char *folder;
#endif
};

void mutt_select_file(char *file, size_t filelen, SelectFileFlags flags, char ***files, int *numfiles);
void mutt_buffer_select_file(struct Buffer *f, SelectFileFlags flags, char ***files, int *numfiles);
void mutt_browser_select_dir(const char *f);
void mutt_browser_cleanup(void);

#endif /* MUTT_BROWSER_H */
