/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
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
#include "mx.h"

#include <ctype.h>
#include <stdlib.h>

/* forward declarations */
static void cmd_make_sequence (char* buf, size_t buflen);
static void cmd_parse_capabilities (IMAP_DATA *idata, char *s);
static void cmd_parse_myrights (IMAP_DATA* idata, char* s);

static char *Capabilities[] = {"IMAP4", "IMAP4rev1", "STATUS", "ACL", 
  "NAMESPACE", "AUTH=CRAM-MD5", "AUTH=KERBEROS_V4", "AUTH=GSSAPI", 
  "AUTH=LOGIN", "AUTH-LOGIN", "AUTH=PLAIN", "AUTH=SKEY", "IDLE", 
  "LOGIN-REFERRALS", "MAILBOX-REFERRALS", "QUOTA", "SCAN", "SORT", 
  "THREAD=ORDEREDSUBJECT", "UIDPLUS", "AUTH=ANONYMOUS", NULL};

/* imap_cmd_start: Given an IMAP command, send it to the server.
 *   Currently a minor convenience, but helps to route all IMAP commands
 *   through a single interface. */
void imap_cmd_start (IMAP_DATA* idata, const char* cmd)
{
  char* out;
  int outlen;

  cmd_make_sequence (idata->seq, sizeof (idata->seq));
  /* seq, space, cmd, \r\n\0 */
  outlen = strlen (idata->seq) + strlen (cmd) + 4;
  out = (char*) safe_malloc (outlen);
  snprintf (out, outlen, "%s %s\r\n", idata->seq, cmd);

  mutt_socket_write (idata->conn, out);

  safe_free ((void**) &out);
}

/* imap_cmd_finish: When the caller has finished reading command responses,
 *   it must call this routine to perform cleanup (eg fetch new mail if
 *   detected, do expunge) */
void imap_cmd_finish (IMAP_DATA* idata)
{
  if (!(idata->state == IMAP_SELECTED) || idata->ctx->closing)
  {
    idata->status = 0;
    mutt_clear_error ();
    return;
  }
  
  if ((idata->status == IMAP_NEW_MAIL || 
       idata->status == IMAP_EXPUNGE ||
       (idata->reopen & (IMAP_REOPEN_PENDING|IMAP_NEWMAIL_PENDING)))
      && (idata->reopen & IMAP_REOPEN_ALLOW))
  {
    int count = idata->newMailCount;

    if (!(idata->reopen & IMAP_REOPEN_PENDING) &&
	((idata->status == IMAP_NEW_MAIL) || (idata->reopen & IMAP_NEWMAIL_PENDING))  
	&& count > idata->ctx->msgcount)
    {
      /* read new mail messages */
      dprint (1, (debugfile, "imap_cmd_finish: fetching new mail\n"));

      count = imap_read_headers (idata->ctx, idata->ctx->msgcount, count-1)+1;
      idata->check_status = IMAP_NEW_MAIL;
      idata->reopen &= ~IMAP_NEWMAIL_PENDING;
    }
    else
    {
      imap_reopen_mailbox (idata->ctx, NULL);
      idata->check_status = IMAP_REOPENED;
      idata->reopen &= ~(IMAP_REOPEN_PENDING|IMAP_NEWMAIL_PENDING);
    }

  }
  else if (!(idata->reopen & IMAP_REOPEN_ALLOW))
  {
    if (idata->status == IMAP_NEW_MAIL)
      idata->reopen |= IMAP_NEWMAIL_PENDING;
    
    if (idata->status == IMAP_EXPUNGE)
      idata->reopen |= IMAP_REOPEN_PENDING;
  }

  idata->status = 0;
  mutt_clear_error ();
}

/* imap_code: returns 1 if the command result was OK, or 0 if NO or BAD */
int imap_code (const char *s)
{
  s += SEQLEN;
  SKIPWS (s);
  return (mutt_strncasecmp ("OK", s, 2) == 0);
}

/* imap_exec: execute a command, and wait for the response from the server.
 * Also, handle untagged responses.
 * Flags:
 *   IMAP_CMD_FAIL_OK: the calling procedure can handle failure. This is used
 *     for checking for a mailbox on append and login
 *   IMAP_CMD_PASS: command contains a password. Suppress logging.
 * Return 0 on success, -1 on Failure, -2 on OK Failure
 */
