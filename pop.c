/*
 * Copyright (C) 2000 Vsevolod Volkov <vvv@mutt.org.ua>
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

#include "mutt.h"
#include "mx.h"
#include "pop.h"

#ifdef HAVE_PGP
#include "pgp.h"
#endif

#include <string.h>
#include <unistd.h>

/* write line to file */
static int fetch_message (char *line, void *file)
{
  FILE *f = (FILE *) file;

  fputs (line, f);
  if (fputc ('\n', f) == EOF)
    return -1;

  return 0;
}

/*
 * Read header
 * returns:
 *  0 on success
 * -1 - conection lost,
 * -2 - invalid command or execution error,
 * -3 - error writing to tempfile
 */
static int pop_read_header (POP_DATA *pop_data, HEADER *h)
{
  FILE *f;
  int ret, index;
  long length;
  char buf[LONG_STRING];
  char tempfile[_POSIX_PATH_MAX];

  mutt_mktemp (tempfile);
  if (!(f = safe_fopen (tempfile, "w+")))
  {
    mutt_perror (tempfile);
    return -3;
  }

  snprintf (buf, sizeof (buf), "LIST %d\r\n", h->refno);
  ret = pop_query (pop_data, buf, sizeof (buf));
  if (ret == 0)
  {
    sscanf (buf, "+OK %d %ld", &index, &length);

    snprintf (buf, sizeof (buf), "TOP %d 0\r\n", h->refno);
    ret = pop_fetch_data (pop_data, buf, NULL, fetch_message, f);

    if (pop_data->cmd_top == 2)
    {
      if (ret == 0)
      {
	pop_data->cmd_top = 1;

	dprint (1, (debugfile, "pop_read_header: set TOP capability\n"));
      }

      if (ret == -2)
      {
	pop_data->cmd_top = 0;

	dprint (1, (debugfile, "pop_read_header: unset TOP capability\n"));
	snprintf (pop_data->err_msg, sizeof (pop_data->err_msg),
		_("Command TOP is not supported by server."));
      }
    }
  }

  switch (ret)
  {
    case 0:
    {
      rewind (f);
      h->env = mutt_read_rfc822_header (f, h, 0, 0);
      h->content->length = length - h->content->offset + 1;
      rewind (f);
      while (!feof (f))
      {
	h->content->length--;
	fgets (buf, sizeof (buf), f);
      }
      break;
    }
    case -2:
    {
      mutt_error ("%s", pop_data->err_msg);
      break;
    }
    case -3:
    {
      mutt_error _("Can't write header to temporary file!");
      break;
    }
  }

  fclose (f);
  unlink (tempfile);
  return ret;
}

/* parse UIDL */
static int fetch_uidl (char *line, void *data)
{
  int i, index;
  CONTEXT *ctx = (CONTEXT *)data;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  sscanf (line, "%d %s", &index, line);
  for (i = 0; i < ctx->msgcount; i++)
    if (!mutt_strcmp (line, ctx->hdrs[i]->data))
      break;

  if (i == ctx->msgcount)
  {
    dprint (1, (debugfile, "pop_fetch_headers: new header %d %s\n", index, line));

    if (i >= ctx->hdrmax)
      mx_alloc_memory(ctx);

    ctx->msgcount++;
    ctx->hdrs[i] = mutt_new_header ();
    ctx->hdrs[i]->data = safe_strdup (line);
  }
  else if (ctx->hdrs[i]->index != index - 1)
    pop_data->clear_cache = 1;

  ctx->hdrs[i]->refno = index;
  ctx->hdrs[i]->index = index - 1;

  return 0;
}

/*
 * Read headers
 * returns:
 *  0 on success
 * -1 - conection lost,
 * -2 - invalid command or execution error,
 * -3 - error writing to tempfile
 */
