/**
 * @file
 * Send/receive commands to/from an IMAP server
 *
 * @authors
 * Copyright (C) 1996-1998,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2011 Brendan Cully <brendan@kublai.com>
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
 * @page imap_command Send/receive commands to/from an IMAP server
 *
 * Send/receive commands to/from an IMAP server
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "buffy.h"
#include "context.h"
#include "globals.h"
#include "header.h"
#include "imap/imap.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_account.h"
#include "mutt_menu.h"
#include "mutt_socket.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "url.h"

#define IMAP_CMD_BUFSIZE 512

/**
 * Capabilities - Server capabilities strings that we understand
 *
 * @note This must be kept in the same order as ImapCaps.
 *
 * @note Gmail documents one string but use another, so we support both.
 */
static const char *const Capabilities[] = {
  "IMAP4",     "IMAP4rev1",     "STATUS",      "ACL",
  "NAMESPACE", "AUTH=CRAM-MD5", "AUTH=GSSAPI", "AUTH=ANONYMOUS",
  "STARTTLS",  "LOGINDISABLED", "IDLE",        "SASL-IR",
  "ENABLE",    "X-GM-EXT-1",    "X-GM-EXT1",   NULL,
};

/**
 * cmd_queue_full - Is the IMAP command queue full?
 * @param idata Server data
 * @retval true Queue is full
 */
static bool cmd_queue_full(struct ImapData *idata)
{
  if ((idata->nextcmd + 1) % idata->cmdslots == idata->lastcmd)
    return true;

  return false;
}

/**
 * cmd_new - Create and queue a new command control block
 * @param idata IMAP data
 * @retval NULL if the pipeline is full
 * @retval ptr New command
 */
static struct ImapCommand *cmd_new(struct ImapData *idata)
{
  struct ImapCommand *cmd = NULL;

  if (cmd_queue_full(idata))
  {
    mutt_debug(3, "IMAP command queue full\n");
    return NULL;
  }

  cmd = idata->cmds + idata->nextcmd;
  idata->nextcmd = (idata->nextcmd + 1) % idata->cmdslots;

  snprintf(cmd->seq, sizeof(cmd->seq), "a%04u", idata->seqno++);
  if (idata->seqno > 9999)
    idata->seqno = 0;

  cmd->state = IMAP_CMD_NEW;

  return cmd;
}

/**
 * cmd_queue - Add a IMAP command to the queue
 * @param idata  Server data
 * @param cmdstr Command string
 * @param flags  Server flags, e.g. #IMAP_CMD_POLL
 * @retval  0 Success
 * @retval <0 Failure, e.g. #IMAP_CMD_BAD
 *
 * If the queue is full, attempts to drain it.
 */
static int cmd_queue(struct ImapData *idata, const char *cmdstr, int flags)
{
  if (cmd_queue_full(idata))
  {
    mutt_debug(3, "Draining IMAP command pipeline\n");

    const int rc = imap_exec(idata, NULL, IMAP_CMD_FAIL_OK | (flags & IMAP_CMD_POLL));

    if (rc < 0 && rc != -2)
      return rc;
  }

  struct ImapCommand *cmd = cmd_new(idata);
  if (!cmd)
    return IMAP_CMD_BAD;

  if (mutt_buffer_printf(idata->cmdbuf, "%s %s\r\n", cmd->seq, cmdstr) < 0)
    return IMAP_CMD_BAD;

  return 0;
}

/**
 * cmd_handle_fatal - When ImapData is in fatal state, do what we can
 * @param idata Server data
 */
static void cmd_handle_fatal(struct ImapData *idata)
{
  idata->status = IMAP_FATAL;

  if ((idata->state >= IMAP_SELECTED) && (idata->reopen & IMAP_REOPEN_ALLOW))
  {
    mx_fastclose_mailbox(idata->ctx);
    mutt_socket_close(idata->conn);
    mutt_error(_("Mailbox %s@%s closed"), idata->conn->account.login,
               idata->conn->account.host);
    idata->state = IMAP_DISCONNECTED;
  }

  imap_close_connection(idata);
  if (!idata->recovering)
  {
    idata->recovering = true;
    if (imap_conn_find(&idata->conn->account, 0))
      mutt_clear_error();
    idata->recovering = false;
  }
}

/**
 * cmd_start - Start a new IMAP command
 * @param idata  Server data
 * @param cmdstr Command string
 * @param flags  Command flags, e.g. #IMAP_CMD_QUEUE
 * @retval  0 Success
 * @retval <0 Failure, e.g. #IMAP_CMD_BAD
 */
