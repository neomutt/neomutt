/**
 * @file
 * String processing routines to generate the mail index
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016 Ian Zimmerman <itz@primate.net>
 * Copyright (C) 2016-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017 Stefan Assmann <sassmann@kpanic.de>
 * Copyright (C) 2019 Victor Fernandes <criw@pm.me>
 * Copyright (C) 2019-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021 Ashish Panigrahi <ashish.panigrahi@protonmail.com>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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
 * @page neo_hdrline String processing routines to generate the mail index
 *
 * String processing routines to generate the mail index
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "hdrline.h"
#include "attach/lib.h"
#include "color/lib.h"
#include "expando/lib.h"
#include "ncrypt/lib.h"
#include "hook.h"
#include "maillist.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "sort.h"
#include "subjectrx.h"
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif

const struct ExpandoRenderData IndexRenderData[];

/**
 * struct HdrFormatInfo - Data passed to index_format_str()
 */
struct HdrFormatInfo
{
  struct Mailbox *mailbox;    ///< Current Mailbox
  int msg_in_pager;           ///< Index of Email displayed in the Pager
  struct Email *email;        ///< Current Email
  const char *pager_progress; ///< String representing Pager position through Email
};

/**
 * enum FieldType - Header types
 *
 * Strings for printing headers
 */
enum FieldType
{
  DISP_TO,    ///< To: string
  DISP_CC,    ///< Cc: string
  DISP_BCC,   ///< Bcc: string
  DISP_FROM,  ///< From: string
  DISP_PLAIN, ///< Empty string
  DISP_MAX,
};

/**
 * make_from_prefix - Create a prefix for an author field
 * @param disp   Type of field
 * @retval ptr Prefix string (do not free it)
 *
 * If $from_chars is set, pick an appropriate character from it.
 * If not, use the default prefix: "To", "Cc", etc
 */
static const char *make_from_prefix(enum FieldType disp)
{
  /* need 2 bytes at the end, one for the space, another for NUL */
  static char padded[8];
  static const char *long_prefixes[DISP_MAX] = {
    [DISP_TO] = "To ", [DISP_CC] = "Cc ", [DISP_BCC] = "Bcc ",
    [DISP_FROM] = "",  [DISP_PLAIN] = "",
  };

  const struct MbTable *c_from_chars = cs_subset_mbtable(NeoMutt->sub, "from_chars");

  if (!c_from_chars || !c_from_chars->chars || (c_from_chars->len == 0))
    return long_prefixes[disp];

  const char *pchar = mbtable_get_nth_wchar(c_from_chars, disp);
  if (mutt_str_len(pchar) == 0)
    return "";

  snprintf(padded, sizeof(padded), "%s ", pchar);
  return padded;
}

/**
 * make_from - Generate a From: field (with optional prefix)
 * @param env      Envelope of the email
 * @param buf      Buffer to store the result
 * @param buflen   Size of the buffer
 * @param do_lists Should we check for mailing lists?
 * @param flags    Format flags, see #MuttFormatFlags
 *
 * Generate the %F or %L field in $index_format.
 * This is the author, or recipient of the email.
 *
 * The field can optionally be prefixed by a character from $from_chars.
 * If $from_chars is not set, the prefix will be, "To", "Cc", etc
 */
static void make_from(struct Envelope *env, char *buf, size_t buflen,
                      bool do_lists, MuttFormatFlags flags)
{
  if (!env || !buf)
    return;

  bool me;
  enum FieldType disp;
  struct AddressList *name = NULL;

  me = mutt_addr_is_user(TAILQ_FIRST(&env->from));

  if (do_lists || me)
  {
    if (check_for_mailing_list(&env->to, make_from_prefix(DISP_TO), buf, buflen))
      return;
    if (check_for_mailing_list(&env->cc, make_from_prefix(DISP_CC), buf, buflen))
      return;
  }

  if (me && !TAILQ_EMPTY(&env->to))
  {
    disp = (flags & MUTT_FORMAT_PLAIN) ? DISP_PLAIN : DISP_TO;
    name = &env->to;
  }
  else if (me && !TAILQ_EMPTY(&env->cc))
  {
    disp = DISP_CC;
    name = &env->cc;
  }
  else if (me && !TAILQ_EMPTY(&env->bcc))
  {
    disp = DISP_BCC;
    name = &env->bcc;
  }
  else if (!TAILQ_EMPTY(&env->from))
  {
    disp = DISP_FROM;
    name = &env->from;
  }
  else
  {
    *buf = '\0';
    return;
  }

  snprintf(buf, buflen, "%s%s", make_from_prefix(disp), mutt_get_name(TAILQ_FIRST(name)));
}

/**
 * make_from_addr - Create a 'from' address for a reply email
 * @param env      Envelope of current email
 * @param buf      Buffer for the result
 * @param buflen   Length of buffer
 * @param do_lists If true, check for mailing lists
 */
