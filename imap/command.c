/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* command.c: routines for sending commands to an IMAP server and parsing
 *  responses */

#include "mutt.h"
#include "imap_private.h"
#include "message.h"
#include "mx.h"

#include <ctype.h>
#include <stdlib.h>

#define IMAP_CMD_BUFSIZE 512

/* forward declarations */
static void cmd_finish (IMAP_DATA* idata);
static void cmd_handle_fatal (IMAP_DATA* idata);
static int cmd_handle_untagged (IMAP_DATA* idata);
static void cmd_make_sequence (IMAP_DATA* idata);
static void cmd_parse_capabilities (IMAP_DATA* idata, char* s);
static void cmd_parse_expunge (IMAP_DATA* idata, char* s);
static void cmd_parse_myrights (IMAP_DATA* idata, char* s);

static char *Capabilities[] = {
  "IMAP4",
  "IMAP4rev1",
  "STATUS",
  "ACL", 
  "NAMESPACE",
  "AUTH=CRAM-MD5",
  "AUTH=GSSAPI",
  "AUTH=ANONYMOUS",
  "STARTTLS",
  "LOGINDISABLED",

  NULL
};

/* imap_cmd_start: Given an IMAP command, send it to the server.
 *   Currently a minor convenience, but helps to route all IMAP commands
 *   through a single interface. */
int imap_cmd_start (IMAP_DATA* idata, const char* cmd)
{
  char* out;
  int outlen;
  int rc;

  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal (idata);
    return IMAP_CMD_BAD;
  }

  cmd_make_sequence (idata);
  /* seq, space, cmd, \r\n\0 */
  outlen = strlen (idata->cmd.seq) + strlen (cmd) + 4;
  out = (char*) safe_malloc (outlen);
  snprintf (out, outlen, "%s %s\r\n", idata->cmd.seq, cmd);

  rc = mutt_socket_write (idata->conn, out);

  FREE (&out);

  return (rc < 0) ? IMAP_CMD_BAD : 0;
}

/* imap_cmd_step: Reads server responses from an IMAP command, detects
 *   tagged completion response, handles untagged messages, can read
 *   arbitrarily large strings (using malloc, so don't make it _too_
 *   large!). */
int imap_cmd_step (IMAP_DATA* idata)
{
  IMAP_COMMAND* cmd = &idata->cmd;
  unsigned int len = 0;
  int c;

  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal (idata);
    return IMAP_CMD_BAD;
  }

  /* read into buffer, expanding buffer as necessary until we have a full
   * line */
  do
  {
    if (len == cmd->blen)
    {
      safe_realloc ((void**) &cmd->buf, cmd->blen + IMAP_CMD_BUFSIZE);
      cmd->blen = cmd->blen + IMAP_CMD_BUFSIZE;
      dprint (3, (debugfile, "imap_cmd_step: grew buffer to %u bytes\n",
		  cmd->blen));
    }

    if ((c = mutt_socket_readln (cmd->buf + len, cmd->blen - len,
      idata->conn)) < 0)
    {
      dprint (1, (debugfile, "imap_cmd_step: Error while reading server response, closing connection.\n"));
      mutt_socket_close (idata->conn);
      idata->status = IMAP_FATAL;
      return IMAP_CMD_BAD;
    }

    len += c;
  }
  /* if we've read all the way to the end of the buffer, we haven't read a
   * full line (mutt_socket_readln strips the \r, so we always have at least
   * one character free when we've read a full line) */
  while (len == cmd->blen);

  /* don't let one large string make cmd->buf hog memory forever */
  if ((cmd->blen > IMAP_CMD_BUFSIZE) && (len <= IMAP_CMD_BUFSIZE))
  {
    safe_realloc ((void**) &cmd->buf, IMAP_CMD_BUFSIZE);
    cmd->blen = IMAP_CMD_BUFSIZE;
    dprint (3, (debugfile, "imap_cmd_step: shrank buffer to %u bytes\n", cmd->blen));
  }
  
  /* handle untagged messages. The caller still gets its shot afterwards. */
  if (!strncmp (cmd->buf, "* ", 2) &&
      cmd_handle_untagged (idata))
    return IMAP_CMD_BAD;

  /* server demands a continuation response from us */
  if (!strncmp (cmd->buf, "+ ", 2))
  {
    return IMAP_CMD_RESPOND;
  }

  /* tagged completion code */
  if (!mutt_strncmp (cmd->buf, cmd->seq, SEQLEN))
  {
    cmd_finish (idata);
    return imap_code (cmd->buf) ? IMAP_CMD_OK : IMAP_CMD_NO;
  }

  return IMAP_CMD_CONTINUE;
}

/* imap_code: returns 1 if the command result was OK, or 0 if NO or BAD */
int imap_code (const char *s)
{
  s += SEQLEN;
  SKIPWS (s);
  return (ascii_strncasecmp ("OK", s, 2) == 0);
}

/* imap_exec: execute a command, and wait for the response from the server.
 * Also, handle untagged responses.
 * Flags:
 *   IMAP_CMD_FAIL_OK: the calling procedure can handle failure. This is used
 *     for checking for a mailbox on append and login
 *   IMAP_CMD_PASS: command contains a password. Suppress logging.
 * Return 0 on success, -1 on Failure, -2 on OK Failure
 */