static int cmd_start(struct ImapData *idata, const char *cmdstr, int flags)
{
  int rc;

  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal(idata);
    return -1;
  }

  if (cmdstr && ((rc = cmd_queue(idata, cmdstr, flags)) < 0))
    return rc;

  if (flags & IMAP_CMD_QUEUE)
    return 0;

  if (idata->cmdbuf->dptr == idata->cmdbuf->data)
    return IMAP_CMD_BAD;

  rc = mutt_socket_send_d(idata->conn, idata->cmdbuf->data,
                          (flags & IMAP_CMD_PASS) ? IMAP_LOG_PASS : IMAP_LOG_CMD);
  idata->cmdbuf->dptr = idata->cmdbuf->data;

  /* unidle when command queue is flushed */
  if (idata->state == IMAP_IDLE)
    idata->state = IMAP_SELECTED;

  return (rc < 0) ? IMAP_CMD_BAD : 0;
}

/**
 * cmd_status - parse response line for tagged OK/NO/BAD
 * @param s Status string from server
 * @retval  0 Success
 * @retval <0 Failure, e.g. #IMAP_CMD_BAD
 */
static int cmd_status(const char *s)
{
  s = imap_next_word((char *) s);

  if (mutt_str_strncasecmp("OK", s, 2) == 0)
    return IMAP_CMD_OK;
  if (mutt_str_strncasecmp("NO", s, 2) == 0)
    return IMAP_CMD_NO;

  return IMAP_CMD_BAD;
}

/**
 * cmd_parse_expunge - Parse expunge command
 * @param idata Server data
 * @param s     String containing MSN of message to expunge
 *
 * cmd_parse_expunge: mark headers with new sequence ID and mark idata to be
 * reopened at our earliest convenience
 */
static void cmd_parse_expunge(struct ImapData *idata, const char *s)
{
  unsigned int exp_msn;
  struct Header *h = NULL;

  mutt_debug(2, "Handling EXPUNGE\n");

  if (mutt_str_atoui(s, &exp_msn) < 0 || exp_msn < 1 || exp_msn > idata->max_msn)
    return;

  h = idata->msn_index[exp_msn - 1];
  if (h)
  {
    /* imap_expunge_mailbox() will rewrite h->index.
     * It needs to resort using SORT_ORDER anyway, so setting to INT_MAX
     * makes the code simpler and possibly more efficient. */
    h->index = INT_MAX;
    HEADER_DATA(h)->msn = 0;
  }

  /* decrement seqno of those above. */
  for (unsigned int cur = exp_msn; cur < idata->max_msn; cur++)
  {
    h = idata->msn_index[cur];
    if (h)
      HEADER_DATA(h)->msn--;
    idata->msn_index[cur - 1] = h;
  }

  idata->msn_index[idata->max_msn - 1] = NULL;
  idata->max_msn--;

  idata->reopen |= IMAP_EXPUNGE_PENDING;
}

/**
 * cmd_parse_fetch - Load fetch response into ImapData
 * @param idata Server data
 * @param s     String containing MSN of message to fetch
 *
 * Currently only handles unanticipated FETCH responses, and only FLAGS data.
 * We get these if another client has changed flags for a mailbox we've
 * selected.  Of course, a lot of code here duplicates code in message.c.
 */
static void cmd_parse_fetch(struct ImapData *idata, char *s)
{
  unsigned int msn, uid;
  struct Header *h = NULL;
  int server_changes = 0;

  mutt_debug(3, "Handling FETCH\n");

  if (mutt_str_atoui(s, &msn) < 0 || msn < 1 || msn > idata->max_msn)
  {
    mutt_debug(3, "#1 FETCH response ignored for this message\n");
    return;
  }

  h = idata->msn_index[msn - 1];
  if (!h || !h->active)
  {
    mutt_debug(3, "#2 FETCH response ignored for this message\n");
    return;
  }

  mutt_debug(2, "Message UID %u updated\n", HEADER_DATA(h)->uid);
  /* skip FETCH */
  s = imap_next_word(s);
  s = imap_next_word(s);

  if (*s != '(')
  {
    mutt_debug(1, "Malformed FETCH response\n");
    return;
  }
  s++;

  while (*s)
  {
    SKIPWS(s);

    if (mutt_str_strncasecmp("FLAGS", s, 5) == 0)
    {
      imap_set_flags(idata, h, s, &server_changes);
      if (server_changes)
      {
        /* If server flags could conflict with neomutt's flags, reopen the mailbox. */
        if (h->changed)
          idata->reopen |= IMAP_EXPUNGE_PENDING;
        else
          idata->check_status = IMAP_FLAGS_PENDING;
      }
      return;
    }
    else if (mutt_str_strncasecmp("UID", s, 3) == 0)
    {
      s += 3;
      SKIPWS(s);
      if (mutt_str_atoui(s, &uid) < 0)
      {
        mutt_debug(2, "Illegal UID.  Skipping update.\n");
        return;
      }
      if (uid != HEADER_DATA(h)->uid)
      {
        mutt_debug(2, "FETCH UID vs MSN mismatch.  Skipping update.\n");
        return;
      }
      s = imap_next_word(s);
    }
    else if (*s == ')')
      s++; /* end of request */
    else if (*s)
    {
      mutt_debug(2, "Only handle FLAGS updates\n");
      return;
    }
  }
}

