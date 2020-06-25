/**
 * @file
 * Dummy code for working around build problems
 *
 * @authors
 * Copyright (C) 2018 Naveen Nathan <naveen@lastninja.net>
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

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "core/lib.h"

struct Address;
struct Body;
struct Buffer;
struct Email;
struct EnterState;
struct Envelope;
struct Mailbox;
struct Message;
struct Pager;
struct Pattern;
struct Progress;
struct State;

struct KeyEvent
{
  int ch; ///< raw key pressed
  int op; ///< function op
};

bool g_addr_is_user = false;
int g_body_parts = 1;
bool g_is_mail_list = false;
bool g_is_subscribed_list = false;
const char *g_myvar = "hello";
short AbortKey;

enum MenuType
{
  mt_dummy
};

typedef uint8_t MuttFormatFlags;
typedef uint16_t CompletionFlags;
typedef uint16_t PagerFlags;
typedef uint8_t SelectFileFlags;

typedef const char *(format_t)(char *buf, size_t buflen, size_t col, int cols,
                               char op, const char *src, const char *prec,
                               const char *if_str, const char *else_str,
                               intptr_t data, MuttFormatFlags flags);

int crypt_valid_passphrase(int flags)
{
  return 0;
}

bool imap_search(struct Mailbox *m, const struct Pattern *pat)
{
  return false;
}

bool mutt_addr_is_user(struct Address *addr)
{
  return g_addr_is_user;
}

struct Address *mutt_alias_reverse_lookup(struct Address *a)
{
  return NULL;
}

int mutt_body_handler(struct Body *b, struct State *s)
{
  return -1;
}

void mutt_clear_error(void)
{
}

int mutt_copy_header(FILE *in, struct Email *e, FILE *out, int flags, const char *prefix)
{
  return -1;
}

int mutt_count_body_parts(struct Mailbox *m, struct Email *e)
{
  return g_body_parts;
}

bool mutt_is_mail_list(struct Address *addr)
{
  return g_is_mail_list;
}

bool mutt_is_subscribed_list(struct Address *addr)
{
  return g_is_subscribed_list;
}

void mutt_parse_mime_message(struct Mailbox *m, struct Email *e)
{
}

void mutt_progress_init(struct Progress *progress, const char *msg, int type, size_t size)
{
}
void mutt_progress_update(struct Progress *progress, long pos, int percent)
{
}

void mutt_set_flag_update(struct Mailbox *m, struct Email *e, int flag, bool bf, bool upd_mbox)
{
}

int mx_msg_close(struct Mailbox *m, struct Message **msg)
{
  return 0;
}

struct Message *mx_msg_open(struct Mailbox *m, int msgno)
{
  return NULL;
}

int mx_msg_padding_size(struct Mailbox *m)
{
  return 0;
}

const char *myvar_get(const char *var)
{
  return g_myvar;
}

struct Email *mutt_get_virt_email(struct Mailbox *m, int vnum)
{
  if (!m || !m->emails || !m->v2r)
    return NULL;

  if ((vnum < 0) || (vnum >= m->vcount))
    return NULL;

  int inum = m->v2r[vnum];
  if ((inum < 0) || (inum >= m->msg_count))
    return NULL;

  return m->emails[inum];
}

void mutt_buffer_mktemp_full(struct Buffer *buf, const char *prefix,
                             const char *suffix, const char *src, int line)
{
}

int mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach,
                             int mode, bool privacy, bool hide_protected_subject)
{
  return 0;
}

void mutt_expando_format(char *buf, size_t buflen, size_t col, int cols, const char *src,
                         format_t *callback, intptr_t data, MuttFormatFlags flags)
{
}

struct Menu *mutt_menu_new(enum MenuType type)
{
  return NULL;
}

void mutt_menu_pop_current(struct Menu *menu)
{
}

int mutt_menu_loop(struct Menu *menu)
{
  return 0;
}

void mutt_menu_current_redraw(void)
{
}

int mutt_enter_string_full(char *buf, size_t buflen, int col,
                           CompletionFlags flags, bool multiple, char ***files,
                           int *numfiles, struct EnterState *state)
{
  return 0;
}

void mutt_resize_screen(void)
{
}

char *mutt_compile_help(char *buf, size_t buflen, enum MenuType menu,
                        const struct Mapping *items)
{
  return NULL;
}

void mutt_menu_push_current(struct Menu *menu)
{
}

struct EnterState *mutt_enter_state_new(void)
{
  return NULL;
}

void mutt_enter_state_free(struct EnterState **ptr)
{
}

void mutt_menu_free(struct Menu **ptr)
{
}

int mutt_pager(const char *banner, const char *fname, PagerFlags flags, struct Pager *extra)
{
  return 0;
}

int mutt_monitor_poll(void)
{
  return 0;
}

int mutt_system(const char *cmd)
{
  return 0;
}

void mutt_buffer_select_file(struct Buffer *file, SelectFileFlags flags,
                             char ***files, int *numfiles)
{
}
