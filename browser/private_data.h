/**
 * @file
 * Private state data for the Browser
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_BROWSER_PRIVATE_DATA_H
#define MUTT_BROWSER_PRIVATE_DATA_H

#include <stdbool.h>
#include <limits.h>
#include "lib.h"

/**
 * struct BrowserPrivateData - Private state data for the Browser
 */
struct BrowserPrivateData
{
  // Parameters passed to buf_select_file()
  struct Buffer *file;            ///< Buffer for the result
  struct Mailbox *mailbox;        ///< Mailbox
  char ***files;                  ///< Array of selected files
  int *numfiles;                  ///< Number of selected files

  // State of the browser
  struct BrowserState state;      ///< State containing list of files/dir/mailboxes
  struct Menu *menu;              ///< Menu
  bool kill_prefix;               ///< Prefix is in use
  bool multiple;                  ///< Allow multiple selections
  bool folder;                    ///< Select folders
  char goto_swapper[PATH_MAX];    ///< Saved path after `<goto-folder>`
  struct Buffer *OldLastDir;      ///< Previous to last dir
  struct Buffer *prefix;          ///< Folder prefix string
  int last_selected_mailbox;      ///< Index of last selected Mailbox
  struct MuttWindow *sbar;        ///< Status Bar
  struct MuttWindow *win_browser; ///< Browser Window
  bool done;                      ///< Should we close the Dialog?
};

void                       browser_private_data_free(struct BrowserPrivateData **ptr);
struct BrowserPrivateData *browser_private_data_new (void);

#endif /* MUTT_BROWSER_PRIVATE_DATA_H */
