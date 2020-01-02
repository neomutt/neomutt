/**
 * @file
 * Definitions of NeoMutt commands
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013,2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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
 * @page mutt_commands Definitions of NeoMutt commands
 *
 * Definitions of NeoMutt commands
 */

#include "config.h"
#include <stddef.h>
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "mutt_commands.h"
#include "command_parse.h"
#include "globals.h"
#include "init.h"
#include "mutt_account.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_lua.h"
#include "score.h"

// clang-format off
const struct Command Commands[] = {
#ifdef USE_SOCKET
  { "account",             mutt_parse_account,     0 },
  { "account-hook",        mutt_parse_hook,        MUTT_ACCOUNT_HOOK },
#endif
  { "alias",               parse_alias,            0 },
  { "alternates",          parse_alternates,       0 },
  { "alternative_order",   parse_stailq,           IP &AlternativeOrderList },
#ifdef USE_COMP_MBOX
  { "append-hook",         mutt_parse_hook,        MUTT_APPEND_HOOK },
#endif
  { "attachments",         parse_attachments,      0 },
  { "auto_view",           parse_stailq,           IP &AutoViewList },
  { "bind",                mutt_parse_bind,        0 },
  { "charset-hook",        mutt_parse_hook,        MUTT_CHARSET_HOOK },
#ifdef USE_COMP_MBOX
  { "close-hook",          mutt_parse_hook,        MUTT_CLOSE_HOOK },
#endif
#ifdef HAVE_COLOR
  { "color",               mutt_parse_color,       0 },
#endif
  { "crypt-hook",          mutt_parse_hook,        MUTT_CRYPT_HOOK },
  { "echo",                parse_echo,             0 },
  { "exec",                mutt_parse_exec,        0 },
  { "fcc-hook",            mutt_parse_hook,        MUTT_FCC_HOOK },
  { "fcc-save-hook",       mutt_parse_hook,        MUTT_FCC_HOOK | MUTT_SAVE_HOOK },
  { "finish",              parse_finish,           0 },
  { "folder-hook",         mutt_parse_hook,        MUTT_FOLDER_HOOK },
  { "group",               parse_group,            MUTT_GROUP },
  { "hdr_order",           parse_stailq,           IP &HeaderOrderList },
  { "iconv-hook",          mutt_parse_hook,        MUTT_ICONV_HOOK },
  { "ifdef",               parse_ifdef,            0 },
  { "ifndef",              parse_ifdef,            1 },
  { "ignore",              parse_ignore,           0 },
  { "index-format-hook",   mutt_parse_idxfmt_hook, 0 },
  { "lists",               parse_lists,            0 },
#ifdef USE_LUA
  { "lua",                 mutt_lua_parse,         0 },
  { "lua-source",          mutt_lua_source_file,   0 },
#endif
  { "macro",               mutt_parse_macro,       0 },
  { "mailboxes",           parse_mailboxes,        0 },
  { "mailto_allow",        parse_stailq,           IP &MailToAllow },
  { "mbox-hook",           mutt_parse_hook,        MUTT_MBOX_HOOK },
  { "message-hook",        mutt_parse_hook,        MUTT_MESSAGE_HOOK },
  { "mime_lookup",         parse_stailq,           IP &MimeLookupList },
  { "mono",                mutt_parse_mono,        0 },
  { "my_hdr",              parse_my_hdr,           0 },
  { "named-mailboxes",     parse_mailboxes,        MUTT_NAMED },
  { "nospam",              parse_spam_list,        MUTT_NOSPAM },
#ifdef USE_COMP_MBOX
  { "open-hook",           mutt_parse_hook,        MUTT_OPEN_HOOK },
#endif
  { "pgp-hook",            mutt_parse_hook,        MUTT_CRYPT_HOOK },
  { "push",                mutt_parse_push,        0 },
  { "reply-hook",          mutt_parse_hook,        MUTT_REPLY_HOOK },
  { "reset",               parse_set,              MUTT_SET_RESET },
  { "save-hook",           mutt_parse_hook,        MUTT_SAVE_HOOK },
  { "score",               mutt_parse_score,       0 },
  { "send-hook",           mutt_parse_hook,        MUTT_SEND_HOOK },
  { "send2-hook",          mutt_parse_hook,        MUTT_SEND2_HOOK },
  { "set",                 parse_set,              MUTT_SET_SET },
  { "setenv",              parse_setenv,           MUTT_SET_SET },
  { "shutdown-hook",       mutt_parse_hook,        MUTT_SHUTDOWN_HOOK | MUTT_GLOBAL_HOOK },
#ifdef USE_SIDEBAR
  { "sidebar_whitelist",   parse_path_list,        IP &SidebarWhitelist },
#endif
  { "source",              parse_source,           0 },
  { "spam",                parse_spam_list,        MUTT_SPAM },
  { "startup-hook",        mutt_parse_hook,        MUTT_STARTUP_HOOK | MUTT_GLOBAL_HOOK },
  { "subjectrx",           parse_subjectrx_list,   0 },
  { "subscribe",           parse_subscribe,        0 },
#ifdef USE_IMAP
  { "subscribe-to",        parse_subscribe_to,     0 },
#endif
  { "tag-formats",         parse_tag_formats,      0 },
  { "tag-transforms",      parse_tag_transforms,   0 },
  { "timeout-hook",        mutt_parse_hook,        MUTT_TIMEOUT_HOOK | MUTT_GLOBAL_HOOK },
  { "toggle",              parse_set,              MUTT_SET_INV },
  { "unaccount",           mutt_parse_unaccount,   0 },
  { "unalias",             parse_unalias,          0 },
  { "unalternates",        parse_unalternates,     0 },
  { "unalternative_order", parse_unstailq,         IP &AlternativeOrderList },
  { "unattachments",       parse_unattachments,    0 },
  { "unauto_view",         parse_unstailq,         IP &AutoViewList },
  { "unbind",              mutt_parse_unbind,      MUTT_UNBIND },
#ifdef HAVE_COLOR
  { "uncolor",             mutt_parse_uncolor,     0 },
#endif
  { "ungroup",             parse_group,            MUTT_UNGROUP },
  { "unhdr_order",         parse_unstailq,         IP &HeaderOrderList },
  { "unhook",              mutt_parse_unhook,      0 },
  { "unignore",            parse_unignore,         0 },
  { "unlists",             parse_unlists,          0 },
  { "unmacro",             mutt_parse_unbind,      MUTT_UNMACRO },
  { "unmailboxes",         parse_unmailboxes,      0 },
  { "unmailto_allow",      parse_unstailq,         IP &MailToAllow },
  { "unmime_lookup",       parse_unstailq,         IP &MimeLookupList },
  { "unmono",              mutt_parse_unmono,      0 },
  { "unmy_hdr",            parse_unmy_hdr,         0 },
  { "unscore",             mutt_parse_unscore,     0 },
  { "unset",               parse_set,              MUTT_SET_UNSET },
  { "unsetenv",            parse_setenv,           MUTT_SET_UNSET },
#ifdef USE_SIDEBAR
  { "unsidebar_whitelist", parse_path_unlist,      IP &SidebarWhitelist },
#endif
  { "unsubjectrx",         parse_unsubjectrx_list, 0 },
  { "unsubscribe",         parse_unsubscribe,      0 },
#ifdef USE_IMAP
  { "unsubscribe-from",    parse_unsubscribe_from, 0 },
#endif
#ifdef USE_NOTMUCH
  { "unvirtual-mailboxes", parse_unmailboxes,      0 },
  { "virtual-mailboxes",   parse_mailboxes,        MUTT_NAMED },
#endif
  { NULL,                  NULL,                   0 },
};
// clang-format on