static int pop_fetch_headers (CONTEXT *ctx)
{
  int i, ret, old_count, new_count;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  time (&pop_data->check_time);
  pop_data->clear_cache = 0;

  for (i = 0; i < ctx->msgcount; i++)
    ctx->hdrs[i]->refno = -1;

  old_count = ctx->msgcount;
  ret = pop_fetch_data (pop_data, "UIDL\r\n", NULL, fetch_uidl, ctx);
  new_count = ctx->msgcount;
  ctx->msgcount = old_count;

  if (pop_data->cmd_uidl == 2)
  {
    if (ret == 0)
    {
      pop_data->cmd_uidl = 1;

      dprint (1, (debugfile, "pop_fetch_headers: set UIDL capability\n"));
    }

    if (ret == -2 && pop_data->cmd_uidl == 2)
    {
      pop_data->cmd_uidl = 0;

      dprint (1, (debugfile, "pop_fetch_headers: unset UIDL capability\n"));
      snprintf (pop_data->err_msg, sizeof (pop_data->err_msg),
	      _("Command UIDL is not supported by server."));
    }
  }

  if (ret == 0)
  {
    for (i = 0; i < old_count; i++)
      if (ctx->hdrs[i]->refno == -1)
	ctx->hdrs[i]->deleted = 1;

    for (i = old_count; i < new_count; i++)
    {
      mutt_message (_("Fetching message headers... [%d/%d]"),
		    i + 1 - old_count, new_count - old_count);

      ret = pop_read_header (pop_data, ctx->hdrs[i]);
      if (ret < 0)
	break;

      mx_update_context (ctx);
    }
  }

  if (ret < 0)
  {
    for (i = ctx->msgcount; i < new_count; i++)
      mutt_free_header (&ctx->hdrs[i]);
    return ret;
  }

  mutt_clear_error ();
  return (new_count - old_count);
}

/* open POP mailbox - fetch only headers */
int pop_open_mailbox (CONTEXT *ctx)
{
  int ret;
  char buf[LONG_STRING];
  ACCOUNT acct;
  POP_DATA *pop_data;
  ciss_url_t url;

  if (pop_parse_path (ctx->path, &acct))
  {
    mutt_error ("%s is an invalid POP path", ctx->path);
    mutt_sleep (2);
    return -1;
  }

  mutt_account_tourl (&acct, &url);
  url.path = NULL;
  url_ciss_tostring (&url, buf, sizeof (buf));

  FREE (&ctx->path);
  ctx->path = safe_strdup (buf);

  pop_data = safe_calloc (1, sizeof (POP_DATA));
  pop_data->conn = mutt_conn_find (NULL, &acct);
  ctx->data = pop_data;

  if (pop_open_connection (pop_data) < 0)
    return -1;

  pop_data->conn->data = pop_data;

  FOREVER
  {
    if (pop_reconnect (ctx) < 0)
      return -1;

    ctx->size = pop_data->size;

    mutt_message _("Fetching list of messages...");

    ret = pop_fetch_headers (ctx);

    if (ret >= 0)
      return 0;

    if (ret < -1)
    {
      mutt_sleep (2);
      return -1;
    }
  }
}

/* delete all cached messages */
static void pop_clear_cache (POP_DATA *pop_data)
{
  int i;

  if (!pop_data->clear_cache)
    return;

  dprint (1, (debugfile, "pop_clear_cache: delete cached messages\n"));

  for (i = 0; i < POP_CACHE_LEN; i++)
  {
    if (pop_data->cache[i].path)
    {
      unlink (pop_data->cache[i].path);
      safe_free ((void **) &pop_data->cache[i].path);
    }
  }
}

/* close POP mailbox */
void pop_close_mailbox (CONTEXT *ctx)
{
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  if (!pop_data)
    return;

  pop_logout (ctx);

  if (pop_data->status != POP_NONE)
    mutt_socket_close (pop_data->conn);

  pop_data->status = POP_NONE;

  pop_data->clear_cache = 1;
  pop_clear_cache (pop_data);

  if (!pop_data->conn->data)
    mutt_socket_free (pop_data->conn);

  return;
}

