/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/*
 * Rather crude POP3 support.
 */

#include "mutt.h"
#include "mailbox.h"
#include "mx.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

static int getLine (int fd, char *s, int len)
{
  char ch;
  int bytes = 0;

  while (read (fd, &ch, 1) > 0)
  {
    *s++ = ch;
    bytes++;
    if (ch == '\n')
    {
      *s = 0;
      return (bytes);
    }
    /* make sure not to overwrite the buffer */
    if (bytes == len - 1)
    {
      *s = 0;
      return bytes;
    }
  }
  *s = 0;
  return (-1);
}

static int getPass (void)
{
  if (!PopPass)
  {
    char tmp[SHORT_STRING];
    tmp[0] = '\0';
    if (mutt_get_password (_("POP Password: "), tmp, sizeof (tmp)) != 0
	|| *tmp == '\0')
      return 0;
    PopPass = safe_strdup (tmp);
  }
  return 1;
}

void mutt_fetchPopMail (void)
{
  struct sockaddr_in sin;
#if SIZEOF_LONG == 4
  long n;
#else
  int n;
#endif
  struct hostent *he;
  char buffer[2048];
  char msgbuf[SHORT_STRING];
  int s, i, msgs, bytes, err = 0;
  CONTEXT ctx;
  MESSAGE *msg = NULL;

  if (!PopHost)
  {
    mutt_error _("POP host is not defined.");
    return;
  }

  if (!PopUser)
  {
    mutt_error _("No POP username is defined.");
    return;
  }
    
  if (!getPass ()) return;

  s = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);

  memset ((char *) &sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons (PopPort);

  if ((n = inet_addr (NONULL(PopHost))) == -1)
  {
    /* Must be a DNS name */
    if ((he = gethostbyname (NONULL(PopHost))) == NULL)
    {
      mutt_error (_("Could not find address for host %s."), PopHost);
      return;
    }
    memcpy ((void *)&sin.sin_addr, *(he->h_addr_list), he->h_length);
  }
  else
    memcpy ((void *)&sin.sin_addr, (void *)&n, sizeof(n));
  
  mutt_message (_("Connecting to %s"), inet_ntoa (sin.sin_addr));

  if (connect (s, (struct sockaddr *) &sin, sizeof (struct sockaddr_in)) == -1)
  {
    mutt_perror ("connect");
    return;
  }
  
  if (getLine (s, buffer, sizeof (buffer)) == -1)
    goto fail;

  if (strncmp (buffer, "+OK", 3) != 0)
  {
    mutt_remove_trailing_ws (buffer);
    mutt_error (buffer);
    goto finish;
  }

  snprintf (buffer, sizeof(buffer), "user %s\r\n", PopUser);
  write (s, buffer, strlen (buffer));

  if (getLine (s, buffer, sizeof (buffer)) == -1)
    goto fail;

  if (strncmp (buffer, "+OK", 3) != 0)
  {
    mutt_remove_trailing_ws (buffer);
    mutt_error (buffer);
    goto finish;
  }
  
  snprintf (buffer, sizeof(buffer), "pass %s\r\n", NONULL(PopPass));
  write (s, buffer, strlen (buffer));
  
  if (getLine (s, buffer, sizeof (buffer)) == -1)
    goto fail;

  if (strncmp (buffer, "+OK", 3) != 0)
  {
    if(PopPass)
      memset(PopPass, 0, strlen(PopPass));
    
    safe_free((void **) &PopPass); /* void the given password */
    mutt_remove_trailing_ws (buffer);
    mutt_error (buffer[0] ? buffer : _("Server closed connection!"));
    goto finish;
  }
  
  /* find out how many messages are in the mailbox. */
  write (s, "stat\r\n", 6);
  
  if (getLine (s, buffer, sizeof (buffer)) == -1)
    goto fail;

  if (strncmp (buffer, "+OK", 3) != 0)
  {
    mutt_remove_trailing_ws (buffer);
    mutt_error (buffer);
    goto finish;
  }
  
  sscanf (buffer, "+OK %d %d", &msgs, &bytes);

  if (msgs == 0)
  {
    mutt_message _("No new mail in POP mailbox.");
    goto finish;
  }

  if (mx_open_mailbox (NONULL(Spoolfile), M_APPEND, &ctx) == NULL)
    goto finish;

  snprintf (msgbuf, sizeof (msgbuf),
	    msgs > 1 ? "_(Reading %d new message (%d bytes)...") :
		    ("Reading %d new messages (%d bytes)..."), msgs, bytes);
  mutt_message (msgbuf);

  for (i = 1 ; i <= msgs ; i++)
  {
    snprintf (buffer, sizeof(buffer), "retr %d\r\n", i);
    write (s, buffer, strlen (buffer));

    if (getLine (s, buffer, sizeof (buffer)) == -1)
    {
      mx_fastclose_mailbox (&ctx);
      goto fail;
    }

    if (strncmp (buffer, "+OK", 3) != 0)
    {
      mutt_remove_trailing_ws (buffer);
      mutt_error (buffer);
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
      int chunk;

      if ((chunk = getLine (s, buffer, sizeof (buffer))) == -1)
      {
	mutt_error _("Error reading message!");
	err = 1;
	break;
      }

      /* check to see if we got a full line */
      if (buffer[chunk-2] == '\r' && buffer[chunk-1] == '\n')
      {
	if (strcmp(".\r\n", buffer) == 0)
	{
	  /* end of message */
	  break;
	}

	/* change CRLF to just LF */
	buffer[chunk-2] = '\n';
	buffer[chunk-1] = 0;
	chunk--;

	/* see if the line was byte-stuffed */
	if (buffer[0] == '.')
	{
	  p = buffer + 1;
	  chunk--;
	}
	else
	  p = buffer;
      }
      else
	p = buffer;
      
      fwrite (p, 1, chunk, msg->fp);
    }

    if (mx_close_message (&msg) != 0)
    {
      mutt_error _("Error while writing mailbox!");
      err = 1;
    }

    if (err)
      break;

    if (option (OPTPOPDELETE))
    {
      /* delete the message on the server */
      snprintf (buffer, sizeof(buffer), "dele %d\r\n", i);
      write (s, buffer, strlen (buffer));

      /* eat the server response */
      getLine (s, buffer, sizeof (buffer));
      if (strncmp (buffer, "+OK", 3) != 0)
      {
	err = 1;
        mutt_remove_trailing_ws (buffer);
	mutt_error (buffer);
	break;
      }
    }

    mutt_message (_("%s [%d messages read]"), msgbuf, i);
  }

  if (msg)
    mx_close_message (&msg);
  mx_close_mailbox (&ctx);

  if (err)
  {
    /* make sure no messages get deleted */
    write (s, "rset\r\n", 6);
    getLine (s, buffer, sizeof (buffer)); /* snarf the response */
  }

finish:

  /* exit gracefully */
  write (s, "quit\r\n", 6);
  getLine (s, buffer, sizeof (buffer)); /* snarf the response */
  close (s);
  return;

  /* not reached */

fail:

  mutt_error _("Server closed connection!");
  close (s);
}
