/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 2000 Vsevolod Volkov <vvv@mutt.kiev.ua>
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

/*
 * Rather crude POP3 support.
 */

#include "mutt.h"
#include "mailbox.h"
#include "mx.h"
#include "mutt_socket.h"

#include <string.h>
#include <unistd.h>

/* pop_authenticate: loop until success or user abort. */
int pop_authenticate (CONNECTION *conn)
{
  char buf[LONG_STRING];
  char user[SHORT_STRING];
  char pass[SHORT_STRING];

  FOREVER
  {
    if (! (conn->account.flags & M_ACCT_USER))
    {
      if (!PopUser)
      {
	strfcpy (user, NONULL (Username), sizeof (user));
	if (mutt_get_field (_("POP Username: "), user, sizeof (user), 0) != 0 ||
	    !user[0])
	  return 0;
      }
      else
	strfcpy (user, PopUser, sizeof (user));
    }
    else
      strfcpy (user, conn->account.user, sizeof (user));

    if (! (conn->account.flags & M_ACCT_PASS))
    {
      if (!PopPass)
      {
	pass[0]=0;
	snprintf (buf, sizeof (buf), _("Password for %s@%s: "), user,
          conn->account.host);
	if (mutt_get_field (buf, pass, sizeof (pass), M_PASS) != 0 || !pass[0])
	  return 0;
      }
      else
	strfcpy (pass, PopPass, sizeof (pass));
    }
    else
      strfcpy (pass, conn->account.pass, sizeof (pass));

    mutt_message _("Logging in...");

    snprintf (buf, sizeof (buf), "user %s\r\n", user);
    mutt_socket_write (conn, buf);

    if (mutt_socket_readln (buf, sizeof (buf), conn) < 0)
      return -1;

    if (!mutt_strncmp (buf, "+OK", 3))
    {
      snprintf (buf, sizeof (buf), "pass %s\r\n", pass);
      mutt_socket_write_d (conn, buf, 5);

#ifdef DEBUG
      /* don't print the password unless we're at the ungodly debugging level */
      if (debuglevel < M_SOCK_LOG_FULL)
	dprint (M_SOCK_LOG_CMD, (debugfile, "> pass *\r\n"));
#endif

      if (mutt_socket_readln (buf, sizeof (buf), conn) < 0)
	return -1;

      if (!mutt_strncmp (buf, "+OK", 3))
      {
	/* If they have a successful login, we may as well cache the 
	 * user/password. */
	if (!(conn->account.flags & M_ACCT_USER))
	  strfcpy (conn->account.user, user, sizeof (conn->account.user));
	if (!(conn->account.flags & M_ACCT_PASS))
	  strfcpy (conn->account.pass, pass, sizeof (conn->account.pass));
	conn->account.flags |= (M_ACCT_USER | M_ACCT_PASS);

	return 1;
      }
    }

    mutt_remove_trailing_ws (buf);
    mutt_error (_("Login failed: %s"), buf);
    sleep (1);

    if (!(conn->account.flags & M_ACCT_USER))
      FREE (&PopUser);
    if (!(conn->account.flags & M_ACCT_PASS))
      FREE (&PopPass);
    conn->account.flags &= ~M_ACCT_PASS;
  }
}

