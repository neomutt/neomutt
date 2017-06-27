/**
 * Copyright (C) 1996-1998,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2011 Brendan Cully <brendan@kublai.com>
 *
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

/* command.c: routines for sending commands to an IMAP server and parsing
 *  responses */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "imap_private.h"
#include "account.h"
#include "ascii.h"
#include "buffer.h"
#include "buffy.h"
#include "context.h"
#include "globals.h"
#include "hash.h"
#include "header.h"
#include "imap/imap.h"
#include "lib.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_menu.h"
#include "mutt_socket.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "url.h"

#define IMAP_CMD_BUFSIZE 512

static const char *const Capabilities[] = {
  "IMAP4",         "IMAP4rev1",   "STATUS",         "ACL",      "NAMESPACE",
  "AUTH=CRAM-MD5", "AUTH=GSSAPI", "AUTH=ANONYMOUS", "STARTTLS", "LOGINDISABLED",
  "IDLE",          "SASL-IR",     "X-GM-EXT1",      "ENABLE",   NULL,
};

/* Gmail document one string but use another.  Support both. */
struct Capability_Alias
{
  char *name;
  unsigned int value;
};

static struct Capability_Alias Capability_Aliases[] = {
  { "X-GM-EXT-1", X_GM_EXT1 }, { NULL, 0 },
};

static bool cmd_queue_full(struct ImapData *idata)
{
  if ((idata->nextcmd + 1) % idata->cmdslots == idata->lastcmd)
    return true;

  return false;
}

/* sets up a new command control block and adds it to the queue.
 * Returns NULL if the pipeline is full. */
static struct ImapCommand *cmd_new(struct ImapData *idata)
{
  struct ImapCommand *cmd = NULL;