/**
 * cmd_parse_capability - set capability bits according to CAPABILITY response
 * @param idata Server data
 * @param s     Command string with capabilities
 */
static void cmd_parse_capability(struct ImapData *idata, char *s)
{
  mutt_debug(3, "Handling CAPABILITY\n");

  s = imap_next_word(s);
  char *bracket = strchr(s, ']');
  if (bracket)
    *bracket = '\0';
  FREE(&idata->capstr);
  idata->capstr = mutt_str_strdup(s);

  memset(idata->capabilities, 0, sizeof(idata->capabilities));

  while (*s)
  {
    for (int i = 0; i < CAPMAX; i++)
    {
      if (mutt_str_word_casecmp(Capabilities[i], s) == 0)
      {
        mutt_bit_set(idata->capabilities, i);
        mutt_debug(4, " Found capability \"%s\": %d\n", Capabilities[i], i);
        break;
      }
    }
    s = imap_next_word(s);
  }
}

/**
 * cmd_parse_list - Parse a server LIST command (list mailboxes)
 * @param idata Server data
 * @param s     Command string with folder list
 */
static void cmd_parse_list(struct ImapData *idata, char *s)
{
  struct ImapList *list = NULL;
  struct ImapList lb;
  char delimbuf[5]; /* worst case: "\\"\0 */
  unsigned int litlen;

  if (idata->cmddata && idata->cmdtype == IMAP_CT_LIST)
    list = (struct ImapList *) idata->cmddata;
  else
    list = &lb;

  memset(list, 0, sizeof(struct ImapList));

  /* flags */
  s = imap_next_word(s);
  if (*s != '(')
  {
    mutt_debug(1, "Bad LIST response\n");
    return;
  }
  s++;
  while (*s)
  {
    if (mutt_str_strncasecmp(s, "\\NoSelect", 9) == 0)
      list->noselect = true;
    else if (mutt_str_strncasecmp(s, "\\NoInferiors", 12) == 0)
      list->noinferiors = true;
    /* See draft-gahrns-imap-child-mailbox-?? */
    else if (mutt_str_strncasecmp(s, "\\HasNoChildren", 14) == 0)
      list->noinferiors = true;

    s = imap_next_word(s);
    if (*(s - 2) == ')')
      break;
  }

  /* Delimiter */
  if (mutt_str_strncasecmp(s, "NIL", 3) != 0)
  {
    delimbuf[0] = '\0';
    mutt_str_strcat(delimbuf, 5, s);
    imap_unquote_string(delimbuf);
    list->delim = delimbuf[0];
  }

  /* Name */
  s = imap_next_word(s);
  /* Notes often responds with literals here. We need a real tokenizer. */
  if (imap_get_literal_count(s, &litlen) == 0)
  {
    if (imap_cmd_step(idata) != IMAP_CMD_CONTINUE)
    {
      idata->status = IMAP_FATAL;
      return;
    }
    list->name = idata->buf;
  }
  else
  {
    imap_unmunge_mbox_name(idata, s);
    list->name = s;
  }

  if (list->name[0] == '\0')
  {
    idata->delim = list->delim;
    mutt_debug(3, "Root delimiter: %c\n", idata->delim);
  }
}

/**
 * cmd_parse_lsub - Parse a server LSUB (list subscribed mailboxes)
 * @param idata Server data
 * @param s     Command string with folder list
 */