void mutt_fetchPopMail (void)
{
  char buffer[LONG_STRING];
  char msgbuf[SHORT_STRING];
  int i, delanswer, last = 0, msgs, bytes, err = 0;
  CONTEXT ctx;
  MESSAGE *msg = NULL;
  ACCOUNT account;
  CONNECTION *conn;

  if (!PopHost)
  {
    mutt_error _("POP host is not defined.");
    return;
  }

  strfcpy (account.host, PopHost, sizeof (account.host));
  account.port = PopPort;
  account.type = M_ACCT_TYPE_POP;
  account.flags = 0;
  conn = mutt_socket_find (&account, 0);
  if (mutt_socket_open (conn) < 0)
    return;

  if (mutt_socket_readln (buffer, sizeof (buffer), conn) < 0)
    goto fail;

  if (mutt_strncmp (buffer, "+OK", 3))
  {
    mutt_remove_trailing_ws (buffer);
    mutt_error ("%s", buffer);
    goto finish;
  }

  switch (pop_authenticate (conn))
  {
    case -1:
      goto fail;
    case 0:
    {
      mutt_clear_error ();
      goto finish;
    }
  }

  mutt_message _("Checking for new messages...");

  /* find out how many messages are in the mailbox. */
  mutt_socket_write (conn, "stat\r\n");

  if (mutt_socket_readln (buffer, sizeof (buffer), conn) < 0)
    goto fail;

  if (mutt_strncmp (buffer, "+OK", 3) != 0)
  {
    mutt_remove_trailing_ws (buffer);
    mutt_error ("%s", buffer);
    goto finish;
  }

  sscanf (buffer, "+OK %d %d", &msgs, &bytes);

  if (msgs == 0)
  {
    mutt_message _("No new mail in POP mailbox.");
    goto finish;
  }

  if (mx_open_mailbox (NONULL (Spoolfile), M_APPEND, &ctx) == NULL)
    goto finish;

  /* only get unread messages */
  if (option (OPTPOPLAST))
  {
    mutt_socket_write (conn, "last\r\n");
    if (mutt_socket_readln (buffer, sizeof (buffer), conn) == -1)
      goto fail;

    if (mutt_strncmp (buffer, "+OK", 3) == 0)
      sscanf (buffer, "+OK %d", &last);
    else
      /* ignore an error here and assume all messages are new */
      last = 0;
  }

  if (msgs - last)
    delanswer = query_quadoption (OPT_POPDELETE, _("Delete messages from server?"));

  snprintf (msgbuf, sizeof (msgbuf),
	    msgs > 1 ? _("Reading new messages (%d bytes)...") :
		       _("Reading new message (%d bytes)..."), bytes);
  mutt_message ("%s", msgbuf);

  for (i = last + 1 ; i <= msgs ; i++)
  {
    snprintf (buffer, sizeof (buffer), "retr %d\r\n", i);
    mutt_socket_write (conn, buffer);

    if (mutt_socket_readln (buffer, sizeof (buffer), conn) < 0)
    {
      mx_fastclose_mailbox (&ctx);
      goto fail;
    }

    if (mutt_strncmp (buffer, "+OK", 3) != 0)
    {
      mutt_remove_trailing_ws (buffer);
      mutt_error ("%s", buffer);
      break;
    }

    if ((msg = mx_open_new_message (&ctx, NULL, M_ADD_FROM)) == NULL)
    {
      err = 1;
      break;
    }

    /* Now read the actual message. */
    FOREVER
    {
      char *p;
      int chunk, tail = 0;

      if ((chunk = mutt_socket_readln_d (buffer, sizeof (buffer), conn, 3)) < 0)
      {
	mutt_error _("Error reading message!");
	err = 1;
	break;
      }

      p = buffer;
      if (!tail && buffer[0] == '.')
      {
	if (buffer[1] == '.')
	  p = buffer + 1;
	else
	  break; /* end of message */
      }

      fputs (p, msg->fp);
      if (chunk >= sizeof (buffer))
      {
	tail = 1;
      }
      else
      {
	fputc ('\n', msg->fp);
	tail = 0;
      }
    }

    if (mx_commit_message (msg, &ctx) != 0)
    {
      mutt_error _("Error while writing mailbox!");
      err = 1;
    }

    mx_close_message (&msg);

    if (err)
      break;

    if (delanswer == M_YES)
    {
      /* delete the message on the server */
      snprintf (buffer, sizeof (buffer), "dele %d\r\n", i);
      mutt_socket_write (conn, buffer);

      /* eat the server response */
      mutt_socket_readln (buffer, sizeof (buffer), conn);
      if (mutt_strncmp (buffer, "+OK", 3) != 0)
      {
	err = 1;
        mutt_remove_trailing_ws (buffer);
	mutt_error ("%s", buffer);
	break;
      }
    }

    if ( msgs > 1)
      mutt_message (_("%s [%d of %d messages read]"), msgbuf, i, msgs);
    else
      mutt_message (_("%s [%d message read]"), msgbuf, msgs);
  }

  if (msg)
  {
    if (mx_commit_message (msg, &ctx) != 0)
      err = 1;
    mx_close_message (&msg);
  }
  mx_close_mailbox (&ctx, NULL);

  if (err)
  {
    /* make sure no messages get deleted */
    mutt_socket_write (conn, "rset\r\n");
    mutt_socket_readln (buffer, sizeof (buffer), conn); /* snarf the response */
  }

finish:

  /* exit gracefully */
  mutt_socket_write (conn, "quit\r\n");
  mutt_socket_readln (buffer, sizeof (buffer), conn); /* snarf the response */
  mutt_socket_close (conn);
  return;

  /* not reached */

fail:

  mutt_error _("Server closed connection!");
  mutt_socket_close (conn);
}
