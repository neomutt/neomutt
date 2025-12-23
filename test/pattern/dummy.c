/**
 * @file
 * Dummy code for working around build problems
 *
 * @authors
 * Copyright (C) 2019 Naveen Nathan <naveen@lastninja.net>
 * Copyright (C) 2019-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020-2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Rayford Shireman
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "core/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#include "mview.h"

struct Address;
struct Body;
struct Buffer;
struct ConfigSubset;
struct Email;
struct Envelope;
struct Mapping;
struct MuttWindow;
struct Pager;
struct Pattern;
struct State;
struct TagList;

bool g_addr_is_user = false;
bool g_is_mail_list = false;
bool g_is_subscribed_list = false;
bool OptForceRefresh;
bool OptKeepQuiet;
bool OptGui;

const struct MenuFuncOp OpPgp[] = {
  { NULL, OP_NULL },
};
const struct MenuFuncOp OpSmime[] = {
  { NULL, OP_NULL },
};

typedef uint8_t MuttFormatFlags;
typedef uint16_t CompletionFlags;
typedef uint16_t PagerFlags;
typedef uint8_t SelectFileFlags;

typedef const char *(*format_t)(char *buf, size_t buflen, size_t col, int cols,
                                char op, const char *src, const char *prec,
                                const char *if_str, const char *else_str,
                                intptr_t data, MuttFormatFlags flags);

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

int mutt_copy_header(FILE *in, struct Email *e, FILE *out,
                     CopyHeaderFlags chflags, const char *prefix, int wraplen)
{
  return -1;
}

bool mutt_is_mail_list(struct Address *addr)
{
  return g_is_mail_list;
}

bool mutt_is_subscribed_list(struct Address *addr)
{
  return g_is_subscribed_list;
}

void mutt_set_flag(struct Mailbox *m, struct Email *e, int flag, bool bf, bool upd_mbox)
{
}

int mx_msg_close(struct Mailbox *m, struct Message **msg)
{
  return 0;
}

struct Message *mx_msg_open(struct Mailbox *m, struct Email *e)
{
  return NULL;
}

int mx_msg_padding_size(struct Mailbox *m)
{
  return 0;
}

const char *myvar_get(const char *var)
{
  return NULL;
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

void mutt_expando_format(char *buf, size_t buflen, size_t col, int cols, const char *src,
                         format_t *callback, intptr_t data, MuttFormatFlags flags)
{
}

void menu_pop_current(struct Menu *menu)
{
}

int menu_loop(struct Menu *menu)
{
  return 0;
}

void menu_current_redraw(void)
{
}

void mutt_resize_screen(void)
{
}

void menu_push_current(struct Menu *menu)
{
}

int mutt_monitor_poll(void)
{
  return 0;
}

int mutt_system(const char *cmd)
{
  return 0;
}

struct Mailbox *mview_mailbox(struct MailboxView *mv)
{
  return mv ? mv->mailbox : NULL;
}

int global_function_dispatcher(struct MuttWindow *win, int op)
{
  return 0;
}

int mutt_complete(char *buf, size_t buflen)
{
  return 0;
}

void mutt_help(enum MenuType menu)
{
}

struct Mailbox *mutt_mailbox_next(struct Mailbox *m_cur, struct Buffer *s)
{
  return NULL;
}

void mutt_select_file(char *file, size_t filelen, SelectFileFlags flags,
                      struct Mailbox *m, char ***files, int *numfiles)
{
}

void simple_dialog_free(struct MuttWindow **ptr)
{
}

struct SimpleDialogWindows simple_dialog_new(enum MenuType mtype, enum WindowType wtype,
                                             const struct Mapping *help_data)
{
  struct SimpleDialogWindows ret = { 0 };
  return ret;
}
