/**
 * @file
 * Monitor files for changes
 *
 * @authors
 * Copyright (C) 2018 Gero Treuer <gero@70t.de>
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

#ifndef _MUTT_MONITOR_H
#define _MUTT_MONITOR_H

extern int MonitorFilesChanged;

struct Mailbox;

int mutt_monitor_add(struct Mailbox *b);
int mutt_monitor_remove(struct Mailbox *b);
void mutt_monitor_set_poll_timeout(int timeout);
int mutt_monitor_get_poll_timeout(void);
int mutt_monitor_poll(void);

#endif /* _MUTT_MONITOR_H */