int imap_exec (char* buf, size_t buflen, IMAP_DATA* idata, const char* cmd,
  int flags)
{
  char* out;
  int outlen;

  /* create sequence for command */
  cmd_make_sequence (idata->seq, sizeof (idata->seq));
  /* seq, space, cmd, \r\n\0 */
  outlen = strlen (idata->seq) + strlen (cmd) + 4;
  out = (char*) safe_malloc (outlen);
  snprintf (out, outlen, "%s %s\r\n", idata->seq, cmd);

  mutt_socket_write_d (idata->conn, out,
    flags & IMAP_CMD_PASS ? IMAP_LOG_PASS : IMAP_LOG_CMD);

  safe_free ((void**) &out);

  do
  {
    if (mutt_socket_readln (buf, buflen, idata->conn) < 0)
      return -1;

    if (buf[0] == '*' && imap_handle_untagged (idata, buf) != 0)
      return -1;
  }
  while (mutt_strncmp (buf, idata->seq, SEQLEN) != 0);

  imap_cmd_finish (idata);

  if (!imap_code (buf))
  {
    char *pc;

    if (flags & IMAP_CMD_FAIL_OK)
      return -2;

    dprint (1, (debugfile, "imap_exec: command failed: %s\n", buf));
    pc = buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
    mutt_error ("%s", pc);
    sleep (1);

    return -1;
  }

  return 0;
}

/* imap_handle_untagged: fallback parser for otherwise unhandled messages. */
int imap_handle_untagged (IMAP_DATA *idata, char *s)
{
  char *pn;
  int count;

  s = imap_next_word (s);

  if ((idata->state == IMAP_SELECTED) && isdigit (*s))
  {
    pn = s;
    s = imap_next_word (s);

    /* EXISTS and EXPUNGE are always related to the SELECTED mailbox for the
     * connection, so update that one.
     */
    if (mutt_strncasecmp ("EXISTS", s, 6) == 0)
    {
      /* new mail arrived */
      count = atoi (pn);

      if ( (idata->status != IMAP_EXPUNGE) && count < idata->ctx->msgcount)
      {
	/* something is wrong because the server reported fewer messages
	 * than we previously saw
	 */
	mutt_error _("Fatal error.  Message count is out of sync!");
	idata->status = IMAP_FATAL;
	mx_fastclose_mailbox (idata->ctx);
	return -1;
      }
      /* at least the InterChange server sends EXISTS messages freely,
       * even when there is no new mail */
      else if (count == idata->ctx->msgcount)
	dprint (3, (debugfile,
          "imap_handle_untagged: superfluous EXISTS message.\n"));
      else
      {
	if (idata->status != IMAP_EXPUNGE)
        {
          dprint (2, (debugfile,
            "imap_handle_untagged: New mail in %s - %d messages total.\n",
            idata->mailbox, count));
	  idata->status = IMAP_NEW_MAIL;
        }
	idata->newMailCount = count;
      }
    }
    else if (mutt_strncasecmp ("EXPUNGE", s, 7) == 0)
       idata->status = IMAP_EXPUNGE;
  }
  else if (mutt_strncasecmp ("CAPABILITY", s, 10) == 0)
    cmd_parse_capabilities (idata, s);
  else if (mutt_strncasecmp ("MYRIGHTS", s, 8) == 0)
    cmd_parse_myrights (idata, s);
  else if (mutt_strncasecmp ("BYE", s, 3) == 0)
  {
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
  else if (option (OPTIMAPSERVERNOISE) && (mutt_strncasecmp ("NO", s, 2) == 0))
  {
    /* Display the warning message from the server */
    mutt_error ("%s", s+3);
    sleep (1);
  }
  else
    dprint (1, (debugfile, "imap_handle_untagged(): unhandled request: %s\n",
      s));

  return 0;
}

/* cmd_make_sequence: make a tag suitable for starting an IMAP command */
static void cmd_make_sequence (char* buf, size_t buflen)
{
  static int sequence = 0;
  
  snprintf (buf, buflen, "a%04d", sequence++);

  if (sequence > 9999)
    sequence = 0;
}

/* cmd_parse_capabilities: set capability bits according to CAPABILITY
 *   response */
static void cmd_parse_capabilities (IMAP_DATA *idata, char *s)
{
  int x;

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

/* cmd_parse_myrights: set rights bits according to MYRIGHTS response */
static void cmd_parse_myrights (IMAP_DATA* idata, char* s)
{
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