/* fetch message from POP server */
int pop_fetch_message (MESSAGE* msg, CONTEXT* ctx, int msgno)
{
  int ret;
  void *uidl;
  char buf[LONG_STRING];
  char path[_POSIX_PATH_MAX];
  char *m = _("Fetching message...");
  POP_DATA *pop_data = (POP_DATA *)ctx->data;
  POP_CACHE *cache;
  HEADER *h = ctx->hdrs[msgno];

  /* see if we already have the message in our cache */
  cache = &pop_data->cache[h->index % POP_CACHE_LEN];

  if (cache->path)
  {
    if (cache->index == h->index)
    {
      /* yes, so just return a pointer to the message */
      msg->fp = fopen (cache->path, "r");
      if (msg->fp)
	return 0;

      mutt_perror (cache->path);
      mutt_sleep (2);
      return -1;
    }
    else
    {
      /* clear the previous entry */
      unlink (cache->path);
      FREE (&cache->path);
    }
  }

  FOREVER
  {
    if (pop_reconnect (ctx) < 0)
      return -1;

    /* verify that massage index is correct */
    if (h->refno < 0)
    {
      mutt_error _("The message index is incorrect. Try reopening the mailbox.");
      mutt_sleep (2);
      return -1;
    }

    mutt_message (m);

    mutt_mktemp (path);
    msg->fp = safe_fopen (path, "w+");
    if (!msg->fp)
    {
      mutt_perror (path);
      mutt_sleep (2);
      return -1;
    }

    snprintf (buf, sizeof (buf), "RETR %d\r\n", h->refno);

    ret = pop_fetch_data (pop_data, buf, m, fetch_message, msg->fp);
    if (ret == 0)
      break;

    safe_fclose (&msg->fp);
    unlink (path);

    if (ret == -2)
    {
      mutt_error ("%s", pop_data->err_msg);
      mutt_sleep (2);
      return -1;
    }

    if (ret == -3)
    {
      mutt_error _("Can't write message to temporary file!");
      mutt_sleep (2);
      return -1;
    }
  }

  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.
   */
  cache->index = h->index;
  cache->path = safe_strdup (path);
  rewind (msg->fp);
  uidl = h->data;
  mutt_free_envelope (&h->env);
  h->env = mutt_read_rfc822_header (msg->fp, h, 0, 0);
  h->data = uidl;
  h->lines = 0;
  fgets (buf, sizeof (buf), msg->fp);
  while (!feof (msg->fp))
  {
    ctx->hdrs[msgno]->lines++;
    fgets (buf, sizeof (buf), msg->fp);
  }

  h->content->length = ftell (msg->fp) - h->content->offset;

  /* This needs to be done in case this is a multipart message */
#ifdef HAVE_PGP
  h->pgp = pgp_query (h->content);
#endif /* HAVE_PGP */

  mutt_clear_error();
  rewind (msg->fp);

  return 0;
}

/* update POP mailbox - delete messages from server */
int pop_sync_mailbox (CONTEXT *ctx, int *index_hint)
{
  int i, ret;
  char buf[LONG_STRING];
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  pop_data->check_time = 0;

  FOREVER
  {
    if (pop_reconnect (ctx) < 0)
      return -1;

    mutt_message (_("Marking %d messages deleted..."), ctx->deleted);

    for (i = 0, ret = 0; ret == 0 && i < ctx->msgcount; i++)
    {
      if (ctx->hdrs[i]->deleted)
      {
	snprintf (buf, sizeof (buf), "DELE %d\r\n", ctx->hdrs[i]->refno);
	ret = pop_query (pop_data, buf, sizeof (buf));
      }
    }

    if (ret == 0)
    {
      strfcpy (buf, "QUIT\r\n", sizeof (buf));
      ret = pop_query (pop_data, buf, sizeof (buf));
    }

    if (ret == 0)
    {
      pop_data->clear_cache = 1;
      pop_clear_cache (pop_data);
      pop_data->status = POP_DISCONNECTED;
      return 0;
    }

    if (ret == -2)
    {
      mutt_error ("%s", pop_data->err_msg);
      mutt_sleep (2);
      return -1;
    }
  }
}

/* Check for new messages and fetch headers */
int pop_check_mailbox (CONTEXT *ctx, int *index_hint)
{
  int ret;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  if ((pop_data->check_time + PopCheckTimeout) > time (NULL))
    return 0;

  pop_logout (ctx);

  mutt_socket_close (pop_data->conn);

  if (pop_open_connection (pop_data) < 0)
    return -1;

  ctx->size = pop_data->size;

  mutt_message _("Checking for new messages...");

  ret = pop_fetch_headers (ctx);
  pop_clear_cache (pop_data);

  if (ret < 0)
    return -1;

  if (ret > 0)
    return M_NEW_MAIL;

  return 0;
}

