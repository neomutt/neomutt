/**
 * @file
 * Use inotify to monitor files/dirs for change
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MONITOR_INOTIFY_H
#define MUTT_MONITOR_INOTIFY_H

#include <poll.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * struct Watch - Watch a file/directory
 */
struct Watch
{
  dev_t      st_dev;    ///< Device Number
  ino_t      st_ino;    ///< Inode Number
  int        wd;        ///< Monitor Watch Descriptor
  monitor_t *cb;        ///< Callback function
  void *     wdata;     ///< Private data for callback function
};
ARRAY_HEAD(WatchArray, struct Watch *);

/**
 * struct Monitor - Filesystem Monitor
 */
struct Monitor
{
  int fd_inotify;               ///< Inotify file descriptor
  struct pollfd polls[2];       ///< File descriptors to monitor
  struct WatchArray watches;    ///< All Watches
};

#endif /* MUTT_MONITOR_INOTIFY_H */
