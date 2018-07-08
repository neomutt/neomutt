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

#ifndef _MUTT_BROWSER_H
#define _MUTT_BROWSER_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

/* These Config Variables are only used in browser.c */
extern bool  BrowserAbbreviateMailboxes;
extern char *FolderFormat;
extern char *GroupIndexFormat;
extern char *NewsgroupsCharset;
extern bool  ShowOnlyUnread;
extern short SortBrowser;
extern char *VfolderFormat;

/* flags to mutt_select_file() */
#define MUTT_SEL_BUFFY   (1 << 0)
#define MUTT_SEL_MULTI   (1 << 1)
#define MUTT_SEL_FOLDER  (1 << 2)
#define MUTT_SEL_VFOLDER (1 << 3)

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

  bool new;       /**< true if mailbox has "new mail" */
  int msg_count;  /**< total number of messages */
  int msg_unread; /**< number of unread messages */

#ifdef USE_IMAP
  char delim;

  bool imap : 1;
  bool selectable : 1;
  bool inferiors : 1;
#endif
  bool has_buffy : 1;
  bool local : 1; /**< folder is on local filesystem */
  bool tagged : 1;
#ifdef USE_NNTP
  struct NntpData *nd;
#endif
};

/**
 * struct BrowserState - State of the file/mailbox browser
 */
struct BrowserState
{
  struct FolderFile *entry;
  size_t entrylen; /**< number of real entries */
  unsigned int entrymax; /**< max entry */
#ifdef USE_IMAP
  bool imap_browse;
  char *folder;
  bool noselect : 1;
  bool marked : 1;
  bool unmarked : 1;
#endif
};

void mutt_select_file(char *f, size_t flen, int flags, char ***files, int *numfiles);
void mutt_browser_select_dir(char *f);

#endif /* _MUTT_BROWSER_H */