/* Fetch messages and save them in $spoolfile */
void pop_fetch_mail (void)
{
  char buffer[LONG_STRING];
  char msgbuf[SHORT_STRING];
  char *url, *p;
  int i, delanswer, last = 0, msgs, bytes, rset = 0, ret;
  CONTEXT ctx;
  MESSAGE *msg = NULL;
  ACCOUNT acct;
  POP_DATA *pop_data;

  if (!PopHost)
  {
    mutt_error _("POP host is not defined.");
    return;
  }

  url = p = safe_calloc (strlen (PopHost) + 7, sizeof (char));
  if (url_check_scheme (PopHost) == U_UNKNOWN)
  {
    strcpy (url, "pop://");	/* __STRCPY_CHECKED__ */
    p = strchr (url, '\0');
  }
  strcpy (p, PopHost);		/* __STRCPY_CHECKED__ */

  if (pop_parse_path (url, &acct))
  {
    mutt_error ("%s is an invalid POP path", PopHost);
    return;
  }

  pop_data = safe_calloc (1, sizeof (POP_DATA));
  pop_data->conn = mutt_conn_find (NULL, &acct);

  if (pop_open_connection (pop_data) < 0)
    return;

  pop_data->conn->data = pop_data;

  mutt_message _("Checking for new messages...");

  /* find out how many messages are in the mailbox. */
  strfcpy (buffer, "STAT\r\n", sizeof (buffer));
  ret = pop_query (pop_data, buffer, sizeof (buffer));
  if (ret == -1)
    goto fail;
  if (ret == -2)
  {
    mutt_error ("%s", pop_data->err_msg);
    goto finish;
  }

  sscanf (buffer, "+OK %d %d", &msgs, &bytes);

  /* only get unread messages */
  if (msgs > 0 && option (OPTPOPLAST))
  {
    strfcpy (buffer, "LAST\r\n", sizeof (buffer));
    ret = pop_query (pop_data, buffer, sizeof (buffer));
    if (ret == -1)
      goto fail;
    if (ret == 0)
      sscanf (buffer, "+OK %d", &last);
  }

  if (msgs <= last)
  {
    mutt_message _("No new mail in POP mailbox.");
    goto finish;
  }

  if (mx_open_mailbox (NONULL (Spoolfile), M_APPEND, &ctx) == NULL)
    goto finish;

  delanswer = query_quadoption (OPT_POPDELETE, _("Delete messages from server?"));

  snprintf (msgbuf, sizeof (msgbuf), _("Reading new messages (%d bytes)..."), bytes);
  mutt_message ("%s", msgbuf);

  for (i = last + 1 ; i <= msgs ; i++)
  {
    if ((msg = mx_open_new_message (&ctx, NULL, M_ADD_FROM)) == NULL)
      ret = -3;
    else
    {
      snprintf (buffer, sizeof (buffer), "RETR %d\r\n", i);
      ret = pop_fetch_data (pop_data, buffer, NULL, fetch_message, msg->fp);
      if (ret == -3)
	rset = 1;

      if (ret == 0 && mx_commit_message (msg, &ctx) != 0)
      {
	rset = 1;
	ret = -3;
      }

      mx_close_message (&msg);
    }

    if (ret == 0 && delanswer == M_YES)
    {
      /* delete the message on the server */
      snprintf (buffer, sizeof (buffer), "DELE %d\r\n", i);
      ret = pop_query (pop_data, buffer, sizeof (buffer));
    }

    if (ret == -1)
    {
      mx_close_mailbox (&ctx, NULL);
      goto fail;
    }
    if (ret == -2)
    {
      mutt_error ("%s", pop_data->err_msg);
      break;
    }
    if (ret == -3)
    {
      mutt_error _("Error while writing mailbox!");
      break;
    }

    mutt_message (_("%s [%d of %d messages read]"), msgbuf, i - last, msgs - last);
  }

  mx_close_mailbox (&ctx, NULL);

  if (rset)
  {
    /* make sure no messages get deleted */
    strfcpy (buffer, "RSET\r\n", sizeof (buffer));
    if (pop_query (pop_data, buffer, sizeof (buffer)) == -1)
      goto fail;
  }

finish:
  /* exit gracefully */
  strfcpy (buffer, "QUIT\r\n", sizeof (buffer));
  if (pop_query (pop_data, buffer, sizeof (buffer)) == -1)
    goto fail;
  mutt_socket_close (pop_data->conn);
  FREE (&pop_data);
  return;

fail:
  mutt_error _("Server closed connection!");
  mutt_socket_close (pop_data->conn);
  FREE (&pop_data);
}
