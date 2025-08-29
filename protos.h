/**
 * @file
 * Prototypes for many functions
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PROTOS_H
#define MUTT_PROTOS_H

#include "config.h"
#include <stdbool.h>
#include "mutt.h"
#include "menu/lib.h"

struct Buffer;
struct Email;
struct EmailArray;
struct Mailbox;
struct NotifyCallback;

/**
 * enum XdgType - XDG variable types
 */
enum XdgType
{
  XDG_CONFIG_HOME, ///< XDG home dir: ~/.config
  XDG_CONFIG_DIRS, ///< XDG system dir: /etc/xdg
};

/**
 * enum EvMessage - Edit or View a message
 */
enum EvMessage
{
  EVM_VIEW, ///< View the message
  EVM_EDIT, ///< Edit the message
};

int mutt_ev_message(struct Mailbox *m, struct EmailArray *ea, enum EvMessage action);

int mutt_system(const char *cmd);

int mutt_set_xdg_path(enum XdgType type, struct Buffer *buf);
void mutt_help(enum MenuType menu);
void mutt_set_flag(struct Mailbox *m, struct Email *e, enum MessageType flag, bool bf, bool upd_mbox);
void mutt_signal_init(void);
void mutt_emails_set_flag(struct Mailbox *m, struct EmailArray *ea, enum MessageType flag, bool bf);
int mw_change_flag(struct Mailbox *m, struct EmailArray *ea, bool bf);

int mutt_thread_set_flag(struct Mailbox *m, struct Email *e, enum MessageType flag, bool bf, bool subthread);
extern short PostCount;

#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b);
#endif

int mutt_reply_observer(struct NotifyCallback *nc);

#endif /* MUTT_PROTOS_H */
