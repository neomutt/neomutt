/**
 * @file
 * IMAP network mailbox
 *
 * @authors
 * Copyright (C) 1996-1998,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012,2017 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Mehdi Abaakouk <sileht@sileht.net>
 * Copyright (C) 2018-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Federico Kircheis <federico.kircheis@gmail.com>
 * Copyright (C) 2019 Ian Zimmerman <itz@no-use.mooo.com>
 * Copyright (C) 2019 Sergey Alirzaev <zl29ah@gmail.com>
 * Copyright (C) 2020 Reto Brunner <reto@slightlybroken.com>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
 * Copyright (C) 2023-2024 Dennis Schön <mail@dennis-schoen.de>
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
 * @page imap_imap IMAP network mailbox
 *
 * Support for IMAP4rev1, with the occasional nod to IMAP 4.
 *
 * Implementation: #MxImapOps
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "mutt.h"
#include "lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "parse/lib.h"
#include "progress/lib.h"
#include "question/lib.h"
#include "adata.h"
#include "auth.h"
#include "commands.h"
#include "edata.h"
#include "external.h"
#include "hook.h"
#include "mdata.h"
#include "msg_set.h"
#include "msn.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "muttlib.h"
#include "mx.h"
#include "sort.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

struct Progress;
struct stat;

/**
 * ImapCommands - Imap Commands
 */
static const struct Command ImapCommands[] = {
  // clang-format off
  { "subscribe-to",     parse_subscribe_to,     0 },
  { "unsubscribe-from", parse_unsubscribe_from, 0 },
  // clang-format on
};

/**
 * imap_init - Setup feature commands
 */
void imap_init(void)
{
  commands_register(ImapCommands, mutt_array_size(ImapCommands));
}

/**
 * check_capabilities - Make sure we can log in to this server
 * @param adata Imap Account data
 * @retval  0 Success
 * @retval -1 Failure
 */
static int check_capabilities(struct ImapAccountData *adata)
{
  if (imap_exec(adata, "CAPABILITY", IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
  {
    imap_error("check_capabilities", adata->buf);
    return -1;
  }

  if (!((adata->capabilities & IMAP_CAP_IMAP4) || (adata->capabilities & IMAP_CAP_IMAP4REV1)))
  {
    mutt_error(_("This IMAP server is ancient. NeoMutt does not work with it."));
    return -1;
  }

  return 0;
}

/**
 * get_flags - Make a simple list out of a FLAGS response
 * @param hflags List to store flags
 * @param s      String containing flags
 * @retval ptr  End of the flags
 * @retval NULL Failure
 *
 * return stream following FLAGS response
 */
static char *get_flags(struct ListHead *hflags, char *s)
{
  /* sanity-check string */
  const size_t plen = mutt_istr_startswith(s, "FLAGS");
  if (plen == 0)
  {
    mutt_debug(LL_DEBUG1, "not a FLAGS response: %s\n", s);
    return NULL;
  }
  s += plen;
  SKIPWS(s);
  if (*s != '(')
  {
    mutt_debug(LL_DEBUG1, "bogus FLAGS response: %s\n", s);
    return NULL;
  }

  /* update caller's flags handle */
  while (*s && (*s != ')'))
  {
    s++;
    SKIPWS(s);
    const char *flag_word = s;
    while (*s && (*s != ')') && !isspace(*s))
      s++;
    const char ctmp = *s;
    *s = '\0';
    if (*flag_word)
      mutt_list_insert_tail(hflags, mutt_str_dup(flag_word));
    *s = ctmp;
  }

  /* note bad flags response */
  if (*s != ')')
  {
    mutt_debug(LL_DEBUG1, "Unterminated FLAGS response: %s\n", s);
    mutt_list_free(hflags);

    return NULL;
  }

  s++;

  return s;
}

/**
 * set_flag - Append str to flags if we currently have permission according to aclflag
 * @param[in]  m       Selected Imap Mailbox
 * @param[in]  aclflag Permissions, see #AclFlags
 * @param[in]  flag    Does the email have the flag set?
 * @param[in]  str     Server flag name
 * @param[out] flags   Buffer for server command
 */
static void set_flag(struct Mailbox *m, AclFlags aclflag, bool flag,
                     const char *str, struct Buffer *flags)
{
  if (m->rights & aclflag)
    if (flag && imap_has_flag(&imap_mdata_get(m)->flags, str))
      buf_addstr(flags, str);
}

/**
 * compare_flags_for_copy - Compare local flags against the server
 * @param e Email
 * @retval true  Flags have changed
 * @retval false Flags match cached server flags
 *
 * The comparison of flags EXCLUDES the deleted flag.
 */
static bool compare_flags_for_copy(struct Email *e)
{
  struct ImapEmailData *edata = e->edata;

  if (e->read != edata->read)
    return true;
  if (e->old != edata->old)
    return true;
  if (e->flagged != edata->flagged)
    return true;
  if (e->replied != edata->replied)
    return true;

  return false;
}

/**
 * select_email_uids - Create a list of Email UIDs by type
 * @param emails     Array of Emails
 * @param num_emails Number of Emails in the array
 * @param flag       Flag type on which to filter, e.g. #MUTT_REPLIED
 * @param changed    Include only changed messages in message set
 * @param invert     Invert sense of flag, eg #MUTT_READ matches unread messages
 * @param uida       Array to fill with UIDs
 * @retval num Number of UIDs added
 * @retval  -1 Error
 */
static int select_email_uids(struct Email **emails, int num_emails, enum MessageType flag,
                             bool changed, bool invert, struct UidArray *uida)
{
  if (!emails || !uida)
    return -1;

  for (int i = 0; i < num_emails; i++)
  {
    struct Email *e = emails[i];
    if (changed && !e->changed)
      continue;

    /* don't include pending expunged messages.
     *
     * TODO: can we unset active in cmd_parse_expunge() and
     * cmd_parse_vanished() instead of checking for index != INT_MAX. */
    if (!e || !e->active || (e->index == INT_MAX))
      continue;

    struct ImapEmailData *edata = imap_edata_get(e);

    bool match = false;
    switch (flag)
    {
      case MUTT_DELETED:
        if (e->deleted != edata->deleted)
          match = invert ^ e->deleted;
        break;
      case MUTT_FLAG:
        if (e->flagged != edata->flagged)
          match = invert ^ e->flagged;
        break;
      case MUTT_OLD:
        if (e->old != edata->old)
          match = invert ^ e->old;
        break;
      case MUTT_READ:
        if (e->read != edata->read)
          match = invert ^ e->read;
        break;
      case MUTT_REPLIED:
        if (e->replied != edata->replied)
          match = invert ^ e->replied;
        break;
      case MUTT_TRASH:
        if (e->deleted && !e->purge)
          match = true;
        break;
      default:
        break;
    }

    if (match)
      ARRAY_ADD(uida, edata->uid);
  }

  return ARRAY_SIZE(uida);
}

/**
 * sync_helper - Sync flag changes to the server
 * @param m          Selected Imap Mailbox
 * @param emails     Array of Emails
 * @param num_emails Number of Emails in the array
 * @param right      ACL, see #AclFlags
 * @param flag       NeoMutt flag, e.g. #MUTT_DELETED
 * @param name       Name of server flag
 * @retval >=0 Success, number of messages
 * @retval  -1 Failure
 */
static int sync_helper(struct Mailbox *m, struct Email **emails, int num_emails,
                       AclFlags right, enum MessageType flag, const char *name)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata)
    return -1;

  if ((m->rights & right) == 0)
    return 0;

  if ((right == MUTT_ACL_WRITE) && !imap_has_flag(&imap_mdata_get(m)->flags, name))
    return 0;

  int count = 0;
  char buf[1024] = { 0 };

  struct UidArray uida = ARRAY_HEAD_INITIALIZER;

  // Set the flag (+FLAGS) on matching emails
  select_email_uids(emails, num_emails, flag, true, false, &uida);
  snprintf(buf, sizeof(buf), "+FLAGS.SILENT (%s)", name);
  int rc = imap_exec_msg_set(adata, "UID STORE", buf, &uida);
  if (rc < 0)
    return rc;
  count += rc;
  ARRAY_FREE(&uida);

  // Clear the flag (-FLAGS) on non-matching emails
  select_email_uids(emails, num_emails, flag, true, true, &uida);
  buf[0] = '-';
  rc = imap_exec_msg_set(adata, "UID STORE", buf, &uida);
  if (rc < 0)
    return rc;
  count += rc;
  ARRAY_FREE(&uida);

  return count;
}

/**
 * longest_common_prefix - Find longest prefix common to two strings
 * @param buf   Destination buffer
 * @param src   Source buffer
 * @param start Starting offset into string
 * @retval num Length of the common string
 *
 * Trim dest to the length of the longest prefix it shares with src.
 */
static size_t longest_common_prefix(struct Buffer *buf, const char *src, size_t start)
{
  size_t pos = start;

  size_t len = buf_len(buf);
  while ((pos < len) && buf->data[pos] && (buf->data[pos] == src[pos]))
    pos++;
  buf->data[pos] = '\0';

  buf_fix_dptr(buf);

  return pos;
}

