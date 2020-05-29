/**
 * @file
 * GUI display the mailboxes in a side panel
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

#ifndef MUTT_SIDEBAR_SIDEBAR_PRIVATE_H
#define MUTT_SIDEBAR_SIDEBAR_PRIVATE_H

#include <stdbool.h>

struct NotifyCallback;

extern struct ListHead SidebarWhitelist;

extern int EntryCount;
extern int EntryLen;
extern struct SbEntry **Entries;

extern int TopIndex;
extern int OpnIndex;
extern int HilIndex;
extern int BotIndex;

/**
 * struct SbEntry - Info about folders in the sidebar
 */
struct SbEntry
{
  char box[256];           ///< Mailbox path (possibly abbreviated)
  int depth;               ///< Indentation depth
  struct Mailbox *mailbox; ///< Mailbox this represents
  bool is_hidden;          ///< Don't show, e.g. $sidebar_new_mail_only
};

/**
 * enum DivType - Source of the sidebar divider character
 */
enum DivType
{
  SB_DIV_USER,  ///< User configured using $sidebar_divider_char
  SB_DIV_ASCII, ///< An ASCII vertical bar (pipe)
  SB_DIV_UTF8,  ///< A unicode line-drawing character
};

// observer.c
int sb_observer(struct NotifyCallback *nc);

#endif /* MUTT_SIDEBAR_SIDEBAR_PRIVATE_H */
