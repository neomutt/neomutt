/**
 * @file
 * Monitor files for changes
 *
 * @authors
 * Copyright (C) 2018 Gero Treuner <gero@70t.de>
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

#ifndef MUTT_MONITOR_H
#define MUTT_MONITOR_H

#include <stdbool.h>

struct Mailbox;

extern bool MonitorFilesChanged;   ///< true after a monitored file has changed
extern bool MonitorContextChanged; ///< true after the current mailbox has changed

int mutt_monitor_add(struct Mailbox *m);
int mutt_monitor_remove(struct Mailbox *m);
int mutt_monitor_poll(void);

#endif /* MUTT_MONITOR_H */