/**
 * complete_hosts - Look for completion matches for mailboxes
 * @param buf Partial mailbox name to complete
 * @retval  0 Success
 * @retval -1 Failure
 *
 * look for IMAP URLs to complete from defined mailboxes. Could be extended to
 * complete over open connections and account/folder hooks too.
 */
static int complete_hosts(struct Buffer *buf)
{
  int rc = -1;
  size_t matchlen;

  matchlen = buf_len(buf);
  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (!mutt_str_startswith(mailbox_path(np->mailbox), buf_string(buf)))
      continue;

    if (rc)
    {
      buf_strcpy(buf, mailbox_path(np->mailbox));
      rc = 0;
    }
    else
    {
      longest_common_prefix(buf, mailbox_path(np->mailbox), matchlen);
    }
  }
  neomutt_mailboxlist_clear(&ml);

#if 0
  TAILQ_FOREACH(conn, mutt_socket_head(), entries)
  {
    struct Url url = { 0 };
    char urlstr[1024] = { 0 };

    if (conn->account.type != MUTT_ACCT_TYPE_IMAP)
      continue;

    mutt_account_tourl(&conn->account, &url);
    /* FIXME: how to handle multiple users on the same host? */
    url.user = NULL;
    url.path = NULL;
    url_tostring(&url, urlstr, sizeof(urlstr), U_NO_FLAGS);
    if (mutt_strn_equal(buf, urlstr, matchlen))
    {
      if (rc)
      {
        mutt_str_copy(buf, urlstr, buflen);
        rc = 0;
      }
      else
      {
        longest_common_prefix(buf, urlstr, matchlen);
      }
    }
  }
#endif

  return rc;
}

/**
 * imap_create_mailbox - Create a new mailbox
 * @param adata Imap Account data
 * @param mailbox Mailbox to create
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_create_mailbox(struct ImapAccountData *adata, const char *mailbox)
{
  char buf[2048] = { 0 };
  char mbox[1024] = { 0 };

  imap_munge_mbox_name(adata->unicode, mbox, sizeof(mbox), mailbox);
  snprintf(buf, sizeof(buf), "CREATE %s", mbox);

  if (imap_exec(adata, buf, IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
  {
    mutt_error(_("CREATE failed: %s"), imap_cmd_trailer(adata));
    return -1;
  }

  return 0;
}

/**
 * imap_access - Check permissions on an IMAP mailbox with a new connection
 * @param path Mailbox path
 * @retval  0 Success
 * @retval <0 Failure
 *
 * TODO: ACL checks. Right now we assume if it exists we can mess with it.
 * TODO: This method should take a Mailbox as parameter to be able to reuse the
 * existing connection.
 */
int imap_access(const char *path)
{
  if (imap_path_status(path, false) >= 0)
    return 0;
  return -1;
}

/**
 * imap_rename_mailbox - Rename a mailbox
 * @param adata Imap Account data
 * @param oldname Existing mailbox
 * @param newname New name for mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_rename_mailbox(struct ImapAccountData *adata, char *oldname, const char *newname)
{
  char oldmbox[1024] = { 0 };
  char newmbox[1024] = { 0 };
  int rc = 0;

  imap_munge_mbox_name(adata->unicode, oldmbox, sizeof(oldmbox), oldname);
  imap_munge_mbox_name(adata->unicode, newmbox, sizeof(newmbox), newname);

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "RENAME %s %s", oldmbox, newmbox);

  if (imap_exec(adata, buf_string(buf), IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
    rc = -1;

  buf_pool_release(&buf);

  return rc;
}

/**
 * imap_delete_mailbox - Delete a mailbox
 * @param m  Mailbox
 * @param path  name of the mailbox to delete
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_delete_mailbox(struct Mailbox *m, char *path)
{
  char buf[PATH_MAX + 7];
  char mbox[PATH_MAX] = { 0 };
  struct Url *url = url_parse(path);
  if (!url)
    return -1;

  struct ImapAccountData *adata = imap_adata_get(m);
  imap_munge_mbox_name(adata->unicode, mbox, sizeof(mbox), url->path);
  url_free(&url);
  snprintf(buf, sizeof(buf), "DELETE %s", mbox);
  if (imap_exec(m->account->adata, buf, IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
    return -1;

  return 0;
}

/**
 * imap_logout - Gracefully log out of server
 * @param adata Imap Account data
 */
static void imap_logout(struct ImapAccountData *adata)
{
  /* we set status here to let imap_handle_untagged know we _expect_ to
   * receive a bye response (so it doesn't freak out and close the conn) */
  if (adata->state == IMAP_DISCONNECTED)
  {
    return;
  }

  adata->status = IMAP_BYE;
  imap_cmd_start(adata, "LOGOUT");
  const short c_imap_poll_timeout = cs_subset_number(NeoMutt->sub, "imap_poll_timeout");
  if ((c_imap_poll_timeout <= 0) ||
      (mutt_socket_poll(adata->conn, c_imap_poll_timeout) != 0))
  {
    while (imap_cmd_step(adata) == IMAP_RES_CONTINUE)
      ; // do nothing
  }
  mutt_socket_close(adata->conn);
  adata->state = IMAP_DISCONNECTED;
}

/**
 * imap_logout_all - Close all open connections
 *
 * Quick and dirty until we can make sure we've got all the context we need.
 */
void imap_logout_all(void)
{
  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    if (np->type != MUTT_IMAP)
      continue;

    struct ImapAccountData *adata = np->adata;
    if (!adata)
      continue;

    struct Connection *conn = adata->conn;
    if (!conn || (conn->fd < 0))
      continue;

    mutt_message(_("Closing connection to %s..."), conn->account.host);
    imap_logout(np->adata);
    mutt_clear_error();
  }
}

/**
 * imap_read_literal - Read bytes bytes from server into file
 * @param fp       File handle for email file
 * @param adata    Imap Account data
 * @param bytes    Number of bytes to read
 * @param progress Progress bar
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Not explicitly buffered, relies on FILE buffering.
 *
 * @note Strips `\r` from `\r\n`.
 *       Apparently even literals use `\r\n`-terminated strings ?!
 */
int imap_read_literal(FILE *fp, struct ImapAccountData *adata,
                      unsigned long bytes, struct Progress *progress)
{
  char c;
  bool r = false;
  struct Buffer buf = { 0 }; // Do not allocate, maybe it won't be used

  const short c_debug_level = cs_subset_number(NeoMutt->sub, "debug_level");
  if (c_debug_level >= IMAP_LOG_LTRL)
    buf_alloc(&buf, bytes + 1);

  mutt_debug(LL_DEBUG2, "reading %lu bytes\n", bytes);

  for (unsigned long pos = 0; pos < bytes; pos++)
  {
    if (mutt_socket_readchar(adata->conn, &c) != 1)
    {
      mutt_debug(LL_DEBUG1, "error during read, %lu bytes read\n", pos);
      adata->status = IMAP_FATAL;

      buf_dealloc(&buf);
      return -1;
    }

    if (r && (c != '\n'))
      fputc('\r', fp);

    if (c == '\r')
    {
      r = true;
      continue;
    }
    else
    {
      r = false;
    }

    fputc(c, fp);

    if ((pos % 1024) == 0)
      progress_update(progress, pos, -1);
    if (c_debug_level >= IMAP_LOG_LTRL)
      buf_addch(&buf, c);
  }

  if (c_debug_level >= IMAP_LOG_LTRL)
  {
    mutt_debug(IMAP_LOG_LTRL, "\n%s", buf.data);
    buf_dealloc(&buf);
  }
  return 0;
}

/**
 * imap_notify_delete_email - Inform IMAP that an Email has been deleted
 * @param m Mailbox
 * @param e Email
 */
void imap_notify_delete_email(struct Mailbox *m, struct Email *e)
{
  struct ImapMboxData *mdata = imap_mdata_get(m);
  struct ImapEmailData *edata = imap_edata_get(e);

  if (!mdata || !edata)
    return;

  imap_msn_remove(&mdata->msn, edata->msn - 1);
  edata->msn = 0;
}

/**
 * imap_expunge_mailbox - Purge messages from the server
 * @param m Mailbox
 * @param resort Trigger a resort?
 *
 * Purge IMAP portion of expunged messages from the context. Must not be done
 * while something has a handle on any headers (eg inside pager or editor).
 * That is, check #IMAP_REOPEN_ALLOW.
 */
void imap_expunge_mailbox(struct Mailbox *m, bool resort)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || !mdata)
    return;

  struct Email *e = NULL;

#ifdef USE_HCACHE
  imap_hcache_open(adata, mdata, false);
#endif

  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      break;

    if (e->index == INT_MAX)
    {
      mutt_debug(LL_DEBUG2, "Expunging message UID %u\n", imap_edata_get(e)->uid);

      e->deleted = true;

      imap_cache_del(m, e);
#ifdef USE_HCACHE
      imap_hcache_del(mdata, imap_edata_get(e)->uid);
#endif

      mutt_hash_int_delete(mdata->uid_hash, imap_edata_get(e)->uid, e);

      imap_edata_free((void **) &e->edata);
    }
    else
    {
      /* NeoMutt has several places where it turns off e->active as a
       * hack.  For example to avoid FLAG updates, or to exclude from
       * imap_exec_msg_set.
       *
       * Unfortunately, when a reopen is allowed and the IMAP_EXPUNGE_PENDING
       * flag becomes set (e.g. a flag update to a modified header),
       * this function will be called by imap_cmd_finish().
       *
       * The ctx_update_tables() will free and remove these "inactive" headers,
       * despite that an EXPUNGE was not received for them.
       * This would result in memory leaks and segfaults due to dangling
       * pointers in the msn_index and uid_hash.
       *
       * So this is another hack to work around the hacks.  We don't want to
       * remove the messages, so make sure active is on.  */
      e->active = true;
    }
  }

