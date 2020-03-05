/**
 * @file
 * Fake mailbox backend for local files and directories
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

/**
 * @page local Fake mailbox backend for local files and directories
 *
 * Fake mailbox backend for local files and directories
 */

#include "config.h"
#include "mx.h"

/**
 * local_path2_canon - Canonicalise a Mailbox path - Implements MxOps::path2_canon
 */
int local_path2_canon(struct Path *path)
{
  return -1;
}

/**
 * local_path2_compare - Compare two Mailbox paths - Implements MxOps::path2_compare
 */
int local_path2_compare(struct Path *path1, struct Path *local_path2)
{
  return -1;
}

/**
 * local_path2_parent - Find the parent of a Mailbox path - Implements MxOps::path2_parent
 */
int local_path2_parent(const struct Path *path, struct Path **parent)
{
  return -1;
}

/**
 * local_path2_pretty - Abbreviate a Mailbox path - Implements MxOps::path2_pretty
 */
int local_path2_pretty(struct Path *path, const char *folder)
{
  return -1;
}

/**
 * local_path2_probe - Does this Mailbox type recognise this path? - Implements MxOps::path2_probe
 */
int local_path2_probe(struct Path *path, const struct stat *st)
{
  return -1;
}

/**
 * local_path2_tidy - Tidy a Mailbox path - Implements MxOps::path2_tidy
 */
int local_path2_tidy(struct Path *path)
{
  return -1;
}

// clang-format on
/**
 * MxLocalFileOps - Local File - Implements ::MxOps
 */
struct MxOps MxLocalFileOps = {
  .type             = MUTT_LOCAL_FILE,
  .name             = "file",
  .is_local         = true,
  .ac_find          = NULL,
  .ac_add           = NULL,
  .mbox_open        = NULL,
  .mbox_open_append = NULL,
  .mbox_check       = NULL,
  .mbox_check_stats = NULL,
  .mbox_sync        = NULL,
  .mbox_close       = NULL,
  .msg_open         = NULL,
  .msg_open_new     = NULL,
  .msg_commit       = NULL,
  .msg_close        = NULL,
  .msg_padding_size = NULL,
  .msg_save_hcache  = NULL,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = NULL,
  .path_canon       = NULL,
  .path_parent      = NULL,
  .path2_canon      = local_path2_canon,
  .path2_compare    = local_path2_compare,
  .path2_parent     = local_path2_parent,
  .path2_pretty     = local_path2_pretty,
  .path2_probe      = local_path2_probe,
  .path2_tidy       = local_path2_tidy,
};

/**
 * MxLocalDirOps - Local Directory - Implements ::MxOps
 */
struct MxOps MxLocalDirOps = {
  .type             = MUTT_LOCAL_DIR,
  .name             = "directory",
  .is_local         = true,
  .ac_find          = NULL,
  .ac_add           = NULL,
  .mbox_open        = NULL,
  .mbox_open_append = NULL,
  .mbox_check       = NULL,
  .mbox_check_stats = NULL,
  .mbox_sync        = NULL,
  .mbox_close       = NULL,
  .msg_open         = NULL,
  .msg_open_new     = NULL,
  .msg_commit       = NULL,
  .msg_close        = NULL,
  .msg_padding_size = NULL,
  .msg_save_hcache  = NULL,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = NULL,
  .path_canon       = NULL,
  .path_parent      = NULL,
  .path2_canon      = local_path2_canon,
  .path2_compare    = local_path2_compare,
  .path2_parent     = local_path2_parent,
  .path2_pretty     = local_path2_pretty,
  .path2_probe      = local_path2_probe,
  .path2_tidy       = local_path2_tidy,
};
// clang-format on