static void make_from_addr(struct Envelope *env, char *buf, size_t buflen, bool do_lists)
{
  if (!env || !buf)
    return;

  bool me = mutt_addr_is_user(TAILQ_FIRST(&env->from));

  if (do_lists || me)
  {
    if (check_for_mailing_list_addr(&env->to, buf, buflen))
      return;
    if (check_for_mailing_list_addr(&env->cc, buf, buflen))
      return;
  }

  if (me && !TAILQ_EMPTY(&env->to))
    snprintf(buf, buflen, "%s", buf_string(TAILQ_FIRST(&env->to)->mailbox));
  else if (me && !TAILQ_EMPTY(&env->cc))
    snprintf(buf, buflen, "%s", buf_string(TAILQ_FIRST(&env->cc)->mailbox));
  else if (!TAILQ_EMPTY(&env->from))
    mutt_str_copy(buf, buf_string(TAILQ_FIRST(&env->from)->mailbox), buflen);
  else
    *buf = '\0';
}

/**
 * user_in_addr - Do any of the addresses refer to the user?
 * @param al AddressList
 * @retval true Any of the addresses match one of the user's addresses
 */
static bool user_in_addr(struct AddressList *al)
{
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  if (mutt_addr_is_user(a))
    return true;
  return false;
}

/**
 * user_is_recipient - Is the user a recipient of the message
 * @param e Email to test
 * @retval enum Character index into the `$to_chars` config variable
 */
static enum ToChars user_is_recipient(struct Email *e)
{
  if (!e || !e->env)
    return FLAG_CHAR_TO_NOT_IN_THE_LIST;

  struct Envelope *env = e->env;

  if (!e->recip_valid)
  {
    e->recip_valid = true;

    if (mutt_addr_is_user(TAILQ_FIRST(&env->from)))
    {
      e->recipient = FLAG_CHAR_TO_ORIGINATOR;
    }
    else if (user_in_addr(&env->to))
    {
      if (TAILQ_NEXT(TAILQ_FIRST(&env->to), entries) || !TAILQ_EMPTY(&env->cc))
        e->recipient = FLAG_CHAR_TO_TO; /* non-unique recipient */
      else
        e->recipient = FLAG_CHAR_TO_UNIQUE; /* unique recipient */
    }
    else if (user_in_addr(&env->cc))
    {
      e->recipient = FLAG_CHAR_TO_CC;
    }
    else if (check_for_mailing_list(&env->to, NULL, NULL, 0))
    {
      e->recipient = FLAG_CHAR_TO_SUBSCRIBED_LIST;
    }
    else if (check_for_mailing_list(&env->cc, NULL, NULL, 0))
    {
      e->recipient = FLAG_CHAR_TO_SUBSCRIBED_LIST;
    }
    else if (user_in_addr(&env->reply_to))
    {
      e->recipient = FLAG_CHAR_TO_REPLY_TO;
    }
    else
    {
      e->recipient = FLAG_CHAR_TO_NOT_IN_THE_LIST;
    }
  }

  return e->recipient;
}

/**
 * thread_is_new - Does the email thread contain any new emails?
 * @param e Email
 * @retval true Thread contains new mail
 */
static bool thread_is_new(struct Email *e)
{
  return e->collapsed && (e->num_hidden > 1) && (mutt_thread_contains_unread(e) == 1);
}

/**
 * thread_is_old - Does the email thread contain any unread emails?
 * @param e Email
 * @retval true Thread contains unread mail
 */
static bool thread_is_old(struct Email *e)
{
  return e->collapsed && (e->num_hidden > 1) && (mutt_thread_contains_unread(e) == 2);
}

/**
 * index_date_recv_local_num - Index: Local received date and time - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_date_recv_local_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  return e->received;
}

/**
 * IndexDateChoice - Which email date to display in the Index
 */
enum IndexDateChoice
{
  SENT_SENDER, ///< Date sent in the sender's timezone
  SENT_LOCAL,  ///< Date sent in the local timezone
  RECV_LOCAL   ///< Date received in the local timezone
};

/**
 * index_email_date - Index: Sent/Received Local/Sender date and time
 * @param node   ExpandoNode containing the callback
 * @param e      Email
 * @param which  Which date to use
 * @param flags  Flags, see #MuttFormatFlags
 * @param buf    Buffer for the result
 * @param format Format string
 */
static void index_email_date(const struct ExpandoNode *node, const struct Email *e,
                             enum IndexDateChoice which, MuttFormatFlags flags,
                             struct Buffer *buf, const char *format)
{
  struct tm tm = { 0 };
  switch (which)
  {
    case SENT_SENDER:
    {
      int offset = (e->zhours * 3600 + e->zminutes * 60) * (e->zoccident ? -1 : 1);
      const time_t now = e->date_sent + offset;
      tm = mutt_date_gmtime(now);
      tm.tm_gmtoff = offset;
      break;
    }
    case SENT_LOCAL:
    {
      tm = mutt_date_localtime(e->date_sent);
      break;
    }
    case RECV_LOCAL:
    {
      tm = mutt_date_localtime(e->received);
      break;
    }
  }