#ifdef USE_HCACHE
  imap_hcache_close(mdata);
#endif

  mailbox_changed(m, NT_MAILBOX_UPDATE);
  if (resort)
  {
    mailbox_changed(m, NT_MAILBOX_RESORT);
  }
}

/**
 * imap_open_connection - Open an IMAP connection
 * @param adata Imap Account data
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_open_connection(struct ImapAccountData *adata)
{
  if (mutt_socket_open(adata->conn) < 0)
    return -1;

  adata->state = IMAP_CONNECTED;

  if (imap_cmd_step(adata) != IMAP_RES_OK)
  {
    imap_close_connection(adata);
    return -1;
  }

  if (mutt_istr_startswith(adata->buf, "* OK"))
  {
    if (!mutt_istr_startswith(adata->buf, "* OK [CAPABILITY") && check_capabilities(adata))
    {
      goto bail;
    }
#ifdef USE_SSL
    /* Attempt STARTTLS if available and desired. */
    const bool c_ssl_force_tls = cs_subset_bool(NeoMutt->sub, "ssl_force_tls");
    if ((adata->conn->ssf == 0) &&
        (c_ssl_force_tls || (adata->capabilities & IMAP_CAP_STARTTLS)))
    {
      enum QuadOption ans;

      if (c_ssl_force_tls)
      {
        ans = MUTT_YES;
      }
      else if ((ans = query_quadoption(_("Secure connection with TLS?"),
                                       NeoMutt->sub, "ssl_starttls")) == MUTT_ABORT)
      {
        goto bail;
      }
      if (ans == MUTT_YES)
      {
        enum ImapExecResult rc = imap_exec(adata, "STARTTLS", IMAP_CMD_SINGLE);
        // Clear any data after the STARTTLS acknowledgement
        mutt_socket_empty(adata->conn);

        if (rc == IMAP_EXEC_FATAL)
          goto bail;
        if (rc != IMAP_EXEC_ERROR)
        {
          if (mutt_ssl_starttls(adata->conn))
          {
            mutt_error(_("Could not negotiate TLS connection"));
            goto bail;
          }
          else
          {
            /* RFC2595 demands we recheck CAPABILITY after TLS completes. */
            if (imap_exec(adata, "CAPABILITY", IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
              goto bail;
          }
        }
      }
    }

    if (c_ssl_force_tls && (adata->conn->ssf == 0))
    {
      mutt_error(_("Encrypted connection unavailable"));
      goto bail;
    }
#endif
  }
  else if (mutt_istr_startswith(adata->buf, "* PREAUTH"))
  {
#ifdef USE_SSL
    /* Unless using a secure $tunnel, an unencrypted PREAUTH response may be a
     * MITM attack.  The only way to stop "STARTTLS" MITM attacks is via
     * $ssl_force_tls: an attacker can easily spoof "* OK" and strip the
     * STARTTLS capability.  So consult $ssl_force_tls, not $ssl_starttls, to
     * decide whether to abort. Note that if using $tunnel and
     * $tunnel_is_secure, adata->conn->ssf will be set to 1. */
    const bool c_ssl_force_tls = cs_subset_bool(NeoMutt->sub, "ssl_force_tls");
    if ((adata->conn->ssf == 0) && c_ssl_force_tls)
    {
      mutt_error(_("Encrypted connection unavailable"));
      goto bail;
    }
#endif

    adata->state = IMAP_AUTHENTICATED;
    if (check_capabilities(adata) != 0)
      goto bail;
    FREE(&adata->capstr);
  }
  else
  {
    imap_error("imap_open_connection()", adata->buf);
    goto bail;
  }

  return 0;

bail:
  imap_close_connection(adata);
  FREE(&adata->capstr);
  return -1;
}

/**
 * imap_close_connection - Close an IMAP connection
 * @param adata Imap Account data
 */
void imap_close_connection(struct ImapAccountData *adata)
{
  if (adata->state != IMAP_DISCONNECTED)
  {
    mutt_socket_close(adata->conn);
    adata->state = IMAP_DISCONNECTED;
  }
  adata->seqno = 0;
  adata->nextcmd = 0;
  adata->lastcmd = 0;
  adata->status = 0;
  memset(adata->cmds, 0, sizeof(struct ImapCommand) * adata->cmdslots);
}

/**
 * imap_has_flag - Does the flag exist in the list
 * @param flag_list List of server flags
 * @param flag      Flag to find
 * @retval true Flag exists
 *
 * Do a caseless comparison of the flag against a flag list, return true if
 * found or flag list has '\*'. Note that "flag" might contain additional
 * whitespace at the end, so we really need to compare up to the length of each
 * element in "flag_list".
 */
bool imap_has_flag(struct ListHead *flag_list, const char *flag)
{
  if (STAILQ_EMPTY(flag_list))
    return false;

  const size_t flaglen = mutt_str_len(flag);
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, flag_list, entries)
  {
    const size_t nplen = strlen(np->data);
    if ((flaglen >= nplen) && ((flag[nplen] == '\0') || (flag[nplen] == ' ')) &&
        mutt_istrn_equal(np->data, flag, nplen))
    {
      return true;
    }

    if (mutt_str_equal(np->data, "\\*"))
      return true;
  }

  return false;
}

/**
 * imap_sort_email_uid - Compare two Emails by UID - Implements ::sort_t - @ingroup sort_api
 */
static int imap_sort_email_uid(const void *a, const void *b, void *sdata)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;

  const unsigned int ua = imap_edata_get((struct Email *) ea)->uid;
  const unsigned int ub = imap_edata_get((struct Email *) eb)->uid;

  return mutt_numeric_cmp(ua, ub);
}

/**
 * imap_sync_message_for_copy - Update server to reflect the flags of a single message
 * @param[in]  m            Mailbox
 * @param[in]  e            Email
 * @param[in]  cmd          Buffer for the command string
 * @param[out] err_continue Did the user force a continue?
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Update the IMAP server to reflect the flags for a single message before
 * performing a "UID COPY".
 *
 * @note This does not sync the "deleted" flag state, because it is not
 *       desirable to propagate that flag into the copy.
 */
int imap_sync_message_for_copy(struct Mailbox *m, struct Email *e,
                               struct Buffer *cmd, enum QuadOption *err_continue)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapEmailData *edata = imap_edata_get(e);

  if (!adata || (adata->mailbox != m) || !e)
    return -1;

  if (!compare_flags_for_copy(e))
  {
    if (e->deleted == edata->deleted)
      e->changed = false;
    return 0;
  }

  buf_printf(cmd, "UID STORE %u", edata->uid);

  struct Buffer *flags = buf_pool_get();

  set_flag(m, MUTT_ACL_SEEN, e->read, "\\Seen ", flags);
  set_flag(m, MUTT_ACL_WRITE, e->old, "Old ", flags);
  set_flag(m, MUTT_ACL_WRITE, e->flagged, "\\Flagged ", flags);
  set_flag(m, MUTT_ACL_WRITE, e->replied, "\\Answered ", flags);
  set_flag(m, MUTT_ACL_DELETE, edata->deleted, "\\Deleted ", flags);

  if (m->rights & MUTT_ACL_WRITE)
  {
    /* restore system flags */
    if (edata->flags_system)
      buf_addstr(flags, edata->flags_system);

    /* set custom flags */
    struct Buffer *tags = buf_pool_get();
    driver_tags_get_with_hidden(&e->tags, tags);
    if (!buf_is_empty(tags))
      buf_addstr(flags, buf_string(tags));
    buf_pool_release(&tags);
  }

  mutt_str_remove_trailing_ws(flags->data);
  buf_fix_dptr(flags);

  /* UW-IMAP is OK with null flags, Cyrus isn't. The only solution is to
   * explicitly revoke all system flags (if we have permission) */
  if (buf_is_empty(flags))
  {
    set_flag(m, MUTT_ACL_SEEN, true, "\\Seen ", flags);
    set_flag(m, MUTT_ACL_WRITE, true, "Old ", flags);
    set_flag(m, MUTT_ACL_WRITE, true, "\\Flagged ", flags);
    set_flag(m, MUTT_ACL_WRITE, true, "\\Answered ", flags);
    set_flag(m, MUTT_ACL_DELETE, !edata->deleted, "\\Deleted ", flags);

    /* erase custom flags */
    if ((m->rights & MUTT_ACL_WRITE) && edata->flags_remote)
      buf_addstr(flags, edata->flags_remote);

    mutt_str_remove_trailing_ws(flags->data);
    buf_fix_dptr(flags);

    buf_addstr(cmd, " -FLAGS.SILENT (");
  }
  else
  {
    buf_addstr(cmd, " FLAGS.SILENT (");
  }

  buf_addstr(cmd, buf_string(flags));
  buf_addstr(cmd, ")");

  int rc = -1;

  /* after all this it's still possible to have no flags, if you
   * have no ACL rights */
  if (!buf_is_empty(flags) &&
      (imap_exec(adata, cmd->data, IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS) &&
      err_continue && (*err_continue != MUTT_YES))
  {
    *err_continue = imap_continue("imap_sync_message: STORE failed", adata->buf);
    if (*err_continue != MUTT_YES)
      goto done;
  }

  /* server have now the updated flags */
  FREE(&edata->flags_remote);
  struct Buffer *flags_remote = buf_pool_get();
  driver_tags_get_with_hidden(&e->tags, flags_remote);
  edata->flags_remote = buf_strdup(flags_remote);
  buf_pool_release(&flags_remote);

  if (e->deleted == edata->deleted)
    e->changed = false;

  rc = 0;

done:
  buf_pool_release(&flags);
  return rc;
}