static void cmd_parse_lsub(struct ImapData *idata, char *s)
{
  char buf[STRING];
  char errstr[STRING];
  struct Buffer err, token;
  struct Url url;
  struct ImapList list;

  if (idata->cmddata && idata->cmdtype == IMAP_CT_LIST)
  {
    /* caller will handle response itself */
    cmd_parse_list(idata, s);
    return;
  }

  if (!ImapCheckSubscribed)
    return;

  idata->cmdtype = IMAP_CT_LIST;
  idata->cmddata = &list;
  cmd_parse_list(idata, s);
  idata->cmddata = NULL;
  /* noselect is for a gmail quirk (#3445) */
  if (!list.name || list.noselect)
    return;

  mutt_debug(3, "Subscribing to %s\n", list.name);

  mutt_str_strfcpy(buf, "mailboxes \"", sizeof(buf));
  mutt_account_tourl(&idata->conn->account, &url);
  /* escape \ and " */
  imap_quote_string(errstr, sizeof(errstr), list.name, true);
  url.path = errstr + 1;
  url.path[strlen(url.path) - 1] = '\0';
  if (mutt_str_strcmp(url.user, ImapUser) == 0)
    url.user = NULL;
  url_tostring(&url, buf + 11, sizeof(buf) - 11, 0);
  mutt_str_strcat(buf, sizeof(buf), "\"");
  mutt_buffer_init(&token);
  mutt_buffer_init(&err);
  err.data = errstr;
  err.dsize = sizeof(errstr);
  if (mutt_parse_rc_line(buf, &token, &err))
    mutt_debug(1, "Error adding subscribed mailbox: %s\n", errstr);
  FREE(&token.data);
}

/**
 * cmd_parse_myrights - Set rights bits according to MYRIGHTS response
 * @param idata Server data
 * @param s     Command string with rights info
 */
static void cmd_parse_myrights(struct ImapData *idata, const char *s)
{
  mutt_debug(2, "Handling MYRIGHTS\n");

  s = imap_next_word((char *) s);
  s = imap_next_word((char *) s);

  /* zero out current rights set */
  memset(idata->ctx->rights, 0, sizeof(idata->ctx->rights));

  while (*s && !isspace((unsigned char) *s))
  {
    switch (*s)
    {
      case 'a':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_ADMIN);
        break;
      case 'e':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_EXPUNGE);
        break;
      case 'i':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_INSERT);
        break;
      case 'k':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_CREATE);
        break;
      case 'l':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_LOOKUP);
        break;
      case 'p':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_POST);
        break;
      case 'r':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_READ);
        break;
      case 's':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_SEEN);
        break;
      case 't':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELETE);
        break;
      case 'w':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_WRITE);
        break;
      case 'x':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELMX);
        break;

      /* obsolete rights */
      case 'c':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_CREATE);
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELMX);
        break;
      case 'd':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELETE);
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_EXPUNGE);
        break;
      default:
        mutt_debug(1, "Unknown right: %c\n", *s);
    }
    s++;
  }
}

/**
 * cmd_parse_search - store SEARCH response for later use
 * @param idata Server data
 * @param s     Command string with search results
 */
static void cmd_parse_search(struct ImapData *idata, const char *s)
{
  unsigned int uid;
  struct Header *h = NULL;

  mutt_debug(2, "Handling SEARCH\n");

  while ((s = imap_next_word((char *) s)) && *s != '\0')
  {
    if (mutt_str_atoui(s, &uid) < 0)
      continue;
    h = (struct Header *) mutt_hash_int_find(idata->uid_hash, uid);
    if (h)
      h->matched = true;
  }
}

/**
 * cmd_parse_status - Parse status from server
 * @param idata Server data
 * @param s     Command string with status info
 *
 * first cut: just do buffy update. Later we may wish to cache all mailbox
 * information, even that not desired by buffy
 */
