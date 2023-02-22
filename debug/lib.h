/**
 * @file
 * Convenience wrapper for the debug headers
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page lib_debug Debug Code
 *
 * Debugging code
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | debug/backtrace.c   | @subpage debug_backtrace   |
 * | debug/common.c      | @subpage debug_common      |
 * | debug/email.c       | @subpage debug_email       |
 * | debug/graphviz.c    | @subpage debug_graphviz    |
 * | debug/notify.c      | @subpage debug_notify      |
 * | debug/pager.c       | @subpage debug_pager       |
 * | debug/parse_test.c  | @subpage debug_parse       |
 * | debug/window.c      | @subpage debug_window      |
 */

#ifndef MUTT_DEBUG_LIB_H
#define MUTT_DEBUG_LIB_H

#include <stddef.h>
#include <stdbool.h>
#include "email/lib.h"
#include "core/lib.h"

struct AddressList;
struct AttachCtx;
struct AttachPtr;
struct Buffer;
struct ListHead;
struct MailboxView;
struct NotifyCallback;
struct PagerPrivateData;

// Common
void        add_flag               (struct Buffer *buf, bool is_set, const char *name);
const char *get_content_encoding   (enum ContentEncoding enc);
const char *get_content_disposition(enum ContentDisposition disp);
const char *get_content_type       (enum ContentType type);

// Backtrace
void show_backtrace(void);

// Email
void        add_flag               (struct Buffer *buf, bool is_set, const char *name);
char        body_name              (const struct Body *b);
void        dump_addr_list         (char *buf, size_t buflen, const struct AddressList *al, const char *name);
void        dump_attach            (const struct AttachPtr *att);
void        dump_body              (const struct Body *body);
void        dump_body_next         (struct Buffer *buf, const struct Body *b);
void        dump_body_one_line     (const struct Body *b);
void        dump_email             (const struct Email *e);
void        dump_envelope          (const struct Envelope *env);
void        dump_list_head         (const struct ListHead *list, const char *name);
void        dump_param_list        (const struct ParameterList *pl);
const char *get_content_disposition(enum ContentDisposition disp);
const char *get_content_encoding   (enum ContentEncoding enc);
const char *get_content_type       (enum ContentType type);

// Graphviz
void        add_flag               (struct Buffer *buf, bool is_set, const char *name);
void        dump_graphviz          (const char *title, struct MailboxView *mv);
void        dump_graphviz_attach_ctx(struct AttachCtx *actx);
void        dump_graphviz_body     (struct Body *b);
void        dump_graphviz_email    (struct Email *e);
const char *get_content_disposition(enum ContentDisposition disp);
const char *get_content_encoding   (enum ContentEncoding enc);
const char *get_content_type       (enum ContentType type);

// Notify
int debug_all_observer(struct NotifyCallback *nc);
const char *get_mailbox_type(enum MailboxType type);

// Pager
void dump_pager(struct PagerPrivateData *priv);

// Parse Set
void test_parse_set(void);

// Window
void debug_win_dump(void);

#endif /* MUTT_DEBUG_LIB_H */