/**
 * imap_check_mailbox - Use the NOOP or IDLE command to poll for new mail
 * @param m     Mailbox
 * @param force Don't wait
 * @retval num MxStatus
 */
enum MxStatus imap_check_mailbox(struct Mailbox *m, bool force)
{
  if (!m || !m->account)
    return MX_STATUS_ERROR;

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  /* overload keyboard timeout to avoid many mailbox checks in a row.
   * Most users don't like having to wait exactly when they press a key. */
  int rc = 0;

  /* try IDLE first, unless force is set */
  const bool c_imap_idle = cs_subset_bool(NeoMutt->sub, "imap_idle");
  const short c_imap_keep_alive = cs_subset_number(NeoMutt->sub, "imap_keep_alive");
  if (!force && c_imap_idle && (adata->capabilities & IMAP_CAP_IDLE) &&
      ((adata->state != IMAP_IDLE) || (mutt_date_now() >= adata->lastread + c_imap_keep_alive)))
  {
    if (imap_cmd_idle(adata) < 0)
      return MX_STATUS_ERROR;
  }
  if (adata->state == IMAP_IDLE)
  {
    while ((rc = mutt_socket_poll(adata->conn, 0)) > 0)
    {
      if (imap_cmd_step(adata) != IMAP_RES_CONTINUE)
      {
        mutt_debug(LL_DEBUG1, "Error reading IDLE response\n");
        return MX_STATUS_ERROR;
      }
    }
    if (rc < 0)
    {
      mutt_debug(LL_DEBUG1, "Poll failed, disabling IDLE\n");
      adata->capabilities &= ~IMAP_CAP_IDLE; // Clear the flag
    }
  }

  const short c_timeout = cs_subset_number(NeoMutt->sub, "timeout");
  if ((force || ((adata->state != IMAP_IDLE) && (mutt_date_now() >= adata->lastread + c_timeout))) &&
      (imap_exec(adata, "NOOP", IMAP_CMD_POLL) != IMAP_EXEC_SUCCESS))
  {
    return MX_STATUS_ERROR;
  }

  /* We call this even when we haven't run NOOP in case we have pending
   * changes to process, since we can reopen here. */
  imap_cmd_finish(adata);

  enum MxStatus check = MX_STATUS_OK;
  if (mdata->check_status & IMAP_EXPUNGE_PENDING)
    check = MX_STATUS_REOPENED;
  else if (mdata->check_status & IMAP_NEWMAIL_PENDING)
    check = MX_STATUS_NEW_MAIL;
  else if (mdata->check_status & IMAP_FLAGS_PENDING)
    check = MX_STATUS_FLAGS;
  else if (rc < 0)
    check = MX_STATUS_ERROR;

  mdata->check_status = IMAP_OPEN_NO_FLAGS;

  if (force)
    m->last_checked = 0; // force a check on the next mx_mbox_check() call
  return check;
}

/**
 * imap_status - Refresh the number of total and new messages
 * @param adata  IMAP Account data
 * @param mdata  IMAP Mailbox data
 * @param queue  Queue the STATUS command
 * @retval num   Total number of messages
 */
static int imap_status(struct ImapAccountData *adata, struct ImapMboxData *mdata, bool queue)
{
  char *uidvalidity_flag = NULL;
  char cmd[2048] = { 0 };

  if (!adata || !mdata)
    return -1;

  /* Don't issue STATUS on the selected mailbox, it will be NOOPed or
   * IDLEd elsewhere.
   * adata->mailbox may be NULL for connections other than the current
   * mailbox's. */
  if (adata->mailbox && (adata->mailbox->mdata == mdata))
  {
    adata->mailbox->has_new = false;
    return mdata->messages;
  }

  if (adata->mailbox && !adata->mailbox->poll_new_mail)
    return mdata->messages;

  if (adata->capabilities & IMAP_CAP_IMAP4REV1)
  {
    uidvalidity_flag = "UIDVALIDITY";
  }
  else if (adata->capabilities & IMAP_CAP_STATUS)
  {
    uidvalidity_flag = "UID-VALIDITY";
  }
  else
  {
    mutt_debug(LL_DEBUG2, "Server doesn't support STATUS\n");
    return -1;
  }

  snprintf(cmd, sizeof(cmd), "STATUS %s (UIDNEXT %s UNSEEN RECENT MESSAGES)",
           mdata->munge_name, uidvalidity_flag);

  int rc = imap_exec(adata, cmd, queue ? IMAP_CMD_QUEUE : IMAP_CMD_POLL);
  if (rc != IMAP_EXEC_SUCCESS)
  {
    mutt_debug(LL_DEBUG1, "Error queueing command\n");
    return rc;
  }
  return mdata->messages;
}

/**
 * imap_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::mbox_check_stats() - @ingroup mx_mbox_check_stats
 */
static enum MxStatus imap_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  const bool queue = (flags & MUTT_MAILBOX_CHECK_IMMEDIATE) == 0;
  const int new_msgs = imap_mailbox_status(m, queue);
  if (new_msgs == -1)
    return MX_STATUS_ERROR;
  if (new_msgs == 0)
    return MX_STATUS_OK;
  return MX_STATUS_NEW_MAIL;
}

/**
 * imap_path_status - Refresh the number of total and new messages
 * @param path   Mailbox path
 * @param queue  Queue the STATUS command
 * @retval num   Total number of messages
 */
int imap_path_status(const char *path, bool queue)
{
  struct Mailbox *m = mx_mbox_find2(path);

  const bool is_temp = !m;
  if (is_temp)
  {
    m = mx_path_resolve(path);
    if (!mx_mbox_ac_link(m))
    {
      mailbox_free(&m);
      return 0;
    }
  }

  int rc = imap_mailbox_status(m, queue);

  if (is_temp)
  {
    mx_ac_remove(m, false);
    mailbox_free(&m);
  }

  return rc;
}

/**
 * imap_mailbox_status - Refresh the number of total and new messages
 * @param m      Mailbox
 * @param queue  Queue the STATUS command
 * @retval num Total number of messages
 * @retval -1  Error
 *
 * @note Prepare the mailbox if we are not connected
 */
int imap_mailbox_status(struct Mailbox *m, bool queue)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || !mdata)
    return -1;
  return imap_status(adata, mdata, queue);
}

/**
 * imap_subscribe - Subscribe to a mailbox
 * @param path      Mailbox path
 * @param subscribe True: subscribe, false: unsubscribe
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_subscribe(const char *path, bool subscribe)
{
  struct ImapAccountData *adata = NULL;
  struct ImapMboxData *mdata = NULL;

  if (imap_adata_find(path, &adata, &mdata) < 0)
    return -1;

  if (subscribe)
    mutt_message(_("Subscribing to %s..."), mdata->name);
  else
    mutt_message(_("Unsubscribing from %s..."), mdata->name);

  char buf[2048] = { 0 };
  snprintf(buf, sizeof(buf), "%sSUBSCRIBE %s", subscribe ? "" : "UN", mdata->munge_name);

  if (imap_exec(adata, buf, IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
  {
    imap_mdata_free((void *) &mdata);
    return -1;
  }

  const bool c_imap_check_subscribed = cs_subset_bool(NeoMutt->sub, "imap_check_subscribed");
  if (c_imap_check_subscribed)
  {
    char mbox[1024] = { 0 };
    size_t len = snprintf(mbox, sizeof(mbox), "%smailboxes ", subscribe ? "" : "un");
    imap_quote_string(mbox + len, sizeof(mbox) - len, path, true);
    struct Buffer *err = buf_pool_get();
    if (parse_rc_line(mbox, err))
      mutt_debug(LL_DEBUG1, "Error adding subscribed mailbox: %s\n", buf_string(err));
    buf_pool_release(&err);
  }

  if (subscribe)
    mutt_message(_("Subscribed to %s"), mdata->name);
  else
    mutt_message(_("Unsubscribed from %s"), mdata->name);
  imap_mdata_free((void *) &mdata);
  return 0;
}

/**
 * imap_complete - Try to complete an IMAP folder path
 * @param buf  Buffer for result
 * @param path Partial mailbox name to complete
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Given a partial IMAP folder path, return a string which adds as much to the
 * path as is unique
 */