static void cmd_parse_status(struct ImapData *idata, char *s)
{
  char *value = NULL;
  struct Buffy *inc = NULL;
  struct ImapMbox mx;
  struct ImapStatus *status = NULL;
  unsigned int olduv, oldun;
  unsigned int litlen;
  short new = 0;
  short new_msg_count = 0;

  char *mailbox = imap_next_word(s);

  /* We need a real tokenizer. */
  if (imap_get_literal_count(mailbox, &litlen) == 0)
  {
    if (imap_cmd_step(idata) != IMAP_CMD_CONTINUE)
    {
      idata->status = IMAP_FATAL;
      return;
    }
    mailbox = idata->buf;
    s = mailbox + litlen;
    *s = '\0';
    s++;
    SKIPWS(s);
  }
  else
  {
    s = imap_next_word(mailbox);
    *(s - 1) = '\0';
    imap_unmunge_mbox_name(idata, mailbox);
  }

  status = imap_mboxcache_get(idata, mailbox, 1);
  olduv = status->uidvalidity;
  oldun = status->uidnext;

  if (*s++ != '(')
  {
    mutt_debug(1, "Error parsing STATUS\n");
    return;
  }
  while (*s && *s != ')')
  {
    value = imap_next_word(s);

    errno = 0;
    const unsigned long ulcount = strtoul(value, &value, 10);
    if (((errno == ERANGE) && (ulcount == ULONG_MAX)) || ((unsigned int) ulcount != ulcount))
    {
      mutt_debug(1, "Error parsing STATUS number\n");
      return;
    }
    const unsigned int count = (unsigned int) ulcount;

    if (mutt_str_strncmp("MESSAGES", s, 8) == 0)
    {
      status->messages = count;
      new_msg_count = 1;
    }
    else if (mutt_str_strncmp("RECENT", s, 6) == 0)
      status->recent = count;
    else if (mutt_str_strncmp("UIDNEXT", s, 7) == 0)
      status->uidnext = count;
    else if (mutt_str_strncmp("UIDVALIDITY", s, 11) == 0)
      status->uidvalidity = count;
    else if (mutt_str_strncmp("UNSEEN", s, 6) == 0)
      status->unseen = count;

    s = value;
    if (*s && *s != ')')
      s = imap_next_word(s);
  }
  mutt_debug(3, "%s (UIDVALIDITY: %u, UIDNEXT: %u) %d messages, %d recent, %d unseen\n",
             status->name, status->uidvalidity, status->uidnext,
             status->messages, status->recent, status->unseen);

  /* caller is prepared to handle the result herself */
  if (idata->cmddata && idata->cmdtype == IMAP_CT_STATUS)
  {
    memcpy(idata->cmddata, status, sizeof(struct ImapStatus));
    return;
  }

  mutt_debug(3, "Running default STATUS handler\n");

  /* should perhaps move this code back to imap_buffy_check */
  for (inc = Incoming; inc; inc = inc->next)
  {
    if (inc->magic != MUTT_IMAP)
      continue;

    if (imap_parse_path(inc->path, &mx) < 0)
    {
      mutt_debug(1, "Error parsing mailbox %s, skipping\n", inc->path);
      continue;
    }

    if (imap_account_match(&idata->conn->account, &mx.account))
    {
      if (mx.mbox)
      {
        value = mutt_str_strdup(mx.mbox);
        imap_fix_path(idata, mx.mbox, value, mutt_str_strlen(value) + 1);
        FREE(&mx.mbox);
      }
      else
        value = mutt_str_strdup("INBOX");

      if (value && (imap_mxcmp(mailbox, value) == 0))
      {
        mutt_debug(3, "Found %s in buffy list (OV: %u ON: %u U: %d)\n", mailbox,
                   olduv, oldun, status->unseen);

        if (MailCheckRecent)
        {
          if (olduv && olduv == status->uidvalidity)
          {
            if (oldun < status->uidnext)
              new = (status->unseen > 0);
          }
          else if (!olduv && !oldun)
          {
            /* first check per session, use recent. might need a flag for this. */
            new = (status->recent > 0);
          }
          else
            new = (status->unseen > 0);
        }
        else
          new = (status->unseen > 0);

#ifdef USE_SIDEBAR
        if ((inc->new != new) || (inc->msg_count != status->messages) ||
            (inc->msg_unread != status->unseen))
        {
          mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
        }
#endif
        inc->new = new;
        if (new_msg_count)
          inc->msg_count = status->messages;
        inc->msg_unread = status->unseen;

        if (inc->new)
        {
          /* force back to keep detecting new mail until the mailbox is
             opened */
          status->uidnext = oldun;
        }

        FREE(&value);
        return;
      }

      FREE(&value);
    }

    FREE(&mx.mbox);
  }
}

/**
 * cmd_parse_enabled - Record what the server has enabled
 * @param idata Server data
 * @param s     Command string containing acceptable encodings
 */
static void cmd_parse_enabled(struct ImapData *idata, const char *s)
{
  mutt_debug(2, "Handling ENABLED\n");

  while ((s = imap_next_word((char *) s)) && *s != '\0')
  {
    if ((mutt_str_strncasecmp(s, "UTF8=ACCEPT", 11) == 0) ||
        (mutt_str_strncasecmp(s, "UTF8=ONLY", 9) == 0))
    {
      idata->unicode = 1;
    }
  }
}

/**
 * cmd_handle_untagged - fallback parser for otherwise unhandled messages
 * @param idata Server data
 * @retval  0 Success
 * @retval -1 Failure
 */