  if (cmd_queue_full(idata))
  {
    mutt_debug(3, "cmd_new: IMAP command queue full\n");
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

/* queues command. If the queue is full, attempts to drain it. */
static int cmd_queue(struct ImapData *idata, const char *cmdstr)
{
  struct ImapCommand *cmd = NULL;
  int rc;

  if (cmd_queue_full(idata))
  {
    mutt_debug(3, "Draining IMAP command pipeline\n");

    rc = imap_exec(idata, NULL, IMAP_CMD_FAIL_OK);

    if (rc < 0 && rc != -2)
      return rc;
  }

  if (!(cmd = cmd_new(idata)))
    return IMAP_CMD_BAD;

  if (mutt_buffer_printf(idata->cmdbuf, "%s %s\r\n", cmd->seq, cmdstr) < 0)
    return IMAP_CMD_BAD;

  return 0;
}

/* cmd_handle_fatal: when ImapData is in fatal state, do what we can */
static void cmd_handle_fatal(struct ImapData *idata)
{
  idata->status = IMAP_FATAL;

  if ((idata->state >= IMAP_SELECTED) && (idata->reopen & IMAP_REOPEN_ALLOW))
  {
    mx_fastclose_mailbox(idata->ctx);
    mutt_error(_("Mailbox closed"));
    mutt_sleep(1);
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

static int cmd_start(struct ImapData *idata, const char *cmdstr, int flags)
{
  int rc;

  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal(idata);
    return -1;
  }

  if (cmdstr && ((rc = cmd_queue(idata, cmdstr)) < 0))
    return rc;

  if (flags & IMAP_CMD_QUEUE)
    return 0;

  if (idata->cmdbuf->dptr == idata->cmdbuf->data)
    return IMAP_CMD_BAD;

  rc = mutt_socket_write_d(idata->conn, idata->cmdbuf->data, -1,
                           flags & IMAP_CMD_PASS ? IMAP_LOG_PASS : IMAP_LOG_CMD);
  idata->cmdbuf->dptr = idata->cmdbuf->data;

  /* unidle when command queue is flushed */
  if (idata->state == IMAP_IDLE)
    idata->state = IMAP_SELECTED;

  return (rc < 0) ? IMAP_CMD_BAD : 0;
}

/* parse response line for tagged OK/NO/BAD */
static int cmd_status(const char *s)
{
  s = imap_next_word((char *) s);

  if (ascii_strncasecmp("OK", s, 2) == 0)
    return IMAP_CMD_OK;
  if (ascii_strncasecmp("NO", s, 2) == 0)
    return IMAP_CMD_NO;

  return IMAP_CMD_BAD;
}

/* cmd_parse_expunge: mark headers with new sequence ID and mark idata to
 *   be reopened at our earliest convenience */
static void cmd_parse_expunge(struct ImapData *idata, const char *s)
{
  unsigned int exp_msn;
  struct Header *h = NULL;

  mutt_debug(2, "Handling EXPUNGE\n");

  exp_msn = atoi(s);
  if (exp_msn < 1 || exp_msn > idata->max_msn)
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

/* cmd_parse_fetch: Load fetch response into ImapData. Currently only
 *   handles unanticipated FETCH responses, and only FLAGS data. We get
 *   these if another client has changed flags for a mailbox we've selected.
 *   Of course, a lot of code here duplicates code in message.c. */
static void cmd_parse_fetch(struct ImapData *idata, char *s)
{
  unsigned int msn;
  struct Header *h = NULL;

  mutt_debug(3, "Handling FETCH\n");

  msn = atoi(s);
  if (msn < 1 || msn > idata->max_msn)
  {
    mutt_debug(3, "FETCH response ignored for this message\n");
    return;
  }

  h = idata->msn_index[msn - 1];
  if (!h || !h->active)
  {
    mutt_debug(3, "FETCH response ignored for this message\n");
    return;
  }

  mutt_debug(2, "Message UID %d updated\n", HEADER_DATA(h)->uid);
  /* skip FETCH */
  s = imap_next_word(s);
  s = imap_next_word(s);

  if (*s != '(')
  {
    mutt_debug(1, "Malformed FETCH response\n");
    return;
  }
  s++;

  if (ascii_strncasecmp("FLAGS", s, 5) != 0)
  {
    mutt_debug(2, "Only handle FLAGS updates\n");
    return;
  }

  /* If server flags could conflict with mutt's flags, reopen the mailbox. */
  if (h->changed)
    idata->reopen |= IMAP_EXPUNGE_PENDING;
  else
  {
    imap_set_flags(idata, h, s);
    idata->check_status = IMAP_FLAGS_PENDING;
  }
}

/* cmd_parse_capabilities: set capability bits according to CAPABILITY
 *   response */
static void cmd_parse_capability(struct ImapData *idata, char *s)
{
  int x;
  bool found;
  char *bracket = NULL;

  mutt_debug(3, "Handling CAPABILITY\n");

  s = imap_next_word(s);
  if ((bracket = strchr(s, ']')))
    *bracket = '\0';
  FREE(&idata->capstr);
  idata->capstr = safe_strdup(s);

  memset(idata->capabilities, 0, sizeof(idata->capabilities));

  while (*s)
  {
    found = false;
    for (x = 0; x < CAPMAX; x++)
      if (imap_wordcasecmp(Capabilities[x], s) == 0)
      {
        mutt_bit_set(idata->capabilities, x);
        mutt_debug(4, " Found capability \"%s\": %d\n", Capabilities[x], x);
        found = true;
        break;
      }
    if (!found)
    {
      for (x = 0; Capability_Aliases[x].name != NULL; x++)
      {
        if (imap_wordcasecmp(Capability_Aliases[x].name, s) == 0)
        {
          mutt_bit_set(idata->capabilities, Capability_Aliases[x].value);
          mutt_debug(4, " Found capability \"%s\": %d\n",
                     Capability_Aliases[x].name, Capability_Aliases[x].value);
          found = true;
          break;
        }
      }
    }
    s = imap_next_word(s);
  }
}

static void cmd_parse_list(struct ImapData *idata, char *s)
{
  struct ImapList *list = NULL;
  struct ImapList lb;
  char delimbuf[5]; /* worst case: "\\"\0 */
  long litlen;

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
    if (ascii_strncasecmp(s, "\\NoSelect", 9) == 0)
      list->noselect = true;
    else if (ascii_strncasecmp(s, "\\NoInferiors", 12) == 0)
      list->noinferiors = true;
    /* See draft-gahrns-imap-child-mailbox-?? */
    else if (ascii_strncasecmp(s, "\\HasNoChildren", 14) == 0)
      list->noinferiors = true;

    s = imap_next_word(s);
    if (*(s - 2) == ')')
      break;
  }

  /* Delimiter */
  if (ascii_strncasecmp(s, "NIL", 3) != 0)
  {
    delimbuf[0] = '\0';
    safe_strcat(delimbuf, 5, s);
    imap_unquote_string(delimbuf);
    list->delim = delimbuf[0];
  }

  /* Name */
  s = imap_next_word(s);
  /* Notes often responds with literals here. We need a real tokenizer. */
  if (!imap_get_literal_count(s, &litlen))
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

static void cmd_parse_lsub(struct ImapData *idata, char *s)
{
  char buf[STRING];
  char errstr[STRING];
  struct Buffer err, token;
  struct CissUrl url;
  struct ImapList list;

  if (idata->cmddata && idata->cmdtype == IMAP_CT_LIST)
  {
    /* caller will handle response itself */
    cmd_parse_list(idata, s);
    return;
  }

  if (!option(OPTIMAPCHECKSUBSCRIBED))
    return;

  idata->cmdtype = IMAP_CT_LIST;
  idata->cmddata = &list;
  cmd_parse_list(idata, s);
  idata->cmddata = NULL;
  /* noselect is for a gmail quirk (#3445) */
  if (!list.name || list.noselect)
    return;

  mutt_debug(3, "Subscribing to %s\n", list.name);

  strfcpy(buf, "mailboxes \"", sizeof(buf));
  mutt_account_tourl(&idata->conn->account, &url);
  /* escape \ and " */
  imap_quote_string(errstr, sizeof(errstr), list.name);
  url.path = errstr + 1;
  url.path[strlen(url.path) - 1] = '\0';
  if (mutt_strcmp(url.user, ImapUser) == 0)
    url.user = NULL;
  url_ciss_tostring(&url, buf + 11, sizeof(buf) - 11, 0);
  safe_strcat(buf, sizeof(buf), "\"");
  mutt_buffer_init(&token);
  mutt_buffer_init(&err);
  err.data = errstr;
  err.dsize = sizeof(errstr);
  if (mutt_parse_rc_line(buf, &token, &err))
    mutt_debug(1, "Error adding subscribed mailbox: %s\n", errstr);
  FREE(&token.data);
}

/* cmd_parse_myrights: set rights bits according to MYRIGHTS response */
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
      case 'l':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_LOOKUP);
        break;
      case 'r':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_READ);
        break;
      case 's':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_SEEN);
        break;
      case 'w':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_WRITE);
        break;
      case 'i':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_INSERT);
        break;
      case 'p':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_POST);
        break;
      case 'a':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_ADMIN);
        break;
      case 'k':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_CREATE);
        break;
      case 'x':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELMX);
        break;
      case 't':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELETE);
        break;
      case 'e':
        mutt_bit_set(idata->ctx->rights, MUTT_ACL_EXPUNGE);
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