int imap_complete(struct Buffer *buf, const char *path)
{
  struct ImapAccountData *adata = NULL;
  struct ImapMboxData *mdata = NULL;
  char tmp[2048] = { 0 };
  struct ImapList listresp = { 0 };
  struct Buffer *completion_buf = NULL;
  size_t clen;
  int completions = 0;
  int rc;

  if (imap_adata_find(path, &adata, &mdata) < 0)
  {
    buf_strcpy(buf, path);
    return complete_hosts(buf);
  }

  /* fire off command */
  const bool c_imap_list_subscribed = cs_subset_bool(NeoMutt->sub, "imap_list_subscribed");
  snprintf(tmp, sizeof(tmp), "%s \"\" \"%s%%\"",
           c_imap_list_subscribed ? "LSUB" : "LIST", mdata->real_name);

  imap_cmd_start(adata, tmp);

  /* and see what the results are */
  completion_buf = buf_pool_get();
  buf_strcpy(completion_buf, mdata->name);
  imap_mdata_free((void *) &mdata);

  adata->cmdresult = &listresp;
  do
  {
    listresp.name = NULL;
    rc = imap_cmd_step(adata);

    if ((rc == IMAP_RES_CONTINUE) && listresp.name)
    {
      /* if the folder isn't selectable, append delimiter to force browse
       * to enter it on second tab. */
      if (listresp.noselect)
      {
        clen = strlen(listresp.name);
        listresp.name[clen++] = listresp.delim;
        listresp.name[clen] = '\0';
      }
      /* copy in first word */
      if (!completions)
      {
        buf_strcpy(completion_buf, listresp.name);
        completions++;
        continue;
      }

      longest_common_prefix(completion_buf, listresp.name, 0);
      completions++;
    }
  } while (rc == IMAP_RES_CONTINUE);
  adata->cmdresult = NULL;

  if (completions)
  {
    /* reformat output */
    imap_buf_qualify_path(buf, &adata->conn->account, completion_buf->data);
    buf_pretty_mailbox(buf);
    buf_fix_dptr(buf);
    buf_pool_release(&completion_buf);
    return 0;
  }

  buf_pool_release(&completion_buf);
  return -1;
}

/**
 * imap_fast_trash - Use server COPY command to copy deleted messages to trash
 * @param m    Mailbox
 * @param dest Mailbox to move to
 * @retval -1 Error
 * @retval  0 Success
 * @retval  1 Non-fatal error - try fetch/append
 */
int imap_fast_trash(struct Mailbox *m, const char *dest)
{
  char prompt[1024] = { 0 };
  int rc = -1;
  bool triedcreate = false;
  enum QuadOption err_continue = MUTT_NO;

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapAccountData *dest_adata = NULL;
  struct ImapMboxData *dest_mdata = NULL;

  if (imap_adata_find(dest, &dest_adata, &dest_mdata) < 0)
    return -1;

  struct Buffer *sync_cmd = buf_pool_get();

  /* check that the save-to folder is in the same account */
  if (!imap_account_match(&(adata->conn->account), &(dest_adata->conn->account)))
  {
    mutt_debug(LL_DEBUG3, "%s not same server as %s\n", dest, mailbox_path(m));
    goto out;
  }

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    if (e->active && e->changed && e->deleted && !e->purge)
    {
      rc = imap_sync_message_for_copy(m, e, sync_cmd, &err_continue);
      if (rc < 0)
      {
        mutt_debug(LL_DEBUG1, "could not sync\n");
        goto out;
      }
    }
  }

  /* loop in case of TRYCREATE */
  do
  {
    struct UidArray uida = ARRAY_HEAD_INITIALIZER;
    select_email_uids(m->emails, m->msg_count, MUTT_TRASH, false, false, &uida);
    ARRAY_SORT(&uida, imap_sort_uid, NULL);
    rc = imap_exec_msg_set(adata, "UID COPY", dest_mdata->munge_name, &uida);
    if (rc == 0)
    {
      mutt_debug(LL_DEBUG1, "No messages to trash\n");
      rc = -1;
      goto out;
    }
    else if (rc < 0)
    {
      mutt_debug(LL_DEBUG1, "could not queue copy\n");
      goto out;
    }
    else if (m->verbose)
    {
      mutt_message(ngettext("Copying %d message to %s...", "Copying %d messages to %s...", rc),
                   rc, dest_mdata->name);
    }
    ARRAY_FREE(&uida);

    /* let's get it on */
    rc = imap_exec(adata, NULL, IMAP_CMD_NO_FLAGS);
    if (rc == IMAP_EXEC_ERROR)
    {
      if (triedcreate)
      {
        mutt_debug(LL_DEBUG1, "Already tried to create mailbox %s\n", dest_mdata->name);
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (!mutt_istr_startswith(imap_get_qualifier(adata->buf), "[TRYCREATE]"))
        break;
      mutt_debug(LL_DEBUG3, "server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), dest_mdata->name);
      const bool c_confirm_create = cs_subset_bool(NeoMutt->sub, "confirm_create");
      if (c_confirm_create &&
          (query_yesorno_help(prompt, MUTT_YES, NeoMutt->sub, "confirm_create") != MUTT_YES))
      {
        mutt_clear_error();
        goto out;
      }
      if (imap_create_mailbox(adata, dest_mdata->name) < 0)
        break;
      triedcreate = true;
    }
  } while (rc == IMAP_EXEC_ERROR);

  if (rc != IMAP_EXEC_SUCCESS)
  {
    imap_error("imap_fast_trash", adata->buf);
    goto out;
  }

  rc = IMAP_EXEC_SUCCESS;

out:
  buf_pool_release(&sync_cmd);
  imap_mdata_free((void *) &dest_mdata);

  return ((rc == IMAP_EXEC_SUCCESS) ? 0 : -1);
}

/**
 * imap_sync_mailbox - Sync all the changes to the server
 * @param m       Mailbox
 * @param expunge if true do expunge
 * @param close   if true we move imap state to CLOSE
 * @retval enum #MxStatus
 *
 * @note The flag retvals come from a call to imap_check_mailbox()
 */
enum MxStatus imap_sync_mailbox(struct Mailbox *m, bool expunge, bool close)
{
  if (!m)
    return -1;

  struct Email **emails = NULL;
  int rc;

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (adata->state < IMAP_SELECTED)
  {
    mutt_debug(LL_DEBUG2, "no mailbox selected\n");
    return -1;
  }

  /* This function is only called when the calling code expects the context
   * to be changed. */
  imap_allow_reopen(m);

  enum MxStatus check = imap_check_mailbox(m, false);
  if (check == MX_STATUS_ERROR)
    return check;

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && (m->rights & MUTT_ACL_DELETE))
  {
    struct UidArray uida = ARRAY_HEAD_INITIALIZER;
    select_email_uids(m->emails, m->msg_count, MUTT_DELETED, true, false, &uida);
    ARRAY_SORT(&uida, imap_sort_uid, NULL);
    rc = imap_exec_msg_set(adata, "UID STORE", "+FLAGS.SILENT (\\Deleted)", &uida);
    ARRAY_FREE(&uida);
    if (rc < 0)
    {
      mutt_error(_("Expunge failed"));
      return rc;
    }

    if (rc > 0)
    {
      /* mark these messages as unchanged so second pass ignores them. Done
       * here so BOGUS UW-IMAP 4.7 SILENT FLAGS updates are ignored. */
      for (int i = 0; i < m->msg_count; i++)
      {
        struct Email *e = m->emails[i];
        if (!e)
          break;
        if (e->deleted && e->changed)
          e->active = false;
      }
      if (m->verbose)
      {
        mutt_message(ngettext("Marking %d message deleted...",
                              "Marking %d messages deleted...", rc),
                     rc);
      }
    }
  }

#ifdef USE_HCACHE
  imap_hcache_open(adata, mdata, true);
#endif

  /* save messages with real (non-flag) changes */
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    if (e->deleted)
    {
      imap_cache_del(m, e);
#ifdef USE_HCACHE
      imap_hcache_del(mdata, imap_edata_get(e)->uid);
#endif
    }

    if (e->active && e->changed)
    {
#ifdef USE_HCACHE
      imap_hcache_put(mdata, e);
#endif
      /* if the message has been rethreaded or attachments have been deleted
       * we delete the message and reupload it.
       * This works better if we're expunging, of course. */
      if (e->env->changed || e->attach_del)
      {
        /* L10N: The plural is chosen by the last %d, i.e. the total number */
        if (m->verbose)
        {
          mutt_message(ngettext("Saving changed message... [%d/%d]",
                                "Saving changed messages... [%d/%d]", m->msg_count),
                       i + 1, m->msg_count);
        }
        bool save_append = m->append;
        m->append = true;
        mutt_save_message_mbox(m, e, SAVE_MOVE, TRANSFORM_NONE, m);
        m->append = save_append;
        e->env->changed = false;
      }
    }
  }

#ifdef USE_HCACHE
  imap_hcache_close(mdata);