static int cmd_handle_untagged(struct ImapData *idata)
{
  unsigned int count = 0;
  char *s = imap_next_word(idata->buf);
  char *pn = imap_next_word(s);

  if ((idata->state >= IMAP_SELECTED) && isdigit((unsigned char) *s))
  {
    pn = s;
    s = imap_next_word(s);

    /* EXISTS and EXPUNGE are always related to the SELECTED mailbox for the
     * connection, so update that one.
     */
    if (mutt_str_strncasecmp("EXISTS", s, 6) == 0)
    {
      mutt_debug(2, "Handling EXISTS\n");

      /* new mail arrived */
      if (mutt_str_atoui(pn, &count) < 0)
      {
        mutt_debug(1, "Malformed EXISTS: '%s'\n", pn);
      }

      if (!(idata->reopen & IMAP_EXPUNGE_PENDING) && count < idata->max_msn)
      {
        /* Notes 6.0.3 has a tendency to report fewer messages exist than
         * it should. */
        mutt_debug(1, "Message count is out of sync\n");
        return 0;
      }
      /* at least the InterChange server sends EXISTS messages freely,
       * even when there is no new mail */
      else if (count == idata->max_msn)
        mutt_debug(3, "superfluous EXISTS message.\n");
      else
      {
        if (!(idata->reopen & IMAP_EXPUNGE_PENDING))
        {
          mutt_debug(2, "New mail in %s - %d messages total.\n", idata->mailbox, count);
          idata->reopen |= IMAP_NEWMAIL_PENDING;
        }
        idata->new_mail_count = count;
      }
    }
    /* pn vs. s: need initial seqno */
    else if (mutt_str_strncasecmp("EXPUNGE", s, 7) == 0)
      cmd_parse_expunge(idata, pn);
    else if (mutt_str_strncasecmp("FETCH", s, 5) == 0)
      cmd_parse_fetch(idata, pn);
  }
  else if (mutt_str_strncasecmp("CAPABILITY", s, 10) == 0)
    cmd_parse_capability(idata, s);
  else if (mutt_str_strncasecmp("OK [CAPABILITY", s, 14) == 0)
    cmd_parse_capability(idata, pn);
  else if (mutt_str_strncasecmp("OK [CAPABILITY", pn, 14) == 0)
    cmd_parse_capability(idata, imap_next_word(pn));
  else if (mutt_str_strncasecmp("LIST", s, 4) == 0)
    cmd_parse_list(idata, s);
  else if (mutt_str_strncasecmp("LSUB", s, 4) == 0)
    cmd_parse_lsub(idata, s);
  else if (mutt_str_strncasecmp("MYRIGHTS", s, 8) == 0)
    cmd_parse_myrights(idata, s);
  else if (mutt_str_strncasecmp("SEARCH", s, 6) == 0)
    cmd_parse_search(idata, s);
  else if (mutt_str_strncasecmp("STATUS", s, 6) == 0)
    cmd_parse_status(idata, s);
  else if (mutt_str_strncasecmp("ENABLED", s, 7) == 0)
    cmd_parse_enabled(idata, s);
  else if (mutt_str_strncasecmp("BYE", s, 3) == 0)
  {
    mutt_debug(2, "Handling BYE\n");

    /* check if we're logging out */
    if (idata->status == IMAP_BYE)
      return 0;

    /* server shut down our connection */
    s += 3;
    SKIPWS(s);
    mutt_error("%s", s);
    cmd_handle_fatal(idata);

    return -1;
  }
  else if (ImapServernoise && (mutt_str_strncasecmp("NO", s, 2) == 0))
  {
    mutt_debug(2, "Handling untagged NO\n");

    /* Display the warning message from the server */
    mutt_error("%s", s + 3);
  }

  return 0;
}

/**
 * imap_cmd_start - Given an IMAP command, send it to the server
 * @param idata  Server data
 * @param cmdstr Command string to send
 * @retval  0 Success
 * @retval <0 Failure, e.g. #IMAP_CMD_BAD
 *
 * If cmdstr is NULL, sends queued commands.
 */
int imap_cmd_start(struct ImapData *idata, const char *cmdstr)
{
  return cmd_start(idata, cmdstr, 0);
}

/**
 * imap_cmd_step - Reads server responses from an IMAP command
 * @param idata Server data
 * @retval  0 Success
 * @retval <0 Failure, e.g. #IMAP_CMD_BAD
 *
 * detects tagged completion response, handles untagged messages, can read
 * arbitrarily large strings (using malloc, so don't make it _too_ large!).
 */
