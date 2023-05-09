/**
 * @file
 * Dummy code for working around build problems
 *
 * @authors
 * Copyright (C) 2018 Naveen Nathan <naveen@lastninja.net>
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

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "history/lib.h"
#include "menu/lib.h"
#include "mview.h"

struct Address;
struct Body;
struct Email;
struct Envelope;
struct Keymap;
struct Pager;
struct Pattern;

struct KeyEvent
{
  int ch; ///< raw key pressed
  int op; ///< function op
};

enum WindowType
{
  // Structural Windows
  WT_ROOT,        ///< Parent of All Windows
  WT_CONTAINER,   ///< Invisible shaping container Window
  WT_ALL_DIALOGS, ///< Container for All Dialogs (nested Windows)

  // Dialogs (nested Windows) displayed to the user
  WT_DLG_ALIAS,       ///< Alias Dialog,       dlg_select_alias()
  WT_DLG_ATTACH,      ///< Attach Dialog,      dlg_select_attachment()
  WT_DLG_AUTOCRYPT,   ///< Autocrypt Dialog,   dlg_select_autocrypt_account()
  WT_DLG_BROWSER,     ///< Browser Dialog,     buf_select_file()
  WT_DLG_CERTIFICATE, ///< Certificate Dialog, dlg_verify_certificate()
  WT_DLG_COMPOSE,     ///< Compose Dialog,     mutt_compose_menu()
  WT_DLG_CRYPT_GPGME, ///< Crypt-GPGME Dialog, dlg_select_gpgme_key()
  WT_DLG_DO_PAGER,    ///< Pager Dialog,       mutt_do_pager()
  WT_DLG_HISTORY,     ///< History Dialog,     dlg_select_history()
  WT_DLG_INDEX,       ///< Index Dialog,       index_pager_init()
  WT_DLG_PATTERN,     ///< Pattern Dialog,     create_pattern_menu()
  WT_DLG_PGP,         ///< Pgp Dialog,         dlg_select_pgp_key()
  WT_DLG_POSTPONE,    ///< Postpone Dialog,    dlg_select_postponed_email()
  WT_DLG_QUERY,       ///< Query Dialog,       dlg_select_query()
  WT_DLG_REMAILER,    ///< Remailer Dialog,    dlg_mixmaster()
  WT_DLG_SMIME,       ///< Smime Dialog,       dlg_select_smime_key()

  // Common Windows
  WT_CUSTOM,     ///< Window with a custom drawing function
  WT_HELP_BAR,   ///< Help Bar containing list of useful key bindings
  WT_INDEX,      ///< A panel containing the Index Window
  WT_MENU,       ///< An Window containing a Menu
  WT_MESSAGE,    ///< Window for messages/errors and command entry
  WT_PAGER,      ///< A panel containing the Pager Window
  WT_SIDEBAR,    ///< Side panel containing Accounts or groups of data
  WT_STATUS_BAR, ///< Status Bar containing extra info about the Index/Pager/etc
};

bool g_addr_is_user = false;
int g_body_parts = 1;
bool g_is_mail_list = false;
bool g_is_subscribed_list = false;
short AbortKey;
bool OptForceRefresh;
bool OptIgnoreMacroEvents;
bool OptKeepQuiet;
bool OptNoCurses;
bool OptSearchInvalid;
bool OptSearchReverse;

typedef uint8_t MuttFormatFlags;
typedef uint16_t CompletionFlags;
typedef uint16_t PagerFlags;
typedef uint8_t SelectFileFlags;

typedef const char *(format_t) (char *buf, size_t buflen, size_t col, int cols,
                                char op, const char *src, const char *prec,
                                const char *if_str, const char *else_str,
                                intptr_t data, MuttFormatFlags flags);

struct Address *alias_reverse_lookup(const struct Address *addr)
{
  return NULL;
}

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

int mutt_count_body_parts(struct Mailbox *m, struct Email *e, struct Message *msg)
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

void mutt_parse_mime_message(struct Mailbox *m, struct Email *e, FILE *msg)
{
}

void mutt_str_pretty_size(char *buf, size_t buflen, size_t num)
{
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

int mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach,
                             int mode, bool privacy, bool hide_protected_subject)
{
  return 0;
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

int km_expand_key(char *s, size_t len, struct Keymap *map)
{
  return 0;
}

struct Keymap *km_find_func(enum MenuType menu, int func)
{
  return NULL;
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

void buf_select_file(struct Buffer *file, SelectFileFlags flags, char ***files, int *numfiles)
{
}

struct Mailbox *mview_mailbox(struct MailboxView *mv)
{
  return mv ? mv->mailbox : NULL;
}

int alias_complete(char *buf, size_t buflen, struct ConfigSubset *sub)
{
  return 0;
}

int global_function_dispatcher(struct MuttWindow *win, int op)
{
  return 0;
}

struct KeyEvent km_dokey_event(enum MenuType mtype)
{
  struct KeyEvent ke = { 0 };
  return ke;
}

void km_error_key(enum MenuType mtype)
{
}

int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  return 0;
}

int mutt_complete(char *buf, size_t buflen)
{
  return 0;
}

char *mutt_expand_path(char *buf, size_t buflen)
{
  return NULL;
}

void mutt_help(enum MenuType menu)
{
}

void mutt_hist_complete(char *buf, size_t buflen, enum HistoryClass hclass)
{
}

int mutt_label_complete(char *buf, size_t buflen, int numtabs)
{
  return 0;
}

struct Mailbox *mutt_mailbox_next(struct Mailbox *m_cur, struct Buffer *s)
{
  return NULL;
}

bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  return false;
}

bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs)
{
  return false;
}

void mutt_select_file(char *file, size_t filelen, SelectFileFlags flags,
                      struct Mailbox *m, char ***files, int *numfiles)
{
}

int mutt_var_value_complete(char *buf, size_t buflen, int pos)
{
  return 0;
}

const char *opcodes_get_name(int op)
{
  return NULL;
}

int query_complete(struct Buffer *buf, struct ConfigSubset *sub)
{
  return 0;
}

void sbar_set_title(struct MuttWindow *win, const char *title)
{
}

void simple_dialog_free(struct MuttWindow **ptr)
{
}

struct MuttWindow *simple_dialog_new(enum MenuType mtype, enum WindowType wtype,
                                     const struct Mapping *help_data)
{
  return NULL;
}