int imap_exec (IMAP_DATA* idata, const char* cmd, int flags)
{
  char* out;
  int outlen;
  int rc;

  if (idata->status == IMAP_FATAL)
  {
    cmd_handle_fatal (idata);
    return -1;
  }

  /* create sequence for command */
  cmd_make_sequence (idata);
  /* seq, space, cmd, \r\n\0 */
  outlen = strlen (idata->cmd.seq) + strlen (cmd) + 4;
  out = (char*) safe_malloc (outlen);
  snprintf (out, outlen, "%s %s\r\n", idata->cmd.seq, cmd);

  rc = mutt_socket_write_d (idata->conn, out,
    flags & IMAP_CMD_PASS ? IMAP_LOG_PASS : IMAP_LOG_CMD);
  safe_free ((void**) &out);

  if (rc < 0)
    return -1;

  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc == IMAP_CMD_NO && (flags & IMAP_CMD_FAIL_OK))
    return -2;

  if (rc != IMAP_CMD_OK)
  {
    char *pc;

    if (flags & IMAP_CMD_FAIL_OK)
      return -2;

    dprint (1, (debugfile, "imap_exec: command failed: %s\n", idata->cmd.buf));
    pc = idata->cmd.buf;
    pc = imap_next_word (pc);
    mutt_error ("%s", pc);
    sleep (2);

    return -1;
  }

  return 0;
}

/* imap_cmd_running: Returns whether an IMAP command is in progress. */
int imap_cmd_running (IMAP_DATA* idata)
{
  if (idata->cmd.state == IMAP_CMD_CONTINUE ||
      idata->cmd.state == IMAP_CMD_RESPOND)
    return 1;

  return 0;
}

/* cmd_finish: When the caller has finished reading command responses,
 *   it must call this routine to perform cleanup (eg fetch new mail if
 *   detected, do expunge). Called automatically by imap_cmd_step */
static void cmd_finish (IMAP_DATA* idata)
{
  if (!(idata->state == IMAP_SELECTED) || idata->ctx->closing)
    return;
  
  if ((idata->reopen & IMAP_REOPEN_ALLOW) &&
      (idata->reopen & (IMAP_EXPUNGE_PENDING|IMAP_NEWMAIL_PENDING)))
  {
    int count = idata->newMailCount;

    if (!(idata->reopen & IMAP_EXPUNGE_PENDING) &&
	(idata->reopen & IMAP_NEWMAIL_PENDING)
	&& count > idata->ctx->msgcount)
    {
      /* read new mail messages */
      dprint (2, (debugfile, "cmd_finish: Fetching new mail\n"));
      /* check_status: curs_main uses imap_check_mailbox to detect
       *   whether the index needs updating */
      idata->check_status = IMAP_NEWMAIL_PENDING;
      idata->reopen &= ~IMAP_NEWMAIL_PENDING;
      count = imap_read_headers (idata, idata->ctx->msgcount, count-1)+1;
    }
    else
    {
      dprint (2, (debugfile, "cmd_finish: Expunging mailbox\n"));
      imap_expunge_mailbox (idata);
      idata->reopen &= ~(IMAP_EXPUNGE_PENDING|IMAP_NEWMAIL_PENDING);
    }
  }

  idata->status = 0;
}

/* cmd_handle_fatal: when IMAP_DATA is in fatal state, do what we can */
static void cmd_handle_fatal (IMAP_DATA* idata)
{
  if ((idata->state == IMAP_SELECTED) &&
      (idata->reopen & IMAP_REOPEN_ALLOW) &&
      !idata->ctx->closing)
  {
    idata->status = IMAP_BYE;
    idata->state = IMAP_DISCONNECTED;
    mx_fastclose_mailbox (idata->ctx);
  }
}

