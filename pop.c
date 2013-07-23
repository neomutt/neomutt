/*
 * Copyright (C) 2000-2002 Vsevolod Volkov <vvv@mutt.org.ua>
 * Copyright (C) 2006-7 Rocco Rutte <pdmef@gmx.net>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mx.h"
#include "pop.h"
#include "mutt_crypt.h"
#include "bcache.h"
#if USE_HCACHE
#include "hcache.h"
#endif

#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef USE_HCACHE
#define HC_FNAME	"mutt"		/* filename for hcache as POP lacks paths */
#define HC_FEXT		"hcache"	/* extension for hcache as POP lacks paths */
#endif

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
 * -1 - connection lost,
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

  mutt_mktemp (tempfile, sizeof (tempfile));
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

  safe_fclose (&f);
  unlink (tempfile);
  return ret;
}

/* parse UIDL */
static int fetch_uidl (char *line, void *data)
{
  int i, index;
  CONTEXT *ctx = (CONTEXT *)data;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;
  char *endp;

  errno = 0;
  index = strtol(line, &endp, 10);
  if (errno)
      return -1;
  while (*endp == ' ')
      endp++;
  memmove(line, endp, strlen(endp) + 1);

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

static int msg_cache_check (const char *id, body_cache_t *bcache, void *data)
{
  CONTEXT *ctx;
  POP_DATA *pop_data;
  int i;

  if (!(ctx = (CONTEXT *)data))
    return -1;
  if (!(pop_data = (POP_DATA *)ctx->data))
    return -1;

#ifdef USE_HCACHE
  /* keep hcache file if hcache == bcache */
  if (strcmp (HC_FNAME "." HC_FEXT, id) == 0)
    return 0;
#endif

  for (i = 0; i < ctx->msgcount; i++)
    /* if the id we get is known for a header: done (i.e. keep in cache) */
    if (ctx->hdrs[i]->data && mutt_strcmp (ctx->hdrs[i]->data, id) == 0)
      return 0;

  /* message not found in context -> remove it from cache
   * return the result of bcache, so we stop upon its first error
   */
  return mutt_bcache_del (bcache, id);
}

#ifdef USE_HCACHE
static int pop_hcache_namer (const char *path, char *dest, size_t destlen)
{
  return snprintf (dest, destlen, "%s." HC_FEXT, path);
}

static header_cache_t *pop_hcache_open (POP_DATA *pop_data, const char *path)
{
  ciss_url_t url;
  char p[LONG_STRING];

  if (!pop_data || !pop_data->conn)
    return mutt_hcache_open (HeaderCache, path, NULL);

  mutt_account_tourl (&pop_data->conn->account, &url);
  url.path = HC_FNAME;
  url_ciss_tostring (&url, p, sizeof (p), U_PATH);
  return mutt_hcache_open (HeaderCache, p, pop_hcache_namer);
}
#endif

/*
 * Read headers
 * returns:
 *  0 on success
 * -1 - connection lost,
 * -2 - invalid command or execution error,
 * -3 - error writing to tempfile
 */
static int pop_fetch_headers (CONTEXT *ctx)
{
  int i, ret, old_count, new_count, deleted;
  unsigned short hcached = 0, bcached;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;
  progress_t progress;

#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
  void *data;

  hc = pop_hcache_open (pop_data, ctx->path);
#endif

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

  if (!ctx->quiet)
    mutt_progress_init (&progress, _("Fetching message headers..."),
                        M_PROGRESS_MSG, ReadInc, new_count - old_count);

  if (ret == 0)
  {
    for (i = 0, deleted = 0; i < old_count; i++)
    {
      if (ctx->hdrs[i]->refno == -1)
      {
	ctx->hdrs[i]->deleted = 1;
	deleted++;
      }
    }
    if (deleted > 0)
    {
      mutt_error (_("%d messages have been lost. Try reopening the mailbox."),
		  deleted);
      mutt_sleep (2);
    }

    for (i = old_count; i < new_count; i++)
    {
      if (!ctx->quiet)
	mutt_progress_update (&progress, i + 1 - old_count, -1);
#if USE_HCACHE
      if ((data = mutt_hcache_fetch (hc, ctx->hdrs[i]->data, strlen)))
      {
	char *uidl = safe_strdup (ctx->hdrs[i]->data);
	int refno = ctx->hdrs[i]->refno;
	int index = ctx->hdrs[i]->index;
	/*
	 * - POP dynamically numbers headers and relies on h->refno
	 *   to map messages; so restore header and overwrite restored
	 *   refno with current refno, same for index
	 * - h->data needs to a separate pointer as it's driver-specific
	 *   data freed separately elsewhere
	 *   (the old h->data should point inside a malloc'd block from
	 *   hcache so there shouldn't be a memleak here)
	 */
	HEADER *h = mutt_hcache_restore ((unsigned char *) data, NULL);
	mutt_free_header (&ctx->hdrs[i]);
	ctx->hdrs[i] = h;
	ctx->hdrs[i]->refno = refno;
	ctx->hdrs[i]->index = index;
	ctx->hdrs[i]->data = uidl;
	ret = 0;
	hcached = 1;
      }
      else
#endif
      if ((ret = pop_read_header (pop_data, ctx->hdrs[i])) < 0)
	break;
#if USE_HCACHE
      else
      {
	mutt_hcache_store (hc, ctx->hdrs[i]->data, ctx->hdrs[i], 0, strlen, M_GENERATE_UIDVALIDITY);
      }

      FREE(&data);
#endif

      /*
       * faked support for flags works like this:
       * - if 'hcached' is 1, we have the message in our hcache:
       *        - if we also have a body: read
       *        - if we don't have a body: old
       *          (if $mark_old is set which is maybe wrong as
       *          $mark_old should be considered for syncing the
       *          folder and not when opening it XXX)
       * - if 'hcached' is 0, we don't have the message in our hcache:
       *        - if we also have a body: read
       *        - if we don't have a body: new
       */
      bcached = mutt_bcache_exists (pop_data->bcache, ctx->hdrs[i]->data) == 0;
      ctx->hdrs[i]->old = 0;
      ctx->hdrs[i]->read = 0;
      if (hcached)
      {
        if (bcached)
          ctx->hdrs[i]->read = 1;
        else if (option (OPTMARKOLD))
          ctx->hdrs[i]->old = 1;
      }
      else
      {
        if (bcached)
          ctx->hdrs[i]->read = 1;
      }

      ctx->msgcount++;
    }

    if (i > old_count)
      mx_update_context (ctx, i - old_count);
  }

#if USE_HCACHE
    mutt_hcache_close (hc);
#endif

  if (ret < 0)
  {
    for (i = ctx->msgcount; i < new_count; i++)
      mutt_free_header (&ctx->hdrs[i]);
    return ret;
  }

  /* after putting the result into our structures,
   * clean up cache, i.e. wipe messages deleted outside
   * the availability of our cache
   */
  if (option (OPTMESSAGECACHECLEAN))
    mutt_bcache_list (pop_data->bcache, msg_cache_check, (void*)ctx);

  mutt_clear_error ();
  return (new_count - old_count);
}

/* open POP mailbox - fetch only headers */
int pop_open_mailbox (CONTEXT *ctx)
{
  int ret;
  char buf[LONG_STRING];
  CONNECTION *conn;
  ACCOUNT acct;
  POP_DATA *pop_data;
  ciss_url_t url;

  if (pop_parse_path (ctx->path, &acct))
  {
    mutt_error (_("%s is an invalid POP path"), ctx->path);
    mutt_sleep (2);
    return -1;
  }

  mutt_account_tourl (&acct, &url);
  url.path = NULL;
  url_ciss_tostring (&url, buf, sizeof (buf), 0);
  conn = mutt_conn_find (NULL, &acct);
  if (!conn)
    return -1;

  FREE (&ctx->path);
  ctx->path = safe_strdup (buf);

  pop_data = safe_calloc (1, sizeof (POP_DATA));
  pop_data->conn = conn;
  ctx->data = pop_data;
  ctx->mx_close = pop_close_mailbox;

  if (pop_open_connection (pop_data) < 0)
    return -1;

  conn->data = pop_data;
  pop_data->bcache = mutt_bcache_open (&acct, NULL);

  /* init (hard-coded) ACL rights */
  memset (ctx->rights, 0, sizeof (ctx->rights));
  mutt_bit_set (ctx->rights, M_ACL_SEEN);
  mutt_bit_set (ctx->rights, M_ACL_DELETE);
#if USE_HCACHE
  /* flags are managed using header cache, so it only makes sense to
   * enable them in that case */
  mutt_bit_set (ctx->rights, M_ACL_WRITE);
#endif

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
      FREE (&pop_data->cache[i].path);
    }
  }
}

