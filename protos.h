/**
 * @file
 * Prototypes for many functions
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
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

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "config/lib.h"

struct Context;
struct EnterState;
struct Envelope;
struct Email;
struct EmailList;
struct Mailbox;

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

int mutt_ev_message(struct Mailbox *m, struct EmailList *el, enum EvMessage action);

int mutt_system(const char *cmd);

int mutt_set_xdg_path(enum XdgType type, char *buf, size_t bufsize);
void mutt_help(int menu);
void mutt_make_help(char *d, size_t dlen, const char *txt, int menu, int op);
void mutt_set_flag_update(struct Mailbox *m, struct Email *e, int flag, bool bf, bool upd_mbox);
#define mutt_set_flag(a, b, c, d) mutt_set_flag_update(a, b, c, d, true)
void mutt_signal_init(void);
void mutt_tag_set_flag(int flag, int bf);
int mutt_change_flag(struct Email *e, int bf);

int mutt_complete(char *buf, size_t buflen);
int mutt_prepare_template(FILE *fp, struct Mailbox *m, struct Email *newhdr, struct Email *e, bool resend);
int mutt_enter_string(char *buf, size_t buflen, int col, int flags);
int mutt_enter_string_full(char *buf, size_t buflen, int col, int flags, bool multiple,
                       char ***files, int *numfiles, struct EnterState *state);
int mutt_get_postponed(struct Context *ctx, struct Email *e, struct Email **cur, char *fcc, size_t fcclen);
int mutt_parse_crypt_hdr(const char *p, int set_empty_signas, int crypt_app);
int mutt_num_postponed(struct Mailbox *m, bool force);
int mutt_thread_set_flag(struct Email *e, int flag, int bf, int subthread);
void mutt_update_num_postponed(void);
int url_parse_mailto(struct Envelope *e, char **body, const char *src);
int mutt_is_quote_line(char *buf, regmatch_t *pmatch);

#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b);
#endif

bool mutt_reply_listener(const struct ConfigSet *cs, struct HashElem *he,
                         const char *name, enum ConfigEvent ev);

#endif /* MUTT_PROTOS_H */