/* cmd_handle_untagged: fallback parser for otherwise unhandled messages. */
static int cmd_handle_untagged (IMAP_DATA* idata)
{
  char* s;
  char* pn;
  int count;

  s = imap_next_word (idata->cmd.buf);

  if ((idata->state == IMAP_SELECTED) && isdigit (*s))
  {
    pn = s;
    s = imap_next_word (s);

    /* EXISTS and EXPUNGE are always related to the SELECTED mailbox for the
     * connection, so update that one.
     */
    if (ascii_strncasecmp ("EXISTS", s, 6) == 0)
    {
      dprint (2, (debugfile, "Handling EXISTS\n"));

      /* new mail arrived */
      count = atoi (pn);

      if ( !(idata->reopen & IMAP_EXPUNGE_PENDING) &&
	   count < idata->ctx->msgcount)
      {
	/* something is wrong because the server reported fewer messages
	 * than we previously saw
	 */
	mutt_error _("Fatal error.  Message count is out of sync!");
	idata->status = IMAP_FATAL;
	return -1;
      }
      /* at least the InterChange server sends EXISTS messages freely,
       * even when there is no new mail */
      else if (count == idata->ctx->msgcount)
	dprint (3, (debugfile,
          "cmd_handle_untagged: superfluous EXISTS message.\n"));
      else
      {
	if (!(idata->reopen & IMAP_EXPUNGE_PENDING))
        {
          dprint (2, (debugfile,
            "cmd_handle_untagged: New mail in %s - %d messages total.\n",
            idata->mailbox, count));
	  idata->reopen |= IMAP_NEWMAIL_PENDING;
        }
	idata->newMailCount = count;
      }
    }
    else if (ascii_strncasecmp ("EXPUNGE", s, 7) == 0)
      /* pn vs. s: need initial seqno */
      cmd_parse_expunge (idata, pn);
  }
  else if (ascii_strncasecmp ("CAPABILITY", s, 10) == 0)
    cmd_parse_capabilities (idata, s);
  else if (ascii_strncasecmp ("MYRIGHTS", s, 8) == 0)
    cmd_parse_myrights (idata, s);
  else if (ascii_strncasecmp ("BYE", s, 3) == 0)
  {
    dprint (2, (debugfile, "Handling BYE\n"));

    /* check if we're logging out */
    if (idata->status == IMAP_BYE)
      return 0;

    /* server shut down our connection */
    s += 3;
    SKIPWS (s);
    mutt_error ("%s", s);
    idata->status = IMAP_BYE;
    if (idata->state == IMAP_SELECTED)
      mx_fastclose_mailbox (idata->ctx);
    mutt_socket_close (idata->conn);
    idata->state = IMAP_DISCONNECTED;

    return -1;
  }
  else if (option (OPTIMAPSERVERNOISE) && (ascii_strncasecmp ("NO", s, 2) == 0))
  {
    dprint (2, (debugfile, "Handling untagged NO\n"));

    /* Display the warning message from the server */
    mutt_error ("%s", s+3);
    sleep (2);
  }

  return 0;
}

/* cmd_make_sequence: make a tag suitable for starting an IMAP command */
static void cmd_make_sequence (IMAP_DATA* idata)
{
  snprintf (idata->cmd.seq, sizeof (idata->cmd.seq), "a%04d", idata->seqno++);

  if (idata->seqno > 9999)
    idata->seqno = 0;
}

/* cmd_parse_capabilities: set capability bits according to CAPABILITY
 *   response */
static void cmd_parse_capabilities (IMAP_DATA* idata, char* s)
{
  int x;

  dprint (2, (debugfile, "Handling CAPABILITY\n"));

  idata->capstr = safe_strdup (imap_next_word (s));
  
  while (*s) 
  {
    for (x = 0; x < CAPMAX; x++)
      if (imap_wordcasecmp(Capabilities[x], s) == 0)
      {
	mutt_bit_set (idata->capabilities, x);
	break;
      }
    s = imap_next_word (s);
  }
}

/* cmd_parse_expunge: mark headers with new sequence ID and mark idata to
 *   be reopened at our earliest convenience */
static void cmd_parse_expunge (IMAP_DATA* idata, char* s)
{
  int expno, cur;
  HEADER* h;

  dprint (2, (debugfile, "Handling EXPUNGE\n"));

  expno = atoi (s);

  /* walk headers, zero seqno of expunged message, decrement seqno of those
   * above. Possibly we could avoid walking the whole list by resorting
   * and guessing a good starting point, but I'm guessing the resort would
   * nullify the gains */
  for (cur = 0; cur < idata->ctx->msgcount; cur++)
  {
    h = idata->ctx->hdrs[cur];

    if (h->index+1 == expno)
      h->index = -1;
    else if (h->index+1 > expno)
      h->index--;
  }

  idata->reopen |= IMAP_EXPUNGE_PENDING;
}

/* cmd_parse_myrights: set rights bits according to MYRIGHTS response */
static void cmd_parse_myrights (IMAP_DATA* idata, char* s)
{
  dprint (2, (debugfile, "Handling MYRIGHTS\n"));

  s = imap_next_word (s);
  s = imap_next_word (s);

  /* zero out current rights set */
  memset (idata->rights, 0, sizeof (idata->rights));

  while (*s && !isspace(*s))
  {
    switch (*s) 
    {
      case 'l':
	mutt_bit_set (idata->rights, IMAP_ACL_LOOKUP);
	break;
      case 'r':
	mutt_bit_set (idata->rights, IMAP_ACL_READ);
	break;
      case 's':
	mutt_bit_set (idata->rights, IMAP_ACL_SEEN);
	break;
      case 'w':
	mutt_bit_set (idata->rights, IMAP_ACL_WRITE);
	break;
      case 'i':
	mutt_bit_set (idata->rights, IMAP_ACL_INSERT);
	break;
      case 'p':
	mutt_bit_set (idata->rights, IMAP_ACL_POST);
	break;
      case 'c':
	mutt_bit_set (idata->rights, IMAP_ACL_CREATE);
	break;
      case 'd':
	mutt_bit_set (idata->rights, IMAP_ACL_DELETE);
	break;
      case 'a':
	mutt_bit_set (idata->rights, IMAP_ACL_ADMIN);
	break;
    }
    s++;
  }
}