/* close POP mailbox */
int pop_close_mailbox (CONTEXT *ctx)
{
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  if (!pop_data)
    return 0;

  pop_logout (ctx);

  if (pop_data->status != POP_NONE)
    mutt_socket_close (pop_data->conn);

  pop_data->status = POP_NONE;

  pop_data->clear_cache = 1;
  pop_clear_cache (pop_data);

  if (!pop_data->conn->data)
    mutt_socket_free (pop_data->conn);

  mutt_bcache_close (&pop_data->bcache);

  return 0;
}

/* fetch message from POP server */
int pop_fetch_message (MESSAGE* msg, CONTEXT* ctx, int msgno)
{
  int ret;
  void *uidl;
  char buf[LONG_STRING];
  char path[_POSIX_PATH_MAX];
  progress_t progressbar;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;
  POP_CACHE *cache;
  HEADER *h = ctx->hdrs[msgno];
  unsigned short bcache = 1;

  /* see if we already have the message in body cache */
  if ((msg->fp = mutt_bcache_get (pop_data->bcache, h->data)))
    return 0;

  /*
   * see if we already have the message in our cache in
   * case $message_cachedir is unset
   */
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

    mutt_progress_init (&progressbar, _("Fetching message..."),
			M_PROGRESS_SIZE, NetInc, h->content->length + h->content->offset - 1);

    /* see if we can put in body cache; use our cache as fallback */
    if (!(msg->fp = mutt_bcache_put (pop_data->bcache, h->data, 1)))
    {
      /* no */
      bcache = 0;
      mutt_mktemp (path, sizeof (path));
      if (!(msg->fp = safe_fopen (path, "w+")))
      {
	mutt_perror (path);
	mutt_sleep (2);
	return -1;
      }
    }

    snprintf (buf, sizeof (buf), "RETR %d\r\n", h->refno);

    ret = pop_fetch_data (pop_data, buf, &progressbar, fetch_message, msg->fp);
    if (ret == 0)
      break;

    safe_fclose (&msg->fp);

    /* if RETR failed (e.g. connection closed), be sure to remove either
     * the file in bcache or from POP's own cache since the next iteration
     * of the loop will re-attempt to put() the message */
    if (!bcache)
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
  if (bcache)
    mutt_bcache_commit (pop_data->bcache, h->data);
  else
  {
    cache->index = h->index;
    cache->path = safe_strdup (path);
  }
  rewind (msg->fp);
  uidl = h->data;

  /* we replace envelop, key in subj_hash has to be updated as well */
  if (ctx->subj_hash && h->env->real_subj)
    hash_delete (ctx->subj_hash, h->env->real_subj, h, NULL);
  mutt_free_envelope (&h->env);
  h->env = mutt_read_rfc822_header (msg->fp, h, 0, 0);
  if (ctx->subj_hash && h->env->real_subj)
    hash_insert (ctx->subj_hash, h->env->real_subj, h, 1);

  h->data = uidl;
  h->lines = 0;
  fgets (buf, sizeof (buf), msg->fp);
  while (!feof (msg->fp))
  {
    ctx->hdrs[msgno]->lines++;
    fgets (buf, sizeof (buf), msg->fp);
  }

  h->content->length = ftello (msg->fp) - h->content->offset;

  /* This needs to be done in case this is a multipart message */
  if (!WithCrypto)
    h->security = crypt_query (h->content);

  mutt_clear_error();
  rewind (msg->fp);

  return 0;
}

