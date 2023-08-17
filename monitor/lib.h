/**
 * @file
 * Monitor files/dirs for changes
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

/**
 * @page lib_monitor Monitor files/dirs for changes
 *
 * Monitor files/dirs for changes
 *
 * | File              | Description              |
 * | :---------------- | :----------------------- |
 * | monitor/inotify.c | @subpage monitor_inotify |
 */

#ifndef MUTT_MONITOR_LIB_H
#define MUTT_MONITOR_LIB_H

#include <inttypes.h>

struct Monitor;

typedef uint32_t MonitorEvent;        ///< Filesystem events, e.g. #MONITOR_NO_FLAGS
#define MONITOR_NO_EVENTS          0  ///< No filesystem event
#define MONITOR_XXX         (1 << 17) ///< XXX

/**
 * @defgroup monitor_api Filesystem Monitor API
 *
 * Prototype for a Monitor callback function
 *
 * @param mon   Filesystem Monitor
 * @param wd    Watch Descriptor
 * @param wdata Data for callback function
 */
typedef void monitor_t(struct Monitor *mon, int wd, MonitorEvent me, void *wdata);

struct Monitor *monitor_init(void);
void            monitor_free(struct Monitor **ptr);

int  monitor_watch_dir   (struct Monitor *mon, const char *dir,  monitor_t *cb, void *wdata);
int  monitor_watch_file  (struct Monitor *mon, const char *file, monitor_t *cb, void *wdata);
void monitor_remove_watch(struct Monitor *mon, int wd);
int  monitor_poll        (struct Monitor *mon);

#endif /* MUTT_MONITOR_LIB_H */