  char *fmt = mutt_str_dup(format);

  const bool use_c_locale = (*fmt == '!');

  if (which != RECV_LOCAL)
  {
    // The sender's time zone might only be available as a numerical offset, so "%Z" behaves like "%z".
    for (char *bigz = fmt; (bigz = strstr(bigz, "%Z")); bigz += 2)
    {
      bigz[1] = 'z';
    }
  }

  char out[128] = { 0 };
  if (use_c_locale)
  {
    strftime_l(out, sizeof(out), fmt + 1, &tm, NeoMutt->time_c_locale);
  }
  else
  {
    strftime(out, sizeof(out), fmt, &tm);
  }

  FREE(&fmt);

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_DATE);
  buf_strcpy(buf, out);
}

/**
 * index_date_recv_local - Index: Received local date and time - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_date_recv_local(const struct ExpandoNode *node, void *data,
                           MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  index_email_date(node, e, RECV_LOCAL, flags, buf, node->text);
}

/**
 * index_date_local_num - Index: Local date and time - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_date_local_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  return e->date_sent;
}

/**
 * index_date_local - Index: Sent local date and time - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_date_local(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  index_email_date(node, e, SENT_LOCAL, flags, buf, node->text);
}

/**
 * index_date_num - Index: Sender's date and time - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_date_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  return e->date_sent;
}

/**
 * index_date - Index: Sent date and time - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_date(const struct ExpandoNode *node, void *data,
                MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  index_email_date(node, e, SENT_SENDER, flags, buf, node->text);
}

/**
 * index_format_hook - Index: index-format-hook - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_format_hook(const struct ExpandoNode *node, void *data,
                       MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  struct Mailbox *m = hfi->mailbox;

  const struct Expando *exp = mutt_idxfmt_hook(node->text, m, e);
  if (!exp)
    return;

  expando_filter(exp, IndexRenderData, data, MUTT_FORMAT_NO_FLAGS, buf->dsize, buf);
}

/**
 * index_a - Index: Author Address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_a(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *from = TAILQ_FIRST(&e->env->from);

  const char *s = NULL;
  if (from && from->mailbox)
  {
    s = mutt_addr_for_display(from);
  }

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);
  buf_strcpy(buf, s);
}

/**
 * index_A - Index: Reply-to address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_A(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *reply_to = TAILQ_FIRST(&e->env->reply_to);

  if (reply_to && reply_to->mailbox)
  {
    if (flags & MUTT_FORMAT_INDEX)
      node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);
    const char *s = mutt_addr_for_display(reply_to);
    buf_strcpy(buf, s);
    return;
  }

  index_a(node, data, flags, buf);
}

/**
 * index_b - Index: Filename - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_b(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Mailbox *m = hfi->mailbox;
  if (!m)
  {
    buf_addstr(buf, "(null)");
    return;
  }

  char *p = NULL;

#ifdef USE_NOTMUCH
  struct Email *e = hfi->email;
  if (m->type == MUTT_NOTMUCH)
  {
    p = nm_email_get_folder_rel_db(m, e);
  }
#endif
  if (!p)
  {
    p = strrchr(mailbox_path(m), '/');
    if (p)
    {
      p++;
    }
  }
  buf_addstr(buf, p ? p : mailbox_path(m));
}

/**
 * index_B - Index: Email list - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_B(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  char tmp[128] = { 0 };

  if (first_mailing_list(tmp, sizeof(tmp), &e->env->to) ||
      first_mailing_list(tmp, sizeof(tmp), &e->env->cc))
  {
    buf_strcpy(buf, tmp);
    return;
  }

  index_b(node, data, flags, buf);
}

/**
 * index_c_num - Index: Number of bytes - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_c_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->body)
    return 0;

  return e->body->length;
}

/**
 * index_c - Index: Number of bytes - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_c(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  char tmp[128] = { 0 };

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_SIZE);

  mutt_str_pretty_size(tmp, sizeof(tmp), e->body->length);
  buf_strcpy(buf, tmp);
}

/**
 * index_cr - Index: Number of raw bytes - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_cr(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = (const struct HdrFormatInfo *) data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  char tmp[128] = { 0 };

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_SIZE);

  mutt_str_pretty_size(tmp, sizeof(tmp), email_size(e));
  buf_strcpy(buf, tmp);
}

/**
 * index_C_num - Index: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_C_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_NUMBER);

  return e->msgno + 1;
}

/**
 * index_d_num - Index: Senders Date and time - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_d_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  return e->date_sent;
}

/**
 * index_d - Index: Sent date and time - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_d(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  const char *c_date_format = cs_subset_string(NeoMutt->sub, "date_format");
  const char *cp = NONULL(c_date_format);

  index_email_date(node, e, SENT_SENDER, flags, buf, cp);
}

/**
 * index_D - Index: Sent local date and time - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_D(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  const char *c_date_format = cs_subset_string(NeoMutt->sub, "date_format");
  const char *cp = NONULL(c_date_format);

  index_email_date(node, e, SENT_LOCAL, flags, buf, cp);
}

/**
 * index_D_num - Index: Local Date and time - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_D_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  return e->date_sent;
}

/**
 * index_e_num - Index: Thread index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_e_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  struct Mailbox *m = hfi->mailbox;

  return mutt_messages_in_thread(m, e, MIT_POSITION);
}

/**
 * index_E_num - Index: Number of messages thread - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_E_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  struct Mailbox *m = hfi->mailbox;

  return mutt_messages_in_thread(m, e, MIT_NUM_MESSAGES);
}

/**
 * index_f - Index: Sender - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_f(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  mutt_addrlist_write(&e->env->from, buf, true);
}

/**
 * index_F - Index: Author name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_F(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  char tmp[128] = { 0 };

  make_from(e->env, tmp, sizeof(tmp), false, MUTT_FORMAT_NO_FLAGS);

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);

  buf_strcpy(buf, tmp);
}

/**
 * index_Fp - Index: Plain author name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_Fp(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = (const struct HdrFormatInfo *) data;
  struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  char tmp[128] = { 0 };

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);

  make_from(e->env, tmp, sizeof(tmp), false, MUTT_FORMAT_PLAIN);

  buf_strcpy(buf, tmp);
}

/**
 * index_g - Index: Message tags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_g(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_TAGS);
  driver_tags_get_transformed(&e->tags, buf);
}

/**
 * index_G - Index: Individual tag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_G(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  char *tag = mutt_hash_find(TagFormats, node->text);
  if (!tag)
    return;

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_TAG);
  driver_tags_get_transformed_for(&e->tags, tag, buf);
}

/**
 * index_H - Index: Spam attributes - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_H(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  buf_copy(buf, &e->env->spam);
}

/**
 * index_i - Index: Message-id - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_i(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const char *s = e->env->message_id ? e->env->message_id : "<no.id>";
  buf_strcpy(buf, s);
}

/**
 * index_I - Index: Initials of author - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_I(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *from = TAILQ_FIRST(&e->env->from);

  char tmp[128] = { 0 };

  if (mutt_mb_get_initials(mutt_get_name(from), tmp, sizeof(tmp)))
  {
    if (flags & MUTT_FORMAT_INDEX)
      node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);

    buf_strcpy(buf, tmp);
    return;
  }

  index_a(node, data, flags, buf);
}

/**
 * index_J - Index: Tags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_J(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  bool have_tags = true;
  struct Buffer *tags = buf_pool_get();
  driver_tags_get_transformed(&e->tags, tags);
  if (!buf_is_empty(tags))
  {
    if (flags & MUTT_FORMAT_TREE)
    {
      struct Buffer *parent_tags = buf_pool_get();
      if (e->thread->prev && e->thread->prev->message)
      {
        driver_tags_get_transformed(&e->thread->prev->message->tags, parent_tags);
      }
      if (!parent_tags && e->thread->parent && e->thread->parent->message)
      {
        driver_tags_get_transformed(&e->thread->parent->message->tags, parent_tags);
      }
      if (parent_tags && buf_iequal(tags, parent_tags))
        have_tags = false;
      buf_pool_release(&parent_tags);
    }
  }
  else
  {
    have_tags = false;
  }

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_TAGS);

  const char *s = have_tags ? buf_string(tags) : "";
  buf_strcpy(buf, s);

  buf_pool_release(&tags);
}

/**
 * index_K - Index: Mailing list - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_K(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  char tmp[128] = { 0 };

  if (first_mailing_list(tmp, sizeof(tmp), &e->env->to) ||
      first_mailing_list(tmp, sizeof(tmp), &e->env->cc))
  {
    buf_strcpy(buf, tmp);
  }
}

/**
 * index_l_num - Index: Number of lines - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_l_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_SIZE);

  return e->lines;
}

/**
 * index_L - Index: List address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_L(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  char tmp[128] = { 0 };

  make_from(e->env, tmp, sizeof(tmp), true, flags);

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);
  buf_strcpy(buf, tmp);
}

/**
 * index_m_num - Index: Total number of message - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_m_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Mailbox *m = hfi->mailbox;

  if (m)
    return m->msg_count;

  return 0;
}

/**
 * index_M - Index: Number of hidden messages - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_M(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  const bool threads = mutt_using_threads();
  const bool is_index = (flags & MUTT_FORMAT_INDEX) != 0;

  if (threads && is_index && e->collapsed && (e->num_hidden > 1))
  {
    if (flags & MUTT_FORMAT_INDEX)
      node_expando_set_color(node, MT_COLOR_INDEX_COLLAPSED);
    const int num = e->num_hidden;
    buf_printf(buf, "%d", num);
  }
  else if (is_index && threads)
  {
    if (flags & MUTT_FORMAT_INDEX)
      node_expando_set_color(node, MT_COLOR_INDEX_COLLAPSED);
    const char *s = " ";
    buf_strcpy(buf, s);
  }
}

/**
 * index_M_num - Index: Number of hidden messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_M_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  const bool threads = mutt_using_threads();
  const bool is_index = (flags & MUTT_FORMAT_INDEX) != 0;

  if (threads && is_index && e->collapsed && (e->num_hidden > 1))
  {
    if (flags & MUTT_FORMAT_INDEX)
      node_expando_set_color(node, MT_COLOR_INDEX_COLLAPSED);
    return e->num_hidden;
  }

  return 0;
}

/**
 * index_n - Index: Author's real name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_n(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *from = TAILQ_FIRST(&e->env->from);

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_AUTHOR);

  const char *s = mutt_get_name(from);
  buf_strcpy(buf, s);
}

/**
 * index_N_num - Index: Message score - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_N_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return 0;

  return e->score;
}

/**
 * index_O - Index: List Name or Save folder - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_O(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  char tmp[128] = { 0 };
  char *p = NULL;

  make_from_addr(e->env, tmp, sizeof(tmp), true);
  const bool c_save_address = cs_subset_bool(NeoMutt->sub, "save_address");
  if (!c_save_address && (p = strpbrk(tmp, "%@")))
  {
    *p = '\0';
  }

  buf_strcpy(buf, tmp);
}

/**
 * index_P - Index: Progress indicator - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_P(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;

  const char *s = hfi->pager_progress;
  buf_strcpy(buf, s);
}

/**
 * index_q - Index: Newsgroup name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_q(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const char *s = e->env->newsgroups;
  buf_strcpy(buf, s);
}

/**
 * index_r - Index: To recipients - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_r(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  mutt_addrlist_write(&e->env->to, buf, true);
}

/**
 * index_R - Index: Cc recipients - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_R(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  mutt_addrlist_write(&e->env->cc, buf, true);
}

/**
 * index_s - Index: Subject - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_s(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  if ((flags & MUTT_FORMAT_TREE) && !e->collapsed && !(flags & MUTT_FORMAT_FORCESUBJ))
    return;

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_SUBJECT);

  subjrx_apply_mods(e->env);

  if (e->env->disp_subj)
    buf_strcpy(buf, e->env->disp_subj);
  else
    buf_strcpy(buf, e->env->subject);
}

/**
 * index_S - Index: Status flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_S(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  const int msg_in_pager = hfi->msg_in_pager;

  const char *wch = NULL;
  if (e->deleted)
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_DELETED);
  else if (e->attach_del)
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_DELETED_ATTACH);
  else if (e->tagged)
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_TAGGED);
  else if (e->flagged)
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_IMPORTANT);
  else if (e->replied)
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_REPLIED);
  else if (e->read && (msg_in_pager != e->msgno))
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_SEMPTY);
  else if (e->old)
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_OLD);
  else
    wch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_NEW);

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_FLAGS);

  buf_strcpy(buf, wch);
}

/**
 * index_t - Index: To field - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_t(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *to = TAILQ_FIRST(&e->env->to);
  const struct Address *cc = TAILQ_FIRST(&e->env->cc);

  char tmp[128] = { 0 };

  if (!check_for_mailing_list(&e->env->to, "To ", tmp, sizeof(tmp)) &&
      !check_for_mailing_list(&e->env->cc, "Cc ", tmp, sizeof(tmp)))
  {
    if (to)
      snprintf(tmp, sizeof(tmp), "To %s", mutt_get_name(to));
    else if (cc)
      snprintf(tmp, sizeof(tmp), "Cc %s", mutt_get_name(cc));
    else
    {
      tmp[0] = '\0';
    }
  }

  buf_strcpy(buf, tmp);
}

/**
 * index_T - Index: $to_chars flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_T(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  const struct MbTable *c_to_chars = cs_subset_mbtable(NeoMutt->sub, "to_chars");

  int i;
  const char *s = (c_to_chars && ((i = user_is_recipient(e))) < c_to_chars->len) ?
                      c_to_chars->chars[i] :
                      " ";

  buf_strcpy(buf, s);
}

/**
 * index_tree - Index: Thread tree - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_tree(const struct ExpandoNode *node, void *data,
                MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  if (!(flags & MUTT_FORMAT_TREE) || e->collapsed)
    return;

  node_expando_set_color(node, MT_COLOR_TREE);
  node_expando_set_has_tree(node, true);
  buf_strcpy(buf, e->tree);
}

/**
 * index_u - Index: User name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_u(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *from = TAILQ_FIRST(&e->env->from);
  if (!from || !from->mailbox)
    return;

  char tmp[128] = { 0 };
  char *p = NULL;

  mutt_str_copy(tmp, mutt_addr_for_display(from), sizeof(tmp));
  p = strpbrk(tmp, "%@");
  if (p)
  {
    *p = '\0';
  }

  buf_strcpy(buf, tmp);
}

/**
 * index_v - Index: First name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_v(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const struct Address *from = TAILQ_FIRST(&e->env->from);
  const struct Address *to = TAILQ_FIRST(&e->env->to);
  const struct Address *cc = TAILQ_FIRST(&e->env->cc);

  char tmp[128] = { 0 };
  char *p = NULL;

  if (mutt_addr_is_user(from))
  {
    if (to)
    {
      const char *s = mutt_get_name(to);
      mutt_str_copy(tmp, NONULL(s), sizeof(tmp));
    }
    else if (cc)
    {
      const char *s = mutt_get_name(cc);
      mutt_str_copy(tmp, NONULL(s), sizeof(tmp));
    }
  }
  else
  {
    const char *s = mutt_get_name(from);
    mutt_str_copy(tmp, NONULL(s), sizeof(tmp));
  }
  p = strpbrk(tmp, " %@");
  if (p)
  {
    *p = '\0';
  }

  buf_strcpy(buf, tmp);
}

/**
 * index_W - Index: Organization - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_W(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const char *s = e->env->organization;
  buf_strcpy(buf, s);
}

/**
 * index_x - Index: X-Comment-To - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_x(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  const char *s = e->env->x_comment_to;
  buf_strcpy(buf, s);
}

/**
 * index_X_num - Index: Number of MIME attachments - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long index_X_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return 0;

  struct Mailbox *m = hfi->mailbox;

  struct Message *msg = mx_msg_open(m, e);
  if (!msg)
    return 0;

  const int num = mutt_count_body_parts(m, e, msg->fp);
  mx_msg_close(m, &msg);
  return num;
}

/**
 * index_y - Index: X-Label - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_y(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_LABEL);

  const char *s = e->env->x_label;
  buf_strcpy(buf, s);
}

/**
 * index_Y - Index: X-Label (if different) - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_Y(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e || !e->env)
    return;

  bool label = true;
  if (e->env->x_label)
  {
    struct Email *e_tmp = NULL;
    if (flags & MUTT_FORMAT_TREE && (e->thread->prev && e->thread->prev->message &&
                                     e->thread->prev->message->env->x_label))
    {
      e_tmp = e->thread->prev->message;
    }
    else if (flags & MUTT_FORMAT_TREE && (e->thread->parent && e->thread->parent->message &&
                                          e->thread->parent->message->env->x_label))
    {
      e_tmp = e->thread->parent->message;
    }

    if (e_tmp && mutt_istr_equal(e->env->x_label, e_tmp->env->x_label))
    {
      label = false;
    }
  }
  else
  {
    label = false;
  }

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_LABEL);

  if (label)
  {
    const char *s = e->env->x_label;
    buf_strcpy(buf, s);
  }
}

/**
 * index_zc - Index: Message crypto flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_zc(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;
  if (!e)
    return;

  const struct MbTable *c_crypt_chars = cs_subset_mbtable(NeoMutt->sub, "crypt_chars");

  const char *ch = NULL;
  if ((WithCrypto != 0) && (e->security & SEC_GOODSIGN))
  {
    ch = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_GOOD_SIGN);
  }
  else if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT))
  {
    ch = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_ENCRYPTED);
  }
  else if ((WithCrypto != 0) && (e->security & SEC_SIGN))
  {
    ch = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_SIGNED);
  }
  else if (((WithCrypto & APPLICATION_PGP) != 0) && ((e->security & PGP_KEY) == PGP_KEY))
  {
    ch = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_CONTAINS_KEY);
  }
  else
  {
    ch = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_NO_CRYPTO);
  }

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_FLAGS);
  buf_strcpy(buf, ch);
}

/**
 * index_zs - Index: Message status flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_zs(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  const bool threads = mutt_using_threads();
  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  const int msg_in_pager = hfi->msg_in_pager;

  const char *ch = NULL;
  if (e->deleted)
  {
    ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_DELETED);
  }
  else if (e->attach_del)
  {
    ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_DELETED_ATTACH);
  }
  else if (threads && thread_is_new(e))
  {
    ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_NEW_THREAD);
  }
  else if (threads && thread_is_old(e))
  {
    ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_OLD_THREAD);
  }
  else if (e->read && (msg_in_pager != e->msgno))
  {
    if (e->replied)
    {
      ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_REPLIED);
    }
    else
    {
      ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_ZEMPTY);
    }
  }
  else
  {
    if (e->old)
    {
      ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_OLD);
    }
    else
    {
      ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_NEW);
    }
  }

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_FLAGS);
  buf_strcpy(buf, ch);
}

/**
 * index_zt - Index: Message tag flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_zt(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  const struct MbTable *c_to_chars = cs_subset_mbtable(NeoMutt->sub, "to_chars");

  const char *ch = NULL;
  if (e->tagged)
  {
    ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_TAGGED);
  }
  else if (e->flagged)
  {
    ch = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_IMPORTANT);
  }
  else
  {
    ch = mbtable_get_nth_wchar(c_to_chars, user_is_recipient(e));
  }

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_FLAGS);
  buf_strcpy(buf, ch);
}

/**
 * index_Z - Index: Status flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void index_Z(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct HdrFormatInfo *hfi = data;
  struct Email *e = hfi->email;
  if (!e)
    return;

  const int msg_in_pager = hfi->msg_in_pager;

  const struct MbTable *c_crypt_chars = cs_subset_mbtable(NeoMutt->sub, "crypt_chars");
  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  const struct MbTable *c_to_chars = cs_subset_mbtable(NeoMutt->sub, "to_chars");
  const bool threads = mutt_using_threads();

  const char *first = NULL;
  if (threads && thread_is_new(e))
  {
    first = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_NEW_THREAD);
  }
  else if (threads && thread_is_old(e))
  {
    first = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_OLD_THREAD);
  }
  else if (e->read && (msg_in_pager != e->msgno))
  {
    if (e->replied)
    {
      first = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_REPLIED);
    }
    else
    {
      first = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_ZEMPTY);
    }
  }
  else
  {
    if (e->old)
    {
      first = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_OLD);
    }
    else
    {
      first = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_NEW);
    }
  }

  /* Marked for deletion; deleted attachments; crypto */
  const char *second = NULL;
  if (e->deleted)
    second = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_DELETED);
  else if (e->attach_del)
    second = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_DELETED_ATTACH);
  else if ((WithCrypto != 0) && (e->security & SEC_GOODSIGN))
    second = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_GOOD_SIGN);
  else if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT))
    second = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_ENCRYPTED);
  else if ((WithCrypto != 0) && (e->security & SEC_SIGN))
    second = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_SIGNED);
  else if (((WithCrypto & APPLICATION_PGP) != 0) && (e->security & PGP_KEY))
    second = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_CONTAINS_KEY);
  else
    second = mbtable_get_nth_wchar(c_crypt_chars, FLAG_CHAR_CRYPT_NO_CRYPTO);

  /* Tagged, flagged and recipient flag */
  const char *third = NULL;
  if (e->tagged)
    third = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_TAGGED);
  else if (e->flagged)
    third = mbtable_get_nth_wchar(c_flag_chars, FLAG_CHAR_IMPORTANT);
  else
    third = mbtable_get_nth_wchar(c_to_chars, user_is_recipient(e));

  if (flags & MUTT_FORMAT_INDEX)
    node_expando_set_color(node, MT_COLOR_INDEX_FLAGS);

  buf_printf(buf, "%s%s%s", first, second, third);
}