#endif

  /* presort here to avoid doing 10 resorts in imap_exec_msg_set */
  emails = MUTT_MEM_MALLOC(m->msg_count, struct Email *);
  memcpy(emails, m->emails, m->msg_count * sizeof(struct Email *));
  mutt_qsort_r(emails, m->msg_count, sizeof(struct Email *), imap_sort_email_uid, NULL);

  rc = sync_helper(m, emails, m->msg_count, MUTT_ACL_DELETE, MUTT_DELETED, "\\Deleted");
  if (rc >= 0)
    rc |= sync_helper(m, emails, m->msg_count, MUTT_ACL_WRITE, MUTT_FLAG, "\\Flagged");
  if (rc >= 0)
    rc |= sync_helper(m, emails, m->msg_count, MUTT_ACL_WRITE, MUTT_OLD, "Old");
  if (rc >= 0)
    rc |= sync_helper(m, emails, m->msg_count, MUTT_ACL_SEEN, MUTT_READ, "\\Seen");
  if (rc >= 0)
    rc |= sync_helper(m, emails, m->msg_count, MUTT_ACL_WRITE, MUTT_REPLIED, "\\Answered");

  FREE(&emails);

  /* Flush the queued flags if any were changed in sync_helper. */
  if (rc > 0)
    if (imap_exec(adata, NULL, IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
      rc = -1;

  if (rc < 0)
  {
    if (close)
    {
      if (query_yesorno(_("Error saving flags. Close anyway?"), MUTT_NO) == MUTT_YES)
      {
        adata->state = IMAP_AUTHENTICATED;
        return 0;
      }
    }
    else
    {
      mutt_error(_("Error saving flags"));
    }
    return -1;
  }

  /* Update local record of server state to reflect the synchronization just
   * completed.  imap_read_headers always overwrites hcache-origin flags, so
   * there is no need to mutate the hcache after flag-only changes. */
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    struct ImapEmailData *edata = imap_edata_get(e);
    edata->deleted = e->deleted;
    edata->flagged = e->flagged;
    edata->old = e->old;
    edata->read = e->read;
    edata->replied = e->replied;
    e->changed = false;
  }
  m->changed = false;

  /* We must send an EXPUNGE command if we're not closing. */
  if (expunge && !close && (m->rights & MUTT_ACL_DELETE))
  {
    if (m->verbose)
      mutt_message(_("Expunging messages from server..."));
    /* Set expunge bit so we don't get spurious reopened messages */
    mdata->reopen |= IMAP_EXPUNGE_EXPECTED;
    if (imap_exec(adata, "EXPUNGE", IMAP_CMD_NO_FLAGS) != IMAP_EXEC_SUCCESS)
    {
      mdata->reopen &= ~IMAP_EXPUNGE_EXPECTED;
      imap_error(_("imap_sync_mailbox: EXPUNGE failed"), adata->buf);
      return -1;
    }
    mdata->reopen &= ~IMAP_EXPUNGE_EXPECTED;
  }

  if (expunge && close)
  {
    adata->closing = true;
    imap_exec(adata, "CLOSE", IMAP_CMD_NO_FLAGS);
    adata->state = IMAP_AUTHENTICATED;
  }

  const bool c_message_cache_clean = cs_subset_bool(NeoMutt->sub, "message_cache_clean");
  if (c_message_cache_clean)
    imap_cache_clean(m);

  return check;
}

/**
 * imap_ac_owns_path - Check whether an Account owns a Mailbox path - Implements MxOps::ac_owns_path() - @ingroup mx_ac_owns_path
 */
static bool imap_ac_owns_path(struct Account *a, const char *path)
{
  struct Url *url = url_parse(path);
  if (!url)
    return false;

  struct ImapAccountData *adata = a->adata;
  struct ConnAccount *cac = &adata->conn->account;

  const bool rc = mutt_istr_equal(url->host, cac->host) &&
                  (!url->user || mutt_istr_equal(url->user, cac->user));
  url_free(&url);
  return rc;
}

/**
 * imap_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add() - @ingroup mx_ac_add
 */
static bool imap_ac_add(struct Account *a, struct Mailbox *m)
{
  struct ImapAccountData *adata = a->adata;

  if (!adata)
  {
    struct ConnAccount cac = { { 0 } };
    char mailbox[PATH_MAX] = { 0 };

    if (imap_parse_path(mailbox_path(m), &cac, mailbox, sizeof(mailbox)) < 0)
      return false;

    adata = imap_adata_new(a);
    adata->conn = mutt_conn_new(&cac);
    if (!adata->conn)
    {
      imap_adata_free((void **) &adata);
      return false;
    }

    mutt_account_hook(m->realpath);

    if (imap_login(adata) < 0)
    {
      imap_adata_free((void **) &adata);
      return false;
    }

    a->adata = adata;
    a->adata_free = imap_adata_free;
  }

  if (!m->mdata)
  {
    struct Url *url = url_parse(mailbox_path(m));
    if (!url)
      return false;
    struct ImapMboxData *mdata = imap_mdata_new(adata, url->path);

    /* fixup path and realpath, mainly to replace / by /INBOX */
    char buf[1024] = { 0 };
    imap_qualify_path(buf, sizeof(buf), &adata->conn->account, mdata->name);
    buf_strcpy(&m->pathbuf, buf);
    mutt_str_replace(&m->realpath, mailbox_path(m));

    m->mdata = mdata;
    m->mdata_free = imap_mdata_free;
    url_free(&url);
  }
  return true;
}

/**
 * imap_mbox_select - Select a Mailbox
 * @param m Mailbox
 */
static void imap_mbox_select(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || !mdata)
    return;

  const char *condstore = NULL;
#ifdef USE_HCACHE
  const bool c_imap_condstore = cs_subset_bool(NeoMutt->sub, "imap_condstore");
  if ((adata->capabilities & IMAP_CAP_CONDSTORE) && c_imap_condstore)
    condstore = " (CONDSTORE)";
  else
#endif
    condstore = "";

  char buf[PATH_MAX] = { 0 };
  snprintf(buf, sizeof(buf), "%s %s%s", m->readonly ? "EXAMINE" : "SELECT",
           mdata->munge_name, condstore);

  adata->state = IMAP_SELECTED;

  imap_cmd_start(adata, buf);
}

/**
 * imap_login - Open an IMAP connection
 * @param adata Imap Account data
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Ensure ImapAccountData is connected and logged into the imap server.
 */
int imap_login(struct ImapAccountData *adata)
{
  if (!adata)
    return -1;

  if (adata->state == IMAP_DISCONNECTED)
  {
    buf_reset(&adata->cmdbuf); // purge outstanding queued commands
    imap_open_connection(adata);
  }
  if (adata->state == IMAP_CONNECTED)
  {
    if (imap_authenticate(adata) == IMAP_AUTH_SUCCESS)
    {
      adata->state = IMAP_AUTHENTICATED;
      FREE(&adata->capstr);
      if (adata->conn->ssf != 0)
      {
        mutt_debug(LL_DEBUG2, "Communication encrypted at %d bits\n",
                   adata->conn->ssf);
      }
    }
    else
    {
      mutt_account_unsetpass(&adata->conn->account);
    }
  }
  if (adata->state == IMAP_AUTHENTICATED)
  {
    /* capabilities may have changed */
    imap_exec(adata, "CAPABILITY", IMAP_CMD_PASS);

#ifdef USE_ZLIB
    /* RFC4978 */
    const bool c_imap_deflate = cs_subset_bool(NeoMutt->sub, "imap_deflate");
    if ((adata->capabilities & IMAP_CAP_COMPRESS) && c_imap_deflate &&
        (imap_exec(adata, "COMPRESS DEFLATE", IMAP_CMD_PASS) == IMAP_EXEC_SUCCESS))
    {
      mutt_debug(LL_DEBUG2, "IMAP compression is enabled on connection to %s\n",
                 adata->conn->account.host);
      mutt_zstrm_wrap_conn(adata->conn);
    }
#endif

    /* enable RFC2971, if the server supports that */
    const bool c_imap_send_id = cs_subset_bool(NeoMutt->sub, "imap_send_id");
    if (c_imap_send_id && (adata->capabilities & IMAP_CAP_ID))
    {
      imap_exec(adata, "ID (\"name\" \"NeoMutt\" \"version\" \"" PACKAGE_VERSION "\")",
                IMAP_CMD_QUEUE);
    }

    /* enable RFC6855, if the server supports that */
    const bool c_imap_rfc5161 = cs_subset_bool(NeoMutt->sub, "imap_rfc5161");
    if (c_imap_rfc5161 && (adata->capabilities & IMAP_CAP_ENABLE))
      imap_exec(adata, "ENABLE UTF8=ACCEPT", IMAP_CMD_QUEUE);

    /* enable QRESYNC.  Advertising QRESYNC also means CONDSTORE
     * is supported (even if not advertised), so flip that bit. */
    if (adata->capabilities & IMAP_CAP_QRESYNC)
    {
      adata->capabilities |= IMAP_CAP_CONDSTORE;
      const bool c_imap_qresync = cs_subset_bool(NeoMutt->sub, "imap_qresync");
      if (c_imap_rfc5161 && c_imap_qresync)
        imap_exec(adata, "ENABLE QRESYNC", IMAP_CMD_QUEUE);
    }

    /* get root delimiter, '/' as default */
    adata->delim = '/';
    imap_exec(adata, "LIST \"\" \"\"", IMAP_CMD_QUEUE);

    /* we may need the root delimiter before we open a mailbox */
    imap_exec(adata, NULL, IMAP_CMD_NO_FLAGS);

    /* select the mailbox that used to be open before disconnect */
    if (adata->mailbox)
    {
      imap_mbox_select(adata->mailbox);
    }
  }

  if (adata->state < IMAP_AUTHENTICATED)
    return -1;

  return 0;
}