/* update POP mailbox - delete messages from server */
int pop_sync_mailbox (CONTEXT *ctx, int *index_hint)
{
  int i, j, ret = 0;
  char buf[LONG_STRING];
  POP_DATA *pop_data = (POP_DATA *)ctx->data;
  progress_t progress;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
#endif

  pop_data->check_time = 0;

  FOREVER
  {
    if (pop_reconnect (ctx) < 0)
      return -1;

    mutt_progress_init (&progress, _("Marking messages deleted..."),
			M_PROGRESS_MSG, WriteInc, ctx->deleted);

#if USE_HCACHE
    hc = pop_hcache_open (pop_data, ctx->path);
#endif

    for (i = 0, j = 0, ret = 0; ret == 0 && i < ctx->msgcount; i++)
    {
      if (ctx->hdrs[i]->deleted && ctx->hdrs[i]->refno != -1)
      {
	j++;
	if (!ctx->quiet)
	  mutt_progress_update (&progress, j, -1);
	snprintf (buf, sizeof (buf), "DELE %d\r\n", ctx->hdrs[i]->refno);
	if ((ret = pop_query (pop_data, buf, sizeof (buf))) == 0)
	{
	  mutt_bcache_del (pop_data->bcache, ctx->hdrs[i]->data);
#if USE_HCACHE
	  mutt_hcache_delete (hc, ctx->hdrs[i]->data, strlen);
#endif
	}
      }

#if USE_HCACHE
      if (ctx->hdrs[i]->changed)
      {
	mutt_hcache_store (hc, ctx->hdrs[i]->data, ctx->hdrs[i], 0, strlen, M_GENERATE_UIDVALIDITY);
      }
#endif

    }

#if USE_HCACHE
    mutt_hcache_close (hc);
#endif

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
  CONNECTION *conn;
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

  ret = pop_parse_path (url, &acct);
  FREE (&url);
  if (ret)
  {
    mutt_error (_("%s is an invalid POP path"), PopHost);
    return;
  }

  conn = mutt_conn_find (NULL, &acct);
  if (!conn)
    return;

  pop_data = safe_calloc (1, sizeof (POP_DATA));
  pop_data->conn = conn;

  if (pop_open_connection (pop_data) < 0)
  {
    mutt_socket_free (pop_data->conn);
    FREE (&pop_data);
    return;
  }

  conn->data = pop_data;

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
  mutt_socket_close (conn);
  FREE (&pop_data);
  return;

fail:
  mutt_error _("Server closed connection!");
  mutt_socket_close (conn);
  FREE (&pop_data);
}