/**
 * mutt_make_string - Create formatted strings using mailbox expandos
 * @param buf      Buffer for the result
 * @param max_cols Number of screen columns (-1 means unlimited)
 * @param exp      Expando containing expando tree
 * @param m        Mailbox
 * @param inpgr    Message shown in the pager
 * @param e        Email
 * @param flags    Flags, see #MuttFormatFlags
 * @param progress Pager progress string
 * @retval num Number of screen columns used
 */
int mutt_make_string(struct Buffer *buf, size_t max_cols,
                     const struct Expando *exp, struct Mailbox *m, int inpgr,
                     struct Email *e, MuttFormatFlags flags, const char *progress)
{
  if (!exp)
    return 0;

  struct HdrFormatInfo hfi = { 0 };

  hfi.email = e;
  hfi.mailbox = m;
  hfi.msg_in_pager = inpgr;
  hfi.pager_progress = progress;

  return expando_filter(exp, IndexRenderData, &hfi, flags, max_cols, buf);
}

/**
 * IndexRenderData - Callbacks for Index Expandos
 *
 * @sa IndexFormatDef, ExpandoDataEmail, ExpandoDataEnvelope, ExpandoDataGlobal, ExpandoDataMailbox
 */
const struct ExpandoRenderData IndexRenderData[] = {
  // clang-format off
  { ED_EMAIL,    ED_EMA_STRF_RECV_LOCAL,     index_date_recv_local, index_date_recv_local_num },
  { ED_EMAIL,    ED_EMA_INDEX_HOOK,          index_format_hook,     NULL },
  { ED_ENVELOPE, ED_ENV_FROM,                index_a,               NULL },
  { ED_ENVELOPE, ED_ENV_REPLY_TO,            index_A,               NULL },
  { ED_ENVELOPE, ED_ENV_LIST_ADDRESS,        index_B,               NULL },
  { ED_MAILBOX,  ED_MBX_MAILBOX_NAME,        index_b,               NULL },
  { ED_EMAIL,    ED_EMA_NUMBER,              NULL,                  index_C_num },
  { ED_EMAIL,    ED_EMA_SIZE,                index_c,               index_c_num },
  { ED_EMAIL,    ED_EMA_BODY_CHARACTERS,     index_cr,              NULL },
  { ED_EMAIL,    ED_EMA_DATE_FORMAT,         index_d,               index_d_num },
  { ED_EMAIL,    ED_EMA_DATE_FORMAT_LOCAL,   index_D,               index_D_num },
  { ED_EMAIL,    ED_EMA_THREAD_COUNT,        NULL,                  index_E_num },
  { ED_EMAIL,    ED_EMA_THREAD_NUMBER,       NULL,                  index_e_num },
  { ED_ENVELOPE, ED_ENV_FROM_FULL,           index_f,               NULL },
  { ED_ENVELOPE, ED_ENV_SENDER,              index_F,               NULL },
  { ED_ENVELOPE, ED_ENV_SENDER_PLAIN,        index_Fp,              NULL },
  { ED_EMAIL,    ED_EMA_TAGS,                index_g,               NULL },
  { ED_EMAIL,    ED_EMA_TAGS_TRANSFORMED,    index_G,               NULL },
  { ED_ENVELOPE, ED_ENV_SPAM,                index_H,               NULL },
  { ED_ENVELOPE, ED_ENV_INITIALS,            index_I,               NULL },
  { ED_ENVELOPE, ED_ENV_MESSAGE_ID,          index_i,               NULL },
  { ED_EMAIL,    ED_EMA_THREAD_TAGS,         index_J,               NULL },
  { ED_ENVELOPE, ED_ENV_LIST_EMPTY,          index_K,               NULL },
  { ED_EMAIL,    ED_EMA_FROM_LIST,           index_L,               NULL },
  { ED_EMAIL,    ED_EMA_LINES,               NULL,                  index_l_num },
  { ED_MAILBOX,  ED_MBX_MESSAGE_COUNT,       NULL,                  index_m_num },
  { ED_EMAIL,    ED_EMA_THREAD_HIDDEN_COUNT, index_M,               index_M_num },
  { ED_ENVELOPE, ED_ENV_NAME,                index_n,               NULL },
  { ED_EMAIL,    ED_EMA_SCORE,               NULL,                  index_N_num },
  { ED_EMAIL,    ED_EMA_LIST_OR_SAVE_FOLDER, index_O,               NULL },
  { ED_MAILBOX,  ED_MBX_PERCENTAGE,          index_P,               NULL },
  { ED_ENVELOPE, ED_ENV_NEWSGROUP,           index_q,               NULL },
  { ED_ENVELOPE, ED_ENV_CC_ALL,              index_R,               NULL },
  { ED_ENVELOPE, ED_ENV_TO_ALL,              index_r,               NULL },
  { ED_EMAIL,    ED_EMA_FLAG_CHARS,          index_S,               NULL },
  { ED_ENVELOPE, ED_ENV_SUBJECT,             index_s,               NULL },
  { ED_ENVELOPE, ED_ENV_TO,                  index_t,               NULL },
  { ED_EMAIL,    ED_EMA_TO_CHARS,            index_T,               NULL },
  { ED_ENVELOPE, ED_ENV_THREAD_TREE,         index_tree,            NULL },
  { ED_ENVELOPE, ED_ENV_USERNAME,            index_u,               NULL },
  { ED_ENVELOPE, ED_ENV_FIRST_NAME,          index_v,               NULL },
  { ED_ENVELOPE, ED_ENV_ORGANIZATION,        index_W,               NULL },
  { ED_EMAIL,    ED_EMA_ATTACHMENT_COUNT,    NULL,                  index_X_num },
  { ED_ENVELOPE, ED_ENV_X_COMMENT_TO,        index_x,               NULL },
  { ED_ENVELOPE, ED_ENV_THREAD_X_LABEL,      index_Y,               NULL },
  { ED_ENVELOPE, ED_ENV_X_LABEL,             index_y,               NULL },
  { ED_EMAIL,    ED_EMA_COMBINED_FLAGS,      index_Z,               NULL },
  { ED_EMAIL,    ED_EMA_CRYPTO_FLAGS,        index_zc,              NULL },
  { ED_EMAIL,    ED_EMA_STATUS_FLAGS,        index_zs,              NULL },
  { ED_EMAIL,    ED_EMA_MESSAGE_FLAGS,       index_zt,              NULL },
  { ED_EMAIL,    ED_EMA_STRF_LOCAL,          index_date_local,      index_date_local_num },
  { ED_EMAIL,    ED_EMA_STRF,                index_date,            index_date_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