/* cmd_parse_search: store SEARCH response for later use */
static void cmd_parse_search(struct ImapData *idata, const char *s)
{
  unsigned int uid;
  struct Header *h = NULL;

  mutt_debug(2, "Handling SEARCH\n");

  while ((s = imap_next_word((char *) s)) && *s != '\0')
  {
    uid = (unsigned int) atoi(s);
    h = (struct Header *) int_hash_find(idata->uid_hash, uid);
    if (h)
      h->matched = true;
  }
}

/* first cut: just do buffy update. Later we may wish to cache all
 * mailbox information, even that not desired by buffy */
static void cmd_parse_status(struct ImapData *idata, char *s)
{
  char *mailbox = NULL;
  char *value = NULL;
  struct Buffy *inc = NULL;
  struct ImapMbox mx;
  int count;
  struct ImapStatus *status = NULL;
  unsigned int olduv, oldun;
  long litlen;
  short new = 0;
  short new_msg_count = 0;

  mailbox = imap_next_word(s);

  /* We need a real tokenizer. */
  if (!imap_get_literal_count(mailbox, &litlen))
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
    count = strtol(value, &value, 10);

    if (ascii_strncmp("MESSAGES", s, 8) == 0)
    {
      status->messages = count;
      new_msg_count = 1;
    }
    else if (ascii_strncmp("RECENT", s, 6) == 0)
      status->recent = count;
    else if (ascii_strncmp("UIDNEXT", s, 7) == 0)
      status->uidnext = count;
    else if (ascii_strncmp("UIDVALIDITY", s, 11) == 0)
      status->uidvalidity = count;
    else if (ascii_strncmp("UNSEEN", s, 6) == 0)
      status->unseen = count;

    s = value;
    if (*s && *s != ')')
      s = imap_next_word(s);
  }
  mutt_debug(
      3,
      "%s (UIDVALIDITY: %d, UIDNEXT: %d) %d messages, %d recent, %d unseen\n",
      status->name, status->uidvalidity, status->uidnext, status->messages,
      status->recent, status->unseen);

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
        value = safe_strdup(mx.mbox);
        imap_fix_path(idata, mx.mbox, value, mutt_strlen(value) + 1);
        FREE(&mx.mbox);
      }
      else
        value = safe_strdup("INBOX");

      if (value && (imap_mxcmp(mailbox, value) == 0))
      {
        mutt_debug(3, "Found %s in buffy list (OV: %d ON: %d U: %d)\n", mailbox,
                   olduv, oldun, status->unseen);

        if (option(OPTMAILCHECKRECENT))
        {
          if (olduv && olduv == status->uidvalidity)
          {
            if (oldun < status->uidnext)
              new = (status->unseen > 0);
          }
          else if (!olduv && !oldun)
            /* first check per session, use recent. might need a flag for this. */
            new = (status->recent > 0);
          else
            new = (status->unseen > 0);
        }
        else
          new = (status->unseen > 0);

#ifdef USE_SIDEBAR
        if ((inc->new != new) || (inc->msg_count != status->messages) ||
            (inc->msg_unread != status->unseen))
          mutt_set_current_menu_redraw(REDRAW_SIDEBAR);
#endif
        inc->new = new;
        if (new_msg_count)
          inc->msg_count = status->messages;
        inc->msg_unread = status->unseen;

        if (inc->new)
          /* force back to keep detecting new mail until the mailbox is
             opened */
          status->uidnext = oldun;

        FREE(&value);
        return;
      }

      FREE(&value);
    }

    FREE(&mx.mbox);
  }
}