int imap_cmd_step(struct ImapData *idata)
{
  size_t len = 0;
  int c;
  int rc;
  int stillrunning = 0;
  struct ImapCommand *cmd = NULL;

  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal(idata);
    return IMAP_CMD_BAD;
  }

  /* read into buffer, expanding buffer as necessary until we have a full
   * line */
  do
  {
    if (len == idata->blen)
    {
      mutt_mem_realloc(&idata->buf, idata->blen + IMAP_CMD_BUFSIZE);
      idata->blen = idata->blen + IMAP_CMD_BUFSIZE;
      mutt_debug(3, "grew buffer to %u bytes\n", idata->blen);
    }

    /* back up over '\0' */
    if (len)
      len--;
    c = mutt_socket_readln(idata->buf + len, idata->blen - len, idata->conn);
    if (c <= 0)
    {
      mutt_debug(1, "Error reading server response.\n");
      cmd_handle_fatal(idata);
      return IMAP_CMD_BAD;
    }

    len += c;
  }
  /* if we've read all the way to the end of the buffer, we haven't read a
   * full line (mutt_socket_readln strips the \r, so we always have at least
   * one character free when we've read a full line) */
  while (len == idata->blen);

  /* don't let one large string make cmd->buf hog memory forever */
  if ((idata->blen > IMAP_CMD_BUFSIZE) && (len <= IMAP_CMD_BUFSIZE))
  {
    mutt_mem_realloc(&idata->buf, IMAP_CMD_BUFSIZE);
    idata->blen = IMAP_CMD_BUFSIZE;
    mutt_debug(3, "shrank buffer to %u bytes\n", idata->blen);
  }

  idata->lastread = time(NULL);

  /* handle untagged messages. The caller still gets its shot afterwards. */
  if (((mutt_str_strncmp(idata->buf, "* ", 2) == 0) ||
       (mutt_str_strncmp(imap_next_word(idata->buf), "OK [", 4) == 0)) &&
      cmd_handle_untagged(idata))
  {
    return IMAP_CMD_BAD;
  }

  /* server demands a continuation response from us */
  if (idata->buf[0] == '+')
    return IMAP_CMD_RESPOND;

  /* Look for tagged command completions.
   *
   * Some response handlers can end up recursively calling
   * imap_cmd_step() and end up handling all tagged command
   * completions.
   * (e.g. FETCH->set_flag->set_header_color->~h pattern match.)
   *
   * Other callers don't even create an idata->cmds entry.
   *
   * For both these cases, we default to returning OK */
  rc = IMAP_CMD_OK;
  c = idata->lastcmd;
  do
  {
    cmd = &idata->cmds[c];
    if (cmd->state == IMAP_CMD_NEW)
    {
      if (mutt_str_strncmp(idata->buf, cmd->seq, SEQLEN) == 0)
      {
        if (!stillrunning)
        {
          /* first command in queue has finished - move queue pointer up */
          idata->lastcmd = (idata->lastcmd + 1) % idata->cmdslots;
        }
        cmd->state = cmd_status(idata->buf);
        /* bogus - we don't know which command result to return here. Caller
         * should provide a tag. */
        rc = cmd->state;
      }
      else
        stillrunning++;
    }

    c = (c + 1) % idata->cmdslots;
  } while (c != idata->nextcmd);

  if (stillrunning)
    rc = IMAP_CMD_CONTINUE;
  else
  {
    mutt_debug(3, "IMAP queue drained\n");
    imap_cmd_finish(idata);
  }

  return rc;
}

/**
 * imap_code - Was the command successful
 * @param s IMAP command status
 * @retval 1 Command result was OK
 * @retval 0 If NO or BAD
 */
bool imap_code(const char *s)
{
  return (cmd_status(s) == IMAP_CMD_OK);
}

/**
 * imap_cmd_trailer - Extra information after tagged command response if any
 * @param idata Server data
 * @retval ptr Extra command information (pointer into idata->buf)
 * @retval ""  Error (static string)
 */
const char *imap_cmd_trailer(struct ImapData *idata)
{
  static const char *notrailer = "";
  const char *s = idata->buf;

  if (!s)
  {
    mutt_debug(2, "not a tagged response\n");
    return notrailer;
  }

  s = imap_next_word((char *) s);
  if (!s || ((mutt_str_strncasecmp(s, "OK", 2) != 0) &&
             (mutt_str_strncasecmp(s, "NO", 2) != 0) &&
             (mutt_str_strncasecmp(s, "BAD", 3) != 0)))
  {
    mutt_debug(2, "not a command completion: %s\n", idata->buf);
    return notrailer;
  }

  s = imap_next_word((char *) s);
  if (!s)
    return notrailer;

  return s;
}

/**
 * imap_exec - Execute a command and wait for the response from the server
 * @param idata  IMAP data
 * @param cmdstr Command to execute
 * @param flags  Flags (see below)
 * @retval  0 Success
 * @retval -1 Failure
 * @retval -2 OK Failure
 *
 * Also, handle untagged responses.
 *
 * Flags:
 * * IMAP_CMD_FAIL_OK: the calling procedure can handle failure.
 *       This is used for checking for a mailbox on append and login
 * * IMAP_CMD_PASS: command contains a password. Suppress logging.
 * * IMAP_CMD_QUEUE: only queue command, do not execute.
 * * IMAP_CMD_POLL: poll the socket for a response before running imap_cmd_step.
 */