/**
 * imap_mbox_open - Open a mailbox - Implements MxOps::mbox_open() - @ingroup mx_mbox_open
 */
static enum MxOpenReturns imap_mbox_open(struct Mailbox *m)
{
  if (!m->account || !m->mdata)
    return MX_OPEN_ERROR;

  char buf[PATH_MAX] = { 0 };
  int count = 0;
  int rc;

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  mutt_debug(LL_DEBUG3, "opening %s, saving %s\n", m->pathbuf.data,
             (adata->mailbox ? adata->mailbox->pathbuf.data : "(none)"));
  adata->prev_mailbox = adata->mailbox;
  adata->mailbox = m;

  /* clear mailbox status */
  adata->status = 0;
  m->rights = 0;
  mdata->new_mail_count = 0;

  if (m->verbose)
    mutt_message(_("Selecting %s..."), mdata->name);

  /* pipeline ACL test */
  if (adata->capabilities & IMAP_CAP_ACL)
  {
    snprintf(buf, sizeof(buf), "MYRIGHTS %s", mdata->munge_name);
    imap_exec(adata, buf, IMAP_CMD_QUEUE);
  }
  else
  {
    /* assume we have all rights if ACL is unavailable */
    m->rights |= MUTT_ACL_LOOKUP | MUTT_ACL_READ | MUTT_ACL_SEEN | MUTT_ACL_WRITE |
                 MUTT_ACL_INSERT | MUTT_ACL_POST | MUTT_ACL_CREATE | MUTT_ACL_DELETE;
  }

  /* pipeline the postponed count if possible */
  const char *const c_postponed = cs_subset_string(NeoMutt->sub, "postponed");
  struct Mailbox *m_postponed = mx_mbox_find2(c_postponed);
  struct ImapAccountData *postponed_adata = imap_adata_get(m_postponed);
  if (postponed_adata &&
      imap_account_match(&postponed_adata->conn->account, &adata->conn->account))
  {
    imap_mailbox_status(m_postponed, true);
  }

  const bool c_imap_check_subscribed = cs_subset_bool(NeoMutt->sub, "imap_check_subscribed");
  if (c_imap_check_subscribed)
    imap_exec(adata, "LSUB \"\" \"*\"", IMAP_CMD_QUEUE);

  imap_mbox_select(m);

  do
  {
    char *pc = NULL;

    rc = imap_cmd_step(adata);
    if (rc != IMAP_RES_CONTINUE)
      break;

    if (!mutt_strn_equal(adata->buf, "* ", 2))
      continue;
    pc = imap_next_word(adata->buf);

    /* Obtain list of available flags here, may be overridden by a
     * PERMANENTFLAGS tag in the OK response */
    if (mutt_istr_startswith(pc, "FLAGS"))
    {
      /* don't override PERMANENTFLAGS */
      if (STAILQ_EMPTY(&mdata->flags))
      {
        mutt_debug(LL_DEBUG3, "Getting mailbox FLAGS\n");
        pc = get_flags(&mdata->flags, pc);
        if (!pc)
          goto fail;
      }
    }
    else if (mutt_istr_startswith(pc, "OK [PERMANENTFLAGS"))
    {
      /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
      mutt_debug(LL_DEBUG3, "Getting mailbox PERMANENTFLAGS\n");
      /* safe to call on NULL */
      mutt_list_free(&mdata->flags);
      /* skip "OK [PERMANENT" so syntax is the same as FLAGS */
      pc += 13;
      pc = get_flags(&(mdata->flags), pc);
      if (!pc)
        goto fail;
    }
    else if (mutt_istr_startswith(pc, "OK [UIDVALIDITY"))
    {
      /* save UIDVALIDITY for the header cache */
      mutt_debug(LL_DEBUG3, "Getting mailbox UIDVALIDITY\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (!mutt_str_atoui(pc, &mdata->uidvalidity))
        goto fail;
    }
    else if (mutt_istr_startswith(pc, "OK [UIDNEXT"))
    {
      mutt_debug(LL_DEBUG3, "Getting mailbox UIDNEXT\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (!mutt_str_atoui(pc, &mdata->uid_next))
        goto fail;
    }
    else if (mutt_istr_startswith(pc, "OK [HIGHESTMODSEQ"))
    {
      mutt_debug(LL_DEBUG3, "Getting mailbox HIGHESTMODSEQ\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (!mutt_str_atoull(pc, &mdata->modseq))
        goto fail;
    }
    else if (mutt_istr_startswith(pc, "OK [NOMODSEQ"))
    {
      mutt_debug(LL_DEBUG3, "Mailbox has NOMODSEQ set\n");
      mdata->modseq = 0;
    }
    else
    {
      pc = imap_next_word(pc);
      if (mutt_istr_startswith(pc, "EXISTS"))
      {
        count = mdata->new_mail_count;
        mdata->new_mail_count = 0;
      }
    }
  } while (rc == IMAP_RES_CONTINUE);

  if (rc == IMAP_RES_NO)
  {
    char *s = imap_next_word(adata->buf); /* skip seq */
    s = imap_next_word(s);                /* Skip response */
    mutt_error("%s", s);
    goto fail;
  }

  if (rc != IMAP_RES_OK)
    goto fail;

  /* check for READ-ONLY notification */
  if (mutt_istr_startswith(imap_get_qualifier(adata->buf), "[READ-ONLY]") &&
      !(adata->capabilities & IMAP_CAP_ACL))
  {
    mutt_debug(LL_DEBUG2, "Mailbox is read-only\n");
    m->readonly = true;
  }

  /* dump the mailbox flags we've found */
  const short c_debug_level = cs_subset_number(NeoMutt->sub, "debug_level");
  if (c_debug_level > LL_DEBUG2)
  {
    if (STAILQ_EMPTY(&mdata->flags))
    {
      mutt_debug(LL_DEBUG3, "No folder flags found\n");
    }
    else
    {
      struct ListNode *np = NULL;
      struct Buffer *flag_buffer = buf_pool_get();
      buf_printf(flag_buffer, "Mailbox flags: ");
      STAILQ_FOREACH(np, &mdata->flags, entries)
      {
        buf_add_printf(flag_buffer, "[%s] ", np->data);
      }
      mutt_debug(LL_DEBUG3, "%s\n", buf_string(flag_buffer));
      buf_pool_release(&flag_buffer);
    }
  }

  if (!((m->rights & MUTT_ACL_DELETE) || (m->rights & MUTT_ACL_SEEN) ||
        (m->rights & MUTT_ACL_WRITE) || (m->rights & MUTT_ACL_INSERT)))
  {
    m->readonly = true;
  }

  mx_alloc_memory(m, count);

  m->msg_count = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->msg_new = 0;
  m->msg_deleted = 0;
  m->size = 0;
  m->vcount = 0;

  if ((count > 0) && (imap_read_headers(m, 1, count, true) < 0))
  {
    mutt_error(_("Error opening mailbox"));
    goto fail;
  }

  mutt_debug(LL_DEBUG2, "msg_count is %d\n", m->msg_count);
  return MX_OPEN_OK;

fail:
  if (adata->state == IMAP_SELECTED)
    adata->state = IMAP_AUTHENTICATED;
  return MX_OPEN_ERROR;
}

/**
 * imap_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append() - @ingroup mx_mbox_open_append
 */
static bool imap_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!m->account)
    return false;

  /* in APPEND mode, we appear to hijack an existing IMAP connection -
   * mailbox is brand new and mostly empty */
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  int rc = imap_mailbox_status(m, false);
  if (rc >= 0)
    return true;
  if (rc == -1)
    return false;

  char buf[PATH_MAX + 64];
  snprintf(buf, sizeof(buf), _("Create %s?"), mdata->name);
  const bool c_confirm_create = cs_subset_bool(NeoMutt->sub, "confirm_create");
  if (c_confirm_create &&
      (query_yesorno_help(buf, MUTT_YES, NeoMutt->sub, "confirm_create") != MUTT_YES))
    return false;

  if (imap_create_mailbox(adata, mdata->name) < 0)
    return false;

  return true;
}

/**
 * imap_mbox_check - Check for new mail - Implements MxOps::mbox_check() - @ingroup mx_mbox_check
 * @param m Mailbox
 * @retval >0 Success, e.g. #MX_STATUS_REOPENED
 * @retval -1 Failure
 */
static enum MxStatus imap_mbox_check(struct Mailbox *m)
{
  imap_allow_reopen(m);
  enum MxStatus rc = imap_check_mailbox(m, false);
  /* NOTE - mv might have been changed at this point. In particular,
   * m could be NULL. Beware. */
  imap_disallow_reopen(m);