/* cmd_parse_enabled: record what the server has enabled */
static void cmd_parse_enabled(struct ImapData *idata, const char *s)
{
  mutt_debug(2, "Handling ENABLED\n");

  while ((s = imap_next_word((char *) s)) && *s != '\0')
  {
    if ((ascii_strncasecmp(s, "UTF8=ACCEPT", 11) == 0) ||
        (ascii_strncasecmp(s, "UTF8=ONLY", 9) == 0))
      idata->unicode = 1;
  }
}

/* cmd_handle_untagged: fallback parser for otherwise unhandled messages. */
static int cmd_handle_untagged(struct ImapData *idata)
{
  char *s = NULL;
  char *pn = NULL;
  unsigned int count;

  s = imap_next_word(idata->buf);
  pn = imap_next_word(s);

  if ((idata->state >= IMAP_SELECTED) && isdigit((unsigned char) *s))
  {
    pn = s;
    s = imap_next_word(s);

    /* EXISTS and EXPUNGE are always related to the SELECTED mailbox for the
     * connection, so update that one.
     */
    if (ascii_strncasecmp("EXISTS", s, 6) == 0)
    {
      mutt_debug(2, "Handling EXISTS\n");

      /* new mail arrived */
      count = atoi(pn);

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
        mutt_debug(3, "cmd_handle_untagged: superfluous EXISTS message.\n");
      else
      {
        if (!(idata->reopen & IMAP_EXPUNGE_PENDING))
        {
          mutt_debug(
              2, "cmd_handle_untagged: New mail in %s - %d messages total.\n",
              idata->mailbox, count);
          idata->reopen |= IMAP_NEWMAIL_PENDING;
        }
        idata->newMailCount = count;
      }
    }
    /* pn vs. s: need initial seqno */
    else if (ascii_strncasecmp("EXPUNGE", s, 7) == 0)
      cmd_parse_expunge(idata, pn);
    else if (ascii_strncasecmp("FETCH", s, 5) == 0)
      cmd_parse_fetch(idata, pn);
  }
  else if (ascii_strncasecmp("CAPABILITY", s, 10) == 0)
    cmd_parse_capability(idata, s);
  else if (ascii_strncasecmp("OK [CAPABILITY", s, 14) == 0)
    cmd_parse_capability(idata, pn);
  else if (ascii_strncasecmp("OK [CAPABILITY", pn, 14) == 0)
    cmd_parse_capability(idata, imap_next_word(pn));
  else if (ascii_strncasecmp("LIST", s, 4) == 0)
    cmd_parse_list(idata, s);
  else if (ascii_strncasecmp("LSUB", s, 4) == 0)
    cmd_parse_lsub(idata, s);
  else if (ascii_strncasecmp("MYRIGHTS", s, 8) == 0)
    cmd_parse_myrights(idata, s);
  else if (ascii_strncasecmp("SEARCH", s, 6) == 0)
    cmd_parse_search(idata, s);
  else if (ascii_strncasecmp("STATUS", s, 6) == 0)
    cmd_parse_status(idata, s);
  else if (ascii_strncasecmp("ENABLED", s, 7) == 0)
    cmd_parse_enabled(idata, s);
  else if (ascii_strncasecmp("BYE", s, 3) == 0)
  {
    mutt_debug(2, "Handling BYE\n");

    /* check if we're logging out */
    if (idata->status == IMAP_BYE)
      return 0;

    /* server shut down our connection */
    s += 3;
    SKIPWS(s);
    mutt_error("%s", s);
    mutt_sleep(2);
    cmd_handle_fatal(idata);

    return -1;
  }
  else if (option(OPTIMAPSERVERNOISE) && (ascii_strncasecmp("NO", s, 2) == 0))
  {
    mutt_debug(2, "Handling untagged NO\n");

    /* Display the warning message from the server */
    mutt_error("%s", s + 3);
    mutt_sleep(2);
  }

  return 0;
}