int imap_exec(struct ImapData *idata, const char *cmdstr, int flags)
{
  int rc;

  rc = cmd_start(idata, cmdstr, flags);
  if (rc < 0)
  {
    cmd_handle_fatal(idata);
    return -1;
  }

  if (flags & IMAP_CMD_QUEUE)
    return 0;

  if ((flags & IMAP_CMD_POLL) && (ImapPollTimeout > 0) &&
      (mutt_socket_poll(idata->conn, ImapPollTimeout)) == 0)
  {
    mutt_error(_("Connection to %s timed out"), idata->conn->account.host);
    cmd_handle_fatal(idata);
    return -1;
  }

  /* Allow interruptions, particularly useful if there are network problems. */
  mutt_sig_allow_interrupt(1);
  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);
  mutt_sig_allow_interrupt(0);

  if (rc == IMAP_CMD_NO && (flags & IMAP_CMD_FAIL_OK))
    return -2;

  if (rc != IMAP_CMD_OK)
  {
    if ((flags & IMAP_CMD_FAIL_OK) && idata->status != IMAP_FATAL)
      return -2;

    mutt_debug(1, "command failed: %s\n", idata->buf);
    return -1;
  }

  return 0;
}

/**
 * imap_cmd_finish - Attempt to perform cleanup
 * @param idata Server data
 *
 * Attempts to perform cleanup (eg fetch new mail if detected, do expunge).
 * Called automatically by imap_cmd_step(), but may be called at any time.
 * Called by imap_check_mailbox() just before the index is refreshed, for
 * instance.
 */
void imap_cmd_finish(struct ImapData *idata)
{
  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal(idata);
    return;
  }

  if (!(idata->state >= IMAP_SELECTED) || idata->ctx->closing)
    return;

  if (idata->reopen & IMAP_REOPEN_ALLOW)
  {
    unsigned int count = idata->new_mail_count;

    if (!(idata->reopen & IMAP_EXPUNGE_PENDING) &&
        (idata->reopen & IMAP_NEWMAIL_PENDING) && count > idata->max_msn)
    {
      /* read new mail messages */
      mutt_debug(2, "Fetching new mail\n");
      /* check_status: curs_main uses imap_check_mailbox to detect
       *   whether the index needs updating */
      idata->check_status = IMAP_NEWMAIL_PENDING;
      imap_read_headers(idata, idata->max_msn + 1, count);
    }
    else if (idata->reopen & IMAP_EXPUNGE_PENDING)
    {
      mutt_debug(2, "Expunging mailbox\n");
      imap_expunge_mailbox(idata);
      /* Detect whether we've gotten unexpected EXPUNGE messages */
      if ((idata->reopen & IMAP_EXPUNGE_PENDING) && !(idata->reopen & IMAP_EXPUNGE_EXPECTED))
        idata->check_status = IMAP_EXPUNGE_PENDING;
      idata->reopen &=
          ~(IMAP_EXPUNGE_PENDING | IMAP_NEWMAIL_PENDING | IMAP_EXPUNGE_EXPECTED);
    }
  }

  idata->status = false;
}

/**
 * imap_cmd_idle - Enter the IDLE state
 * @param idata Server data
 * @retval  0 Success
 * @retval <0 Failure, e.g. #IMAP_CMD_BAD
 */
int imap_cmd_idle(struct ImapData *idata)
{
  int rc;

  if (cmd_start(idata, "IDLE", IMAP_CMD_POLL) < 0)
  {
    cmd_handle_fatal(idata);
    return -1;
  }

  if ((ImapPollTimeout > 0) && (mutt_socket_poll(idata->conn, ImapPollTimeout)) == 0)
  {
    mutt_error(_("Connection to %s timed out"), idata->conn->account.host);
    cmd_handle_fatal(idata);
    return -1;
  }

  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc == IMAP_CMD_RESPOND)
  {
    /* successfully entered IDLE state */
    idata->state = IMAP_IDLE;
    /* queue automatic exit when next command is issued */
    mutt_buffer_printf(idata->cmdbuf, "DONE\r\n");
    rc = IMAP_CMD_OK;
  }
  if (rc != IMAP_CMD_OK)
  {
    mutt_debug(1, "error starting IDLE\n");
    return -1;
  }

  return 0;
}