  return rc;
}

/**
 * imap_mbox_close - Close a Mailbox - Implements MxOps::mbox_close() - @ingroup mx_mbox_close
 */
static enum MxStatus imap_mbox_close(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  /* Check to see if the mailbox is actually open */
  if (!adata || !mdata)
    return MX_STATUS_OK;

  /* imap_mbox_open_append() borrows the struct ImapAccountData temporarily,
   * just for the connection.
   *
   * So when these are equal, it means we are actually closing the
   * mailbox and should clean up adata.  Otherwise, we don't want to
   * touch adata - it's still being used.  */
  if (m == adata->mailbox)
  {
    if ((adata->status != IMAP_FATAL) && (adata->state >= IMAP_SELECTED))
    {
      /* mx_mbox_close won't sync if there are no deleted messages
       * and the mailbox is unchanged, so we may have to close here */
      if (m->msg_deleted == 0)
      {
        adata->closing = true;
        imap_exec(adata, "CLOSE", IMAP_CMD_NO_FLAGS);
      }
      adata->state = IMAP_AUTHENTICATED;
    }

    mutt_debug(LL_DEBUG3, "closing %s, restoring %s\n", m->pathbuf.data,
               (adata->prev_mailbox ? adata->prev_mailbox->pathbuf.data : "(none)"));
    adata->mailbox = adata->prev_mailbox;
    imap_mbox_select(adata->prev_mailbox);
    imap_mdata_cache_reset(m->mdata);
  }

  return MX_STATUS_OK;
}

/**
 * imap_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new() - @ingroup mx_msg_open_new
 */
static bool imap_msg_open_new(struct Mailbox *m, struct Message *msg, const struct Email *e)
{
  bool success = false;

  struct Buffer *tmp = buf_pool_get();
  buf_mktemp(tmp);

  msg->fp = mutt_file_fopen(buf_string(tmp), "w");
  if (!msg->fp)
  {
    mutt_perror("%s", buf_string(tmp));
    goto cleanup;
  }

  msg->path = buf_strdup(tmp);
  success = true;

cleanup:
  buf_pool_release(&tmp);
  return success;
}

/**
 * imap_tags_edit - Prompt and validate new messages tags - Implements MxOps::tags_edit() - @ingroup mx_tags_edit
 */
static int imap_tags_edit(struct Mailbox *m, const char *tags, struct Buffer *buf)
{
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!mdata)
    return -1;

  char *new_tag = NULL;
  char *checker = NULL;

  /* Check for \* flags capability */
  if (!imap_has_flag(&mdata->flags, NULL))
  {
    mutt_error(_("IMAP server doesn't support custom flags"));
    return -1;
  }

  buf_reset(buf);
  if (tags)
    buf_strcpy(buf, tags);

  if (mw_get_field("Tags: ", buf, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) != 0)
    return -1;

  /* each keyword must be atom defined by rfc822 as:
   *
   * atom           = 1*<any CHAR except specials, SPACE and CTLs>
   * CHAR           = ( 0.-127. )
   * specials       = "(" / ")" / "<" / ">" / "@"
   *                  / "," / ";" / ":" / "\" / <">
   *                  / "." / "[" / "]"
   * SPACE          = ( 32. )
   * CTLS           = ( 0.-31., 127.)
   *
   * And must be separated by one space.
   */

  new_tag = buf->data;
  checker = buf->data;
  SKIPWS(checker);
  while (*checker != '\0')
  {
    if ((*checker < 32) || (*checker >= 127) || // We allow space because it's the separator
        (*checker == 40) ||                     // (
        (*checker == 41) ||                     // )
        (*checker == 60) ||                     // <
        (*checker == 62) ||                     // >
        (*checker == 64) ||                     // @
        (*checker == 44) ||                     // ,
        (*checker == 59) ||                     // ;
        (*checker == 58) ||                     // :
        (*checker == 92) ||                     // backslash
        (*checker == 34) ||                     // "
        (*checker == 46) ||                     // .
        (*checker == 91) ||                     // [
        (*checker == 93))                       // ]
    {
      mutt_error(_("Invalid IMAP flags"));
      return 0;
    }

    /* Skip duplicate space */
    while ((checker[0] == ' ') && (checker[1] == ' '))
      checker++;

    /* copy char to new_tag and go the next one */
    *new_tag++ = *checker++;
  }
  *new_tag = '\0';
  new_tag = buf->data; /* rewind */
  mutt_str_remove_trailing_ws(new_tag);

  return !mutt_str_equal(tags, buf_string(buf));
}

/**
 * imap_tags_commit - Save the tags to a message - Implements MxOps::tags_commit() - @ingroup mx_tags_commit
 *
 * This method update the server flags on the server by
 * removing the last know custom flags of a header
 * and adds the local flags
 *
 * If everything success we push the local flags to the
 * last know custom flags (flags_remote).
 *
 * Also this method check that each flags is support by the server
 * first and remove unsupported one.
 */
static int imap_tags_commit(struct Mailbox *m, struct Email *e, const char *buf)
{
  char uid[11] = { 0 };

  struct ImapAccountData *adata = imap_adata_get(m);

  if (*buf == '\0')
    buf = NULL;

  if (!(adata->mailbox->rights & MUTT_ACL_WRITE))
    return 0;

  snprintf(uid, sizeof(uid), "%u", imap_edata_get(e)->uid);

  /* Remove old custom flags */
  if (imap_edata_get(e)->flags_remote)
  {
    struct Buffer *cmd = buf_pool_get();
    buf_addstr(cmd, "UID STORE ");
    buf_addstr(cmd, uid);
    buf_addstr(cmd, " -FLAGS.SILENT (");
    buf_addstr(cmd, imap_edata_get(e)->flags_remote);
    buf_addstr(cmd, ")");

    /* Should we return here, or we are fine and we could
     * continue to add new flags */
    int rc = imap_exec(adata, buf_string(cmd), IMAP_CMD_NO_FLAGS);
    buf_pool_release(&cmd);
    if (rc != IMAP_EXEC_SUCCESS)
    {
      return -1;
    }
  }

  /* Add new custom flags */
  if (buf)
  {
    struct Buffer *cmd = buf_pool_get();
    buf_addstr(cmd, "UID STORE ");
    buf_addstr(cmd, uid);
    buf_addstr(cmd, " +FLAGS.SILENT (");
    buf_addstr(cmd, buf);
    buf_addstr(cmd, ")");

    int rc = imap_exec(adata, buf_string(cmd), IMAP_CMD_NO_FLAGS);
    buf_pool_release(&cmd);
    if (rc != IMAP_EXEC_SUCCESS)
    {
      mutt_debug(LL_DEBUG1, "fail to add new flags\n");
      return -1;
    }
  }

  /* We are good sync them */
  mutt_debug(LL_DEBUG1, "NEW TAGS: %s\n", buf);
  driver_tags_replace(&e->tags, buf);
  FREE(&imap_edata_get(e)->flags_remote);
  struct Buffer *flags_remote = buf_pool_get();
  driver_tags_get_with_hidden(&e->tags, flags_remote);
  imap_edata_get(e)->flags_remote = buf_strdup(flags_remote);
  buf_pool_release(&flags_remote);
  imap_msg_save_hcache(m, e);
  return 0;
}

/**
 * imap_path_probe - Is this an IMAP Mailbox? - Implements MxOps::path_probe() - @ingroup mx_path_probe
 */
enum MailboxType imap_path_probe(const char *path, const struct stat *st)
{
  if (mutt_istr_startswith(path, "imap://"))
    return MUTT_IMAP;

  if (mutt_istr_startswith(path, "imaps://"))
    return MUTT_IMAP;

  return MUTT_UNKNOWN;
}

/**
 * imap_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon() - @ingroup mx_path_canon
 */
int imap_path_canon(struct Buffer *path)
{
  struct Url *url = url_parse(buf_string(path));
  if (!url)
    return 0;

  char tmp[PATH_MAX] = { 0 };
  char tmp2[PATH_MAX] = { 0 };
  if (url->path)
  {
    struct ImapAccountData *adata = NULL;
    if (imap_adata_find(buf_string(path), &adata, NULL) == 0)
    {
      imap_fix_path_with_delim(adata->delim, url->path, tmp, sizeof(tmp));
    }
    else
    {
      imap_fix_path(url->path, tmp, sizeof(tmp));
    }
    url->path = tmp;
  }
  url_tostring(url, tmp2, sizeof(tmp2), U_NO_FLAGS);
  buf_strcpy(path, tmp2);
  url_free(&url);

  return 0;
}

/**
 * imap_path_is_empty - Is the mailbox empty - Implements MxOps::path_is_empty() - @ingroup mx_path_is_empty
 */
static int imap_path_is_empty(struct Buffer *path)
{
  int rc = imap_path_status(buf_string(path), false);
  if (rc < 0)
    return -1;
  if (rc == 0)
    return 1;
  return 0;
}

/**
 * MxImapOps - IMAP Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxImapOps = {
  // clang-format off
  .type            = MUTT_IMAP,
  .name             = "imap",
  .is_local         = false,
  .ac_owns_path     = imap_ac_owns_path,
  .ac_add           = imap_ac_add,
  .mbox_open        = imap_mbox_open,
  .mbox_open_append = imap_mbox_open_append,
  .mbox_check       = imap_mbox_check,
  .mbox_check_stats = imap_mbox_check_stats,
  .mbox_sync        = NULL, /* imap syncing is handled by imap_sync_mailbox */
  .mbox_close       = imap_mbox_close,
  .msg_open         = imap_msg_open,
  .msg_open_new     = imap_msg_open_new,
  .msg_commit       = imap_msg_commit,
  .msg_close        = imap_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = imap_msg_save_hcache,
  .tags_edit        = imap_tags_edit,
  .tags_commit      = imap_tags_commit,
  .path_probe       = imap_path_probe,
  .path_canon       = imap_path_canon,
  .path_is_empty    = imap_path_is_empty,
  // clang-format on
};