/* imap_cmd_start: Given an IMAP command, send it to the server.
 *   If cmdstr is NULL, sends queued commands. */
int imap_cmd_start(struct ImapData *idata, const char *cmdstr)
{
  return cmd_start(idata, cmdstr, 0);
}

/* imap_cmd_step: Reads server responses from an IMAP command, detects
 *   tagged completion response, handles untagged messages, can read
 *   arbitrarily large strings (using malloc, so don't make it _too_
 *   large!). */
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
      safe_realloc(&idata->buf, idata->blen + IMAP_CMD_BUFSIZE);
      idata->blen = idata->blen + IMAP_CMD_BUFSIZE;
      mutt_debug(3, "imap_cmd_step: grew buffer to %u bytes\n", idata->blen);
    }

    /* back up over '\0' */
    if (len)
      len--;
    c = mutt_socket_readln(idata->buf + len, idata->blen - len, idata->conn);
    if (c <= 0)
    {
      mutt_debug(1, "imap_cmd_step: Error reading server response.\n");
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
    safe_realloc(&idata->buf, IMAP_CMD_BUFSIZE);
    idata->blen = IMAP_CMD_BUFSIZE;
    mutt_debug(3, "imap_cmd_step: shrank buffer to %u bytes\n", idata->blen);
  }

  idata->lastread = time(NULL);

  /* handle untagged messages. The caller still gets its shot afterwards. */
  if (((ascii_strncmp(idata->buf, "* ", 2) == 0) ||
       (ascii_strncmp(imap_next_word(idata->buf), "OK [", 4) == 0)) &&
      cmd_handle_untagged(idata))
    return IMAP_CMD_BAD;

  /* server demands a continuation response from us */
  if (idata->buf[0] == '+')
    return IMAP_CMD_RESPOND;

  /* look for tagged command completions */
  rc = IMAP_CMD_CONTINUE;
  c = idata->lastcmd;
  do
  {
    cmd = &idata->cmds[c];
    if (cmd->state == IMAP_CMD_NEW)
    {
      if (ascii_strncmp(idata->buf, cmd->seq, SEQLEN) == 0)
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

/* imap_code: returns 1 if the command result was OK, or 0 if NO or BAD */
int imap_code(const char *s)
{
  return cmd_status(s) == IMAP_CMD_OK;
}

/* imap_cmd_trailer: extra information after tagged command response if any */
const char *imap_cmd_trailer(struct ImapData *idata)
{
  static const char *notrailer = "";
  const char *s = idata->buf;

  if (!s)
  {
    mutt_debug(2, "imap_cmd_trailer: not a tagged response\n");
    return notrailer;
  }

  s = imap_next_word((char *) s);
  if (!s || ((ascii_strncasecmp(s, "OK", 2) != 0) && (ascii_strncasecmp(s, "NO", 2) != 0) &&
             (ascii_strncasecmp(s, "BAD", 3) != 0)))
  {
    mutt_debug(2, "imap_cmd_trailer: not a command completion: %s\n", idata->buf);
    return notrailer;
  }

  s = imap_next_word((char *) s);
  if (!s)
    return notrailer;

  return s;
}

/* imap_exec: execute a command, and wait for the response from the server.
 * Also, handle untagged responses.
 * Flags:
 *   IMAP_CMD_FAIL_OK: the calling procedure can handle failure. This is used
 *     for checking for a mailbox on append and login
 *   IMAP_CMD_PASS: command contains a password. Suppress logging.
 *   IMAP_CMD_QUEUE: only queue command, do not execute.
 * Return 0 on success, -1 on Failure, -2 on OK Failure
 */
int imap_exec(struct ImapData *idata, const char *cmdstr, int flags)
{
  int rc;

  if ((rc = cmd_start(idata, cmdstr, flags)) < 0)
  {
    cmd_handle_fatal(idata);
    return -1;
  }

  if (flags & IMAP_CMD_QUEUE)
    return 0;

  /* Allow interruptions, particularly useful if there are network problems. */
  mutt_allow_interrupt(1);
  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);
  mutt_allow_interrupt(0);

  if (rc == IMAP_CMD_NO && (flags & IMAP_CMD_FAIL_OK))
    return -2;

  if (rc != IMAP_CMD_OK)
  {
    if ((flags & IMAP_CMD_FAIL_OK) && idata->status != IMAP_FATAL)
      return -2;

    mutt_debug(1, "imap_exec: command failed: %s\n", idata->buf);
    return -1;
  }

  return 0;
}

/* imap_cmd_finish: Attempts to perform cleanup (eg fetch new mail if
 *   detected, do expunge). Called automatically by imap_cmd_step, but
 *   may be called at any time. Called by imap_check_mailbox just before
 *   the index is refreshed, for instance. */
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
    unsigned int count = idata->newMailCount;

    if (!(idata->reopen & IMAP_EXPUNGE_PENDING) &&
        (idata->reopen & IMAP_NEWMAIL_PENDING) && count > idata->max_msn)
    {
      /* read new mail messages */
      mutt_debug(2, "imap_cmd_finish: Fetching new mail\n");
      /* check_status: curs_main uses imap_check_mailbox to detect
       *   whether the index needs updating */
      idata->check_status = IMAP_NEWMAIL_PENDING;
      imap_read_headers(idata, idata->max_msn + 1, count);
    }
    else if (idata->reopen & IMAP_EXPUNGE_PENDING)
    {
      mutt_debug(2, "imap_cmd_finish: Expunging mailbox\n");
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

/* imap_cmd_idle: Enter the IDLE state. */
int imap_cmd_idle(struct ImapData *idata)
{
  int rc;

  imap_cmd_start(idata, "IDLE");
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
    mutt_debug(1, "imap_cmd_idle: error starting IDLE\n");
    return -1;
  }

  return 0;
}
