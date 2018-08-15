/**
 * @file
 * Manage IMAP messages
 *
 * @authors
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009 Brendan Cully <brendan@kublai.com>
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
 * @page imap_message Manage IMAP messages
 *
 * Manage IMAP messages
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/email.h"
#include "conn/conn.h"
#include "mutt.h"
#include "message.h"
#include "bcache.h"
#include "context.h"
#include "curs_lib.h"
#include "globals.h"
#include "imap/imap.h"
#include "mutt_account.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "muttlib.h"
#include "mx.h"
#include "progress.h"
#include "protos.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

struct BodyCache;

/* These Config Variables are only used in imap/message.c */
char *ImapHeaders; ///< Config: (imap) Additional email headers to download when getting index

/**
 * new_header_data - Create a new ImapHeaderData
 * @retval ptr New ImapHeaderData
 */
static struct ImapHeaderData *new_header_data(void)
{
  struct ImapHeaderData *d = mutt_mem_calloc(1, sizeof(struct ImapHeaderData));
  return d;
}

/**
 * msg_cache_open - Open a message cache
 * @param idata Server data
 * @retval ptr  Success, using existing cache
 * @retval ptr  Success, opened new cache
 * @retval NULL Failure
 */
static struct BodyCache *msg_cache_open(struct ImapData *idata)
{
  char mailbox[PATH_MAX];

  if (idata->bcache)
    return idata->bcache;

  imap_cachepath(idata, idata->mailbox, mailbox, sizeof(mailbox));

  return mutt_bcache_open(&idata->conn->account, mailbox);
}

/**
 * msg_cache_get - Get the message cache entry for an email
 * @param idata Server data
 * @param h     Email header
 * @retval ptr  Success, handle of cache entry
 * @retval NULL Failure
 */
static FILE *msg_cache_get(struct ImapData *idata, struct Header *h)
{
  if (!idata || !h)
    return NULL;

  idata->bcache = msg_cache_open(idata);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);
  return mutt_bcache_get(idata->bcache, id);
}

/**
 * msg_cache_put - Put an email into the message cache
 * @param idata Server data
 * @param h     Email header
 * @retval ptr  Success, handle of cache entry
 * @retval NULL Failure
 */
static FILE *msg_cache_put(struct ImapData *idata, struct Header *h)
{
  if (!idata || !h)
    return NULL;

  idata->bcache = msg_cache_open(idata);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);
  return mutt_bcache_put(idata->bcache, id);
}

/**
 * msg_cache_commit - Add to the message cache
 * @param idata Server data
 * @param h     Email header
 * @retval  0 Success
 * @retval -1 Failure
 */
static int msg_cache_commit(struct ImapData *idata, struct Header *h)
{
  if (!idata || !h)
    return -1;

  idata->bcache = msg_cache_open(idata);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);

  return mutt_bcache_commit(idata->bcache, id);
}

/**
 * msg_cache_clean_cb - Delete an entry from the message cache - Implements ::bcache_list_t
 * @retval 0 Always
 */
static int msg_cache_clean_cb(const char *id, struct BodyCache *bcache, void *data)
{
  unsigned int uv, uid;
  struct ImapData *idata = data;

  if (sscanf(id, "%u-%u", &uv, &uid) != 2)
    return 0;

  /* bad UID */
  if (uv != idata->uid_validity || !mutt_hash_int_find(idata->uid_hash, uid))
    mutt_bcache_del(bcache, id);

  return 0;
}

/**
 * msg_parse_flags - read a FLAGS token into an ImapHeader
 * @param h Header to store flags
 * @param s Command string containing flags
 * @retval ptr  The end of flags string
 * @retval NULL Failure
 */
static char *msg_parse_flags(struct ImapHeader *h, char *s)
{
  struct ImapHeaderData *hd = h->data;

  /* sanity-check string */
  if (mutt_str_strncasecmp("FLAGS", s, 5) != 0)
  {
    mutt_debug(1, "not a FLAGS response: %s\n", s);
    return NULL;
  }
  s += 5;
  SKIPWS(s);
  if (*s != '(')
  {
    mutt_debug(1, "bogus FLAGS response: %s\n", s);
    return NULL;
  }
  s++;

  FREE(&hd->flags_system);
  FREE(&hd->flags_remote);

  hd->deleted = false;
  hd->flagged = false;
  hd->replied = false;
  hd->read = false;
  hd->old = false;

  /* start parsing */
  while (*s && *s != ')')
  {
    if (mutt_str_strncasecmp("\\deleted", s, 8) == 0)
    {
      s += 8;
      hd->deleted = true;
    }
    else if (mutt_str_strncasecmp("\\flagged", s, 8) == 0)
    {
      s += 8;
      hd->flagged = true;
    }
    else if (mutt_str_strncasecmp("\\answered", s, 9) == 0)
    {
      s += 9;
      hd->replied = true;
    }
    else if (mutt_str_strncasecmp("\\seen", s, 5) == 0)
    {
      s += 5;
      hd->read = true;
    }
    else if (mutt_str_strncasecmp("\\recent", s, 7) == 0)
      s += 7;
    else if (mutt_str_strncasecmp("old", s, 3) == 0)
    {
      s += 3;
      hd->old = MarkOld ? true : false;
    }
    else
    {
      char ctmp;
      char *flag_word = s;
      bool is_system_keyword = (mutt_str_strncasecmp("\\", s, 1) == 0);

      while (*s && !ISSPACE(*s) && *s != ')')
        s++;

      ctmp = *s;
      *s = '\0';

      /* store other system flags as well (mainly \\Draft) */
      if (is_system_keyword)
        mutt_str_append_item(&hd->flags_system, flag_word, ' ');
      /* store custom flags as well */
      else
        mutt_str_append_item(&hd->flags_remote, flag_word, ' ');

      *s = ctmp;
    }
    SKIPWS(s);
  }

  /* wrap up, or note bad flags response */
  if (*s == ')')
    s++;
  else
  {
    mutt_debug(1, "Unterminated FLAGS response: %s\n", s);
    return NULL;
  }

  return s;
}

/**
 * msg_parse_fetch - handle headers returned from header fetch
 * @param h IMAP Header
 * @param s Command string
 * @retval  0 Success
 * @retval -1 String is corrupted
 * @retval -2 Fetch contains a body or header lines that still need to be parsed
 */
static int msg_parse_fetch(struct ImapHeader *h, char *s)
{
  char tmp[SHORT_STRING];
  char *ptmp = NULL;

  if (!s)
    return -1;

  while (*s)
  {
    SKIPWS(s);

    if (mutt_str_strncasecmp("FLAGS", s, 5) == 0)
    {
      s = msg_parse_flags(h, s);
      if (!s)
        return -1;
    }
    else if (mutt_str_strncasecmp("UID", s, 3) == 0)
    {
      s += 3;
      SKIPWS(s);
      if (mutt_str_atoui(s, &h->data->uid) < 0)
        return -1;

      s = imap_next_word(s);
    }
    else if (mutt_str_strncasecmp("INTERNALDATE", s, 12) == 0)
    {
      s += 12;
      SKIPWS(s);
      if (*s != '\"')
      {
        mutt_debug(1, "bogus INTERNALDATE entry: %s\n", s);
        return -1;
      }
      s++;
      ptmp = tmp;
      while (*s && (*s != '\"') && (ptmp != (tmp + sizeof(tmp) - 1)))
        *ptmp++ = *s++;
      if (*s != '\"')
        return -1;
      s++; /* skip past the trailing " */
      *ptmp = '\0';
      h->received = mutt_date_parse_imap(tmp);
    }
    else if (mutt_str_strncasecmp("RFC822.SIZE", s, 11) == 0)
    {
      s += 11;
      SKIPWS(s);
      ptmp = tmp;
      while (isdigit((unsigned char) *s) && (ptmp != (tmp + sizeof(tmp) - 1)))
        *ptmp++ = *s++;
      *ptmp = '\0';
      if (mutt_str_atol(tmp, &h->content_length) < 0)
        return -1;
    }
    else if ((mutt_str_strncasecmp("BODY", s, 4) == 0) ||
             (mutt_str_strncasecmp("RFC822.struct Header", s, 13) == 0))
    {
      /* handle above, in msg_fetch_header */
      return -2;
    }
    else if (mutt_str_strncasecmp("MODSEQ", s, 6) == 0)
    {
      s += 6;
      SKIPWS(s);
      if (*s != '(')
      {
        mutt_debug(1, "msg_parse_flags: bogus MODSEQ response: %s\n", s);
        return -1;
      }
      s++;
      while (*s && *s != ')')
        s++;
      if (*s == ')')
        s++;
      else
      {
        mutt_debug(1, "msg_parse_flags: Unterminated MODSEQ response: %s\n", s);
        return -1;
      }
    }
    else if (*s == ')')
      s++; /* end of request */
    else if (*s)
    {
      /* got something i don't understand */
      imap_error("msg_parse_fetch", s);
      return -1;
    }
  }

  return 0;
}

/**
 * msg_fetch_header - import IMAP FETCH response into an ImapHeader
 * @param ctx Context
 * @param h   ImapHeader
 * @param buf Server string containing FETCH response
 * @param fp  Connection to server
 * @retval  0 Success
 * @retval -1 String is not a fetch response
 * @retval -2 String is a corrupt fetch response
 *
 * Expects string beginning with * n FETCH.
 */
static int msg_fetch_header(struct Context *ctx, struct ImapHeader *h, char *buf, FILE *fp)
{
  unsigned int bytes;
  int rc = -1; /* default now is that string isn't FETCH response */
  int parse_rc;

  struct ImapData *idata = ctx->data;

  if (buf[0] != '*')
    return rc;

  /* skip to message number */
  buf = imap_next_word(buf);
  if (mutt_str_atoui(buf, &h->data->msn) < 0)
    return rc;

  /* find FETCH tag */
  buf = imap_next_word(buf);
  if (mutt_str_strncasecmp("FETCH", buf, 5) != 0)
    return rc;

  rc = -2; /* we've got a FETCH response, for better or worse */
  buf = strchr(buf, '(');
  if (!buf)
    return rc;
  buf++;

  /* FIXME: current implementation - call msg_parse_fetch - if it returns -2,
   *   read header lines and call it again. Silly. */
  parse_rc = msg_parse_fetch(h, buf);
  if (!parse_rc)
    return 0;
  if (parse_rc != -2 || !fp)
    return rc;

  if (imap_get_literal_count(buf, &bytes) == 0)
  {
    imap_read_literal(fp, idata, bytes, NULL);

    /* we may have other fields of the FETCH _after_ the literal
     * (eg Domino puts FLAGS here). Nothing wrong with that, either.
     * This all has to go - we should accept literals and nonliterals
     * interchangeably at any time. */
    if (imap_cmd_step(idata) != IMAP_CMD_CONTINUE)
      return rc;

    if (msg_parse_fetch(h, idata->buf) == -1)
      return rc;
  }

  rc = 0; /* success */

  /* subtract headers from message size - unfortunately only the subset of
   * headers we've requested. */
  h->content_length -= bytes;

  return rc;
}

/**
 * flush_buffer - Write data to a connection
 * @param buf  Buffer containing data
 * @param len  Length of buffer
 * @param conn Network connection
 */
static void flush_buffer(char *buf, size_t *len, struct Connection *conn)
{
  buf[*len] = '\0';
  mutt_socket_write_n(conn, buf, *len);
  *len = 0;
}

/* If the user hits ctrl-c during an initial header download for a mailbox,
 * prompt whether to completely abort the download and close the mailbox.
 */
static int query_abort_header_download(struct ImapData *idata)
{
  int abort = 0;

  mutt_flushinp();
  /* L10N: This prompt is made if the user hits Ctrl-C when opening
   * an IMAP mailbox */
  if (mutt_yesorno(_("Abort download and close mailbox?"), MUTT_YES) == MUTT_YES)
  {
    abort = 1;
    imap_close_connection(idata);
  }
  SigInt = 0;

  return abort;
}

/**
 * alloc_msn_index - Create lookup table of MSN to Header
 * @param idata     Server data
 * @param msn_count Number of MSNs in use
 *
 * Mapping from Message Sequence Number to Header
 */
static void alloc_msn_index(struct ImapData *idata, size_t msn_count)
{
  size_t new_size;

  if (msn_count <= idata->msn_index_size)
    return;

  /* This is a conservative check to protect against a malicious imap
   * server.  Most likely size_t is bigger than an unsigned int, but
   * if msn_count is this big, we have a serious problem. */
  if (msn_count >= (UINT_MAX / sizeof(struct Header *)))
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  /* Add a little padding, like mx_allloc_memory() */
  new_size = msn_count + 25;

  if (!idata->msn_index)
    idata->msn_index = mutt_mem_calloc(new_size, sizeof(struct Header *));
  else
  {
    mutt_mem_realloc(&idata->msn_index, sizeof(struct Header *) * new_size);
    memset(idata->msn_index + idata->msn_index_size, 0,
           sizeof(struct Header *) * (new_size - idata->msn_index_size));
  }

  idata->msn_index_size = new_size;
}

/* This function is run after imap_alloc_msn_index, so we skip the
 * malicious msn_count size check.
 */
static void imap_alloc_uid_hash(struct ImapData *idata, unsigned int msn_count)
{
  if (!idata->uid_hash)
    idata->uid_hash = mutt_hash_int_create(MAX(6 * msn_count / 5, 30), 0);
}

/**
 * generate_seqset - Generate a sequence set
 * @param b         Buffer for the result
 * @param idata     Server data
 * @param msn_begin First Message Sequence number
 * @param msn_end   Last Message Sequence number
 *
 * Generates a more complicated sequence set after using the header cache,
 * in case there are missing MSNs in the middle.
 *
 * There is a suggested limit of 1000 bytes for an IMAP client request.
 * Ideally, we would generate multiple requests if the number of ranges
 * is too big, but for now just abort to using the whole range.
 */
static void imap_fetch_msn_seqset(struct Buffer *b, struct ImapData *idata,
                                  unsigned int msn_begin, unsigned int msn_end)
{
  int chunks = 0;
  int state = 0; /* 1: single msn, 2: range of msn */
  unsigned int range_begin, range_end;

  for (unsigned int msn = msn_begin; msn <= msn_end + 1; msn++)
  {
    if (msn <= msn_end && !idata->msn_index[msn - 1])
    {
      switch (state)
      {
        case 1: /* single: convert to a range */
          state = 2;
        /* fallthrough */
        case 2: /* extend range ending */
          range_end = msn;
          break;
        default:
          state = 1;
          range_begin = msn;
          break;
      }
    }
    else if (state)
    {
      if (chunks++)
        mutt_buffer_addch(b, ',');
      if (chunks == 150)
        break;

      if (state == 1)
        mutt_buffer_printf(b, "%u", range_begin);
      else if (state == 2)
        mutt_buffer_printf(b, "%u:%u", range_begin, range_end);
      state = 0;
    }
  }

  /* Too big.  Just query the whole range then. */
  if (chunks == 150 || mutt_str_strlen(b->data) > 500)
  {
    b->dptr = b->data;
    mutt_buffer_printf(b, "%u:%u", msn_begin, msn_end);
  }
}

/**
 * set_changed_flag - Have the flags of an email changed
 * @param[in]  ctx            Context
 * @param[in]  h              Email Header
 * @param[in]  local_changes  Has the local mailbox been changed?
 * @param[out] server_changes Set to 1 if the flag has changed
 * @param[in]  flag_name      Flag to check, e.g. #MUTT_FLAG
 * @param[in]  old_hd_flag    Old header flags
 * @param[in]  new_hd_flag    New header flags
 * @param[in]  h_flag         Email's value for flag_name
 *
 * Sets server_changes to 1 if a change to a flag is made, or in the
 * case of local_changes, if a change to a flag _would_ have been
 * made.
 */
static void set_changed_flag(struct Context *ctx, struct Header *h,
                             int local_changes, int *server_changes, int flag_name,
                             int old_hd_flag, int new_hd_flag, int h_flag)
{
  /* If there are local_changes, we only want to note if the server
   * flags have changed, so we can set a reopen flag in
   * cmd_parse_fetch().  We don't want to count a local modification
   * to the header flag as a "change".
   */
  if ((old_hd_flag == new_hd_flag) && local_changes)
    return;

  if (new_hd_flag == h_flag)
    return;

  if (server_changes)
    *server_changes = 1;

  /* Local changes have priority */
  if (!local_changes)
    mutt_set_flag(ctx, h, flag_name, new_hd_flag);
}

#if USE_HCACHE
/* Retrieve data from the header cache.
 *
 * Without CONDSTORE or QRESYNC, we need to query all the current
 * UIDs and update their flag state and current MSN.
 *
 * For CONDSTORE, we still need to grab the existing UIDs and
 * their MSN.  The current flag state will be queried in
 * read_headers_condstore_qresync_updates().
 */
static int read_headers_normal_eval_cache(struct ImapData *idata, unsigned int msn_end,
                                          unsigned int uidnext,
                                          int store_flag_updates, int eval_condstore)
{
  struct Context *ctx;
  int idx, msgno, rc, mfhrc = 0;
  struct Progress progress;
  struct ImapHeader h;
  char buf[LONG_STRING];

  ctx = idata->ctx;
  idx = ctx->msgcount;

  /* L10N:
     Comparing the cached data with the IMAP server's data */
  mutt_progress_init(&progress, _("Evaluating cache..."), MUTT_PROGRESS_MSG, ReadInc, msn_end);

  /* If we are using CONDSTORE's "FETCH CHANGEDSINCE", then we keep
   * the flags in the header cache, and update them further below.
   * Otherwise, we fetch the current state of the flags here. */
  snprintf(buf, sizeof(buf), "UID FETCH 1:%u (UID%s)", uidnext - 1,
           eval_condstore ? "" : " FLAGS");

  imap_cmd_start(idata, buf);

  rc = IMAP_CMD_CONTINUE;
  for (msgno = 1; rc == IMAP_CMD_CONTINUE; msgno++)
  {
    if (SigInt && query_abort_header_download(idata))
      return -1;

    mutt_progress_update(&progress, msgno, -1);

    memset(&h, 0, sizeof(h));
    h.data = mutt_mem_calloc(1, sizeof(struct ImapHeaderData));
    do
    {
      rc = imap_cmd_step(idata);
      if (rc != IMAP_CMD_CONTINUE)
        break;

      if ((mfhrc = msg_fetch_header(ctx, &h, idata->buf, NULL)) < 0)
        continue;

      if (!h.data->uid)
      {
        mutt_debug(2, "imap_read_headers: skipping hcache FETCH "
            "response for message number %d missing a UID\n",
            h.data->msn);
        continue;
      }

      if (h.data->msn < 1 || h.data->msn > msn_end)
      {
        mutt_debug(1, "imap_read_headers: skipping hcache FETCH "
            "response for unknown message number %d\n",
            h.data->msn);
        continue;
      }

      if (idata->msn_index[h.data->msn - 1])
      {
        mutt_debug(2, "imap_read_headers: skipping hcache FETCH "
            "for duplicate message %d\n",
            h.data->msn);
        continue;
      }

      ctx->hdrs[idx] = imap_hcache_get(idata, h.data->uid);
      if (ctx->hdrs[idx])
      {
        idata->max_msn = MAX(idata->max_msn, h.data->msn);
        idata->msn_index[h.data->msn - 1] = ctx->hdrs[idx];
        mutt_hash_int_insert(idata->uid_hash, h.data->uid, ctx->hdrs[idx]);

        ctx->hdrs[idx]->index = idx;
        /* messages which have not been expunged are ACTIVE (borrowed from mh
         * folders) */
        ctx->hdrs[idx]->active = 1;
        ctx->hdrs[idx]->changed = 0;
        if (!eval_condstore)
        {
          ctx->hdrs[idx]->read = h.data->read;
          ctx->hdrs[idx]->old = h.data->old;
          ctx->hdrs[idx]->deleted = h.data->deleted;
          ctx->hdrs[idx]->flagged = h.data->flagged;
          ctx->hdrs[idx]->replied = h.data->replied;
        }
        else
        {
          h.data->read = ctx->hdrs[idx]->read;
          h.data->old = ctx->hdrs[idx]->old;
          h.data->deleted = ctx->hdrs[idx]->deleted;
          h.data->flagged = ctx->hdrs[idx]->flagged;
          h.data->replied = ctx->hdrs[idx]->replied;
        }

        /*  ctx->hdrs[msgno]->received is restored from mutt_hcache_restore */
        ctx->hdrs[idx]->data = (void *) (h.data);
        STAILQ_INIT(&ctx->hdrs[idx]->tags);
        driver_tags_replace(&ctx->hdrs[idx]->tags, mutt_str_strdup(h.data->flags_remote));

        ctx->msgcount++;
        ctx->size += ctx->hdrs[idx]->content->length;

        /* If this is the first time we are fetching, we need to
         * store the current state of flags back into the header cache */
        if (!eval_condstore && store_flag_updates)
          imap_hcache_put(idata, ctx->hdrs[idx]);

        h.data = NULL;
        idx++;
      }
    } while (mfhrc == -1);

    imap_free_header_data(&h.data);

    if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
      return -1;
  }

  return 0;
}

/* Retrieve data from the header cache.
 *
 * For QRESYNC, we grab the UIDs in order by MSN from the header
 * cache.
 *
 * In read_headers_condstore_qresync_updates().  We will update change
 * flags using CHANGEDSINCE and find out what UIDs have been expunged
 * using VANISHED.
 */
static int read_headers_qresync_eval_cache(struct ImapData *idata, char *uid_seqset)
{
  struct Context *ctx;
  int rc;
  unsigned int uid, msn;
  struct SeqsetIterator *iter;
  struct Header *h;
  struct ImapHeaderData *ihd;

  mutt_debug(2, "Reading uid seqset from header cache\n");
  ctx = idata->ctx;
  msn = 1;

  iter = mutt_seqset_iterator_new(uid_seqset);
  if (!iter)
    return -1;

  while ((rc = mutt_seqset_iterator_next(iter, &uid)) == 0)
  {
    /* The seqset may contain more headers than the fetch request, so
     * we need to watch and reallocate the context and msn_index */
    if (msn > idata->msn_index_size)
      alloc_msn_index(idata, msn);

    h = imap_hcache_get(idata, uid);
    if (h)
    {
      idata->max_msn = MAX(idata->max_msn, msn);
      idata->msn_index[msn - 1] = h;

      if (ctx->msgcount >= ctx->hdrmax)
        mx_alloc_memory(ctx);

      ihd = mutt_mem_calloc(1, sizeof(struct ImapHeaderData));
      h->data = ihd;

      h->index = ctx->msgcount;
      h->active = 1;
      h->changed = 0;
      ihd->read = h->read;
      ihd->old = h->old;
      ihd->deleted = h->deleted;
      ihd->flagged = h->flagged;
      ihd->replied = h->replied;

      ihd->msn = msn;
      ihd->uid = uid;
      mutt_hash_int_insert(idata->uid_hash, uid, h);

      ctx->size += h->content->length;
      ctx->hdrs[ctx->msgcount++] = h;

      msn++;
    }
  }

  mutt_seqset_iterator_free(&iter);

  return rc;
}

/*
 * Retrieve updates from the server.
 *
 * CONDSTORE and QRESYNC use FETCH extensions to grab updates.
 */
static int read_headers_condstore_qresync_updates(struct ImapData *idata, unsigned int msn_end,
                                                  unsigned int uidnext,
                                                  unsigned long long hc_modseq, int eval_qresync)
{
  struct Context *ctx;
  struct Progress progress;
  int msgno, rc;
  char buf[LONG_STRING];
  unsigned int header_msn;
  char *fetch_buf;

  ctx = idata->ctx;

  /* L10N:
     Fetching IMAP flag changes, using the CONDSTORE extension */
  mutt_progress_init(&progress, _("Fetching flag updates..."),
                     MUTT_PROGRESS_MSG, ReadInc, msn_end);

  snprintf(buf, sizeof(buf), "UID FETCH 1:%u (FLAGS) (CHANGEDSINCE %llu%s)",
           uidnext - 1, hc_modseq, eval_qresync ? " VANISHED" : "");

  imap_cmd_start(idata, buf);

  rc = IMAP_CMD_CONTINUE;
  for (msgno = 1; rc == IMAP_CMD_CONTINUE; msgno++)
  {
    if (SigInt && query_abort_header_download(idata))
      return -1;

    mutt_progress_update(&progress, msgno, -1);

    /* cmd_parse_fetch will update the flags */
    rc = imap_cmd_step(idata);
    if (rc != IMAP_CMD_CONTINUE)
      break;

    /* so we just need to grab the header and persist it back into
     * the header cache */
    fetch_buf = idata->buf;
    if (fetch_buf[0] != '*')
      continue;

    fetch_buf = imap_next_word(fetch_buf);
    if (!isdigit((unsigned char) *fetch_buf) || mutt_str_atoui(fetch_buf, &header_msn) < 0)
      continue;

    if (header_msn < 1 || header_msn > msn_end || !idata->msn_index[header_msn - 1])
    {
      mutt_debug(1, "imap_read_headers: skipping CONDSTORE flag "
          "update for unknown message number %u\n",
          header_msn);
      continue;
    }

    imap_hcache_put(idata, idata->msn_index[header_msn - 1]);
  }

  /* The IMAP flag setting as part of cmd_parse_fetch() ends up
   * flipping these on. */
  idata->check_status &= ~IMAP_FLAGS_PENDING;
  ctx->changed = 0;

  /* VANISHED handling: we need to empty out the messages */
  if (idata->reopen & IMAP_EXPUNGE_PENDING)
  {
    imap_hcache_close(idata);
    imap_expunge_mailbox(idata);
    idata->hcache = imap_hcache_open(idata, NULL);
    idata->reopen &= ~IMAP_EXPUNGE_PENDING;
  }

  return 0;
}
#endif /* USE_HCACHE */

/* Retrieve new messages from the server
 */
static int read_headers_fetch_new(struct ImapData *idata, unsigned int msn_begin,
                                  unsigned int msn_end, int evalhc,
                                  unsigned int *maxuid, int initial_download)
{
  struct Context *ctx;
  int idx, msgno, rc, mfhrc = 0, retval = -1;
  unsigned int fetch_msn_end = 0;
  struct Progress progress;
  char *hdrreq = NULL;
  char tempfile[_POSIX_PATH_MAX];
  FILE *fp = NULL;
  struct ImapHeader h;
  struct Buffer *b;
  static const char *const want_headers =
      "DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE "
      "CONTENT-DESCRIPTION IN-REPLY-TO REPLY-TO LINES LIST-POST X-LABEL "
      "X-ORIGINAL-TO";

  ctx = idata->ctx;
  idx = ctx->msgcount;

  if (mutt_bit_isset(idata->capabilities, IMAP4REV1))
  {
    safe_asprintf(&hdrreq, "BODY.PEEK[struct Header.FIELDS (%s%s%s)]",
                  want_headers, ImapHeaders ? " " : "", NONULL(ImapHeaders));
  }
  else if (mutt_bit_isset(idata->capabilities, IMAP4))
  {
    safe_asprintf(&hdrreq, "RFC822.struct Header.LINES (%s%s%s)", want_headers,
                  ImapHeaders ? " " : "", NONULL(ImapHeaders));
  }
  else
  { /* Unable to fetch headers for lower versions */
    mutt_error(_("Unable to fetch headers from this IMAP server version."));
    mutt_sleep(2); /* pause a moment to let the user see the error */
    goto bail;
  }

  /* instead of downloading all headers and then parsing them, we parse them
   * as they come in. */
  mutt_mktemp(tempfile, sizeof(tempfile));
  if (!(fp = mutt_file_fopen(tempfile, "w+")))
  {
    mutt_error(_("Could not create temporary file %s"), tempfile);
    mutt_sleep(2);
    goto bail;
  }
  unlink(tempfile);

  mutt_progress_init(&progress, _("Fetching message headers..."),
                     MUTT_PROGRESS_MSG, ReadInc, msn_end);

  while (msn_begin <= msn_end && fetch_msn_end < msn_end)
  {
    b = mutt_buffer_new();
    if (evalhc)
    {
      /* In case there are holes in the header cache. */
      evalhc = false;
      imap_fetch_msn_seqset(b, idata, msn_begin, msn_end);
    }
    else
      mutt_buffer_printf(b, "%u:%u", msn_begin, msn_end);

    fetch_msn_end = msn_end;
    char *cmd = NULL;
    safe_asprintf(&cmd, "FETCH %s (UID FLAGS INTERNALDATE RFC822.SIZE %s)", b->data, hdrreq);
    imap_cmd_start(idata, cmd);
    FREE(&cmd);
    mutt_buffer_free(&b);

    rc = IMAP_CMD_CONTINUE;
    for (msgno = msn_begin; rc == IMAP_CMD_CONTINUE; msgno++)
    {
      if (initial_download && SigInt && query_abort_header_download(idata))
        goto bail;

      mutt_progress_update(&progress, msgno, -1);

      rewind(fp);
      memset(&h, 0, sizeof(h));
      h.data = new_header_data();

      /* this DO loop does two things:
       * 1. handles untagged messages, so we can try again on the same msg
       * 2. fetches the tagged response at the end of the last message.
       */
      do
      {
        rc = imap_cmd_step(idata);
        if (rc != IMAP_CMD_CONTINUE)
          break;

        mfhrc = msg_fetch_header(ctx, &h, idata->buf, fp);
        if (mfhrc < 0)
          continue;

        if (!ftello(fp))
        {
          mutt_debug(
              2, "msg_fetch_header: ignoring fetch response with no body\n");
          continue;
        }

        /* make sure we don't get remnants from older larger message headers */
        fputs("\n\n", fp);

        if (h.data->msn < 1 || h.data->msn > fetch_msn_end)
        {
          mutt_debug(1, "skipping FETCH response for unknown message number %d\n",
                     h.data->msn);
          continue;
        }

        /* May receive FLAGS updates in a separate untagged response (#2935) */
        if (idata->msn_index[h.data->msn - 1])
        {
          mutt_debug(2, "skipping FETCH response for duplicate message %d\n",
                     h.data->msn);
          continue;
        }

        ctx->hdrs[idx] = mutt_header_new();

        idata->max_msn = MAX(idata->max_msn, h.data->msn);
        idata->msn_index[h.data->msn - 1] = ctx->hdrs[idx];
        mutt_hash_int_insert(idata->uid_hash, h.data->uid, ctx->hdrs[idx]);

        ctx->hdrs[idx]->index = idx;
        /* messages which have not been expunged are ACTIVE (borrowed from mh
         * folders) */
        ctx->hdrs[idx]->active = true;
        ctx->hdrs[idx]->changed = 0;
        ctx->hdrs[idx]->read = h.data->read;
        ctx->hdrs[idx]->old = h.data->old;
        ctx->hdrs[idx]->deleted = h.data->deleted;
        ctx->hdrs[idx]->flagged = h.data->flagged;
        ctx->hdrs[idx]->replied = h.data->replied;
        ctx->hdrs[idx]->received = h.received;
        ctx->hdrs[idx]->data = (void *) (h.data);
        STAILQ_INIT(&ctx->hdrs[idx]->tags);
        driver_tags_replace(&ctx->hdrs[idx]->tags, mutt_str_strdup(h.data->flags_remote));

        if (*maxuid < h.data->uid)
          *maxuid = h.data->uid;

        rewind(fp);
        /* NOTE: if Date: header is missing, mutt_rfc822_read_header depends
         *   on h.received being set */
        ctx->hdrs[idx]->env = mutt_rfc822_read_header(fp, ctx->hdrs[idx], false, false);
        /* content built as a side-effect of mutt_rfc822_read_header */
        ctx->hdrs[idx]->content->length = h.content_length;
        ctx->size += h.content_length;

#ifdef USE_HCACHE
        imap_hcache_put(idata, ctx->hdrs[idx]);
#endif /* USE_HCACHE */

        ctx->msgcount++;

        h.data = NULL;
        idx++;
      } while (mfhrc == -1);

      imap_free_header_data(&h.data);

      if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
        goto bail;
    }

    /* In case we get new mail while fetching the headers.
     *
     * Note: The RFC says we shouldn't get any EXPUNGE responses in the
     * middle of a FETCH.  But just to be cautious, use the current state
     * of max_msn, not fetch_msn_end to set the next start range.
     */
    if (idata->reopen & IMAP_NEWMAIL_PENDING)
    {
      /* update to the last value we actually pulled down */
      fetch_msn_end = idata->max_msn;
      msn_begin = idata->max_msn + 1;
      msn_end = idata->new_mail_count;
      while (msn_end > ctx->hdrmax)
        mx_alloc_memory(ctx);
      alloc_msn_index(idata, msn_end);
      idata->reopen &= ~IMAP_NEWMAIL_PENDING;
      idata->new_mail_count = 0;
    }
  }

  retval = 0;

bail:
  mutt_file_fclose(&fp);
  FREE(&hdrreq);

  return retval;
}

/**
 * imap_read_headers - Read headers from the server
 * @param idata     Server data
 * @param msn_begin First Message Sequence Number
 * @param msn_end   Last Message Sequence Number
 * @retval num Last MSN
 * @retval -1  Failure
 *
 * Changed to read many headers instead of just one. It will return the msn of
 * the last message read. It will return a value other than msn_end if mail
 * comes in while downloading headers (in theory).
 */
int imap_read_headers(struct ImapData *idata, unsigned int msn_begin,
                      unsigned int msn_end, int initial_download)
{
  struct ImapStatus *status = NULL;
  int oldmsgcount;
  unsigned int maxuid = 0;
  int retval = -1;
  bool evalhc = false;

#ifdef USE_HCACHE
  void *uid_validity = NULL;
  void *puidnext = NULL;
  unsigned int uidnext = 0;
  int has_condstore = 0;
  int has_qresync = 0;
  int eval_condstore = 0;
  int eval_qresync = 0;
  unsigned long long *pmodseq = NULL;
  unsigned long long hc_modseq = 0;
  char *uid_seqset = NULL;
#endif /* USE_HCACHE */

  struct Context *ctx = idata->ctx;

  /* make sure context has room to hold the mailbox */
  while (msn_end > ctx->hdrmax)
    mx_alloc_memory(ctx);
  alloc_msn_index(idata, msn_end);
  imap_alloc_uid_hash(idata, msn_end);

  oldmsgcount = ctx->msgcount;
  idata->reopen &= ~(IMAP_REOPEN_ALLOW | IMAP_NEWMAIL_PENDING);
  idata->new_mail_count = 0;

#ifdef USE_HCACHE
  idata->hcache = imap_hcache_open(idata, NULL);

  if (idata->hcache && initial_download)
  {
    uid_validity = mutt_hcache_fetch_raw(idata->hcache, "/UIDVALIDITY", 12);
    puidnext = mutt_hcache_fetch_raw(idata->hcache, "/UIDNEXT", 8);
    if (puidnext)
    {
      uidnext = *(unsigned int *) puidnext;
      mutt_hcache_free(idata->hcache, &puidnext);
    }

    if (idata->modseq)
    {
      if (mutt_bit_isset(idata->capabilities, CONDSTORE) && ImapCondStore)
        has_condstore = 1;

      /* If mutt_bit_isset(QRESYNC) and option(OPTIMAPQRESYNC) then Mutt
       * sends ENABLE QRESYNC.  If we receive an ENABLED response back, then
       * idata->qresync is set.
       */
      if (idata->qresync)
        has_qresync = 1;
    }

    if (uid_validity && uidnext && *(unsigned int *) uid_validity == idata->uid_validity)
    {
      evalhc = true;
      pmodseq = mutt_hcache_fetch_raw(idata->hcache, "/MODSEQ", 7);
      if (pmodseq)
      {
        hc_modseq = *pmodseq;
        mutt_hcache_free(idata->hcache, (void **) &pmodseq);
      }
      if (hc_modseq)
      {
        if (has_qresync)
        {
          uid_seqset = imap_hcache_get_uid_seqset(idata);
          if (uid_seqset)
            eval_qresync = 1;
        }

        if (!eval_qresync && has_condstore)
          eval_condstore = 1;
      }
    }
    mutt_hcache_free(idata->hcache, &uid_validity);
  }
  if (evalhc)
  {
    if (eval_qresync)
    {
      if (read_headers_qresync_eval_cache(idata, uid_seqset) < 0)
        goto bail;
    }
    else
    {
      if (read_headers_normal_eval_cache(idata, msn_end, uidnext, has_condstore || has_qresync,
                                         eval_condstore) < 0)
        goto bail;
    }

    if ((eval_condstore || eval_qresync) && (hc_modseq != idata->modseq))
      if (read_headers_condstore_qresync_updates(idata, msn_end, uidnext,
                                                 hc_modseq, eval_qresync) < 0)
        goto bail;

    /* Look for the first empty MSN and start there */
    while (msn_begin <= msn_end)
    {
      if (!idata->msn_index[msn_begin - 1])
        break;
      msn_begin++;
    }
  }
#endif /* USE_HCACHE */

  if (read_headers_fetch_new(idata, msn_begin, msn_end, evalhc, &maxuid, initial_download) < 0)
    goto bail;

  if (maxuid && (status = imap_mboxcache_get(idata, idata->mailbox, 0)) &&
      (status->uidnext < maxuid + 1))
    status->uidnext = maxuid + 1;

#if USE_HCACHE
  mutt_hcache_store_raw(idata->hcache, "/UIDVALIDITY", 12, &idata->uid_validity,
                        sizeof(idata->uid_validity));
  if (maxuid && idata->uidnext < maxuid + 1)
  {
    mutt_debug(2, "Overriding UIDNEXT: %u -> %u\n", idata->uidnext, maxuid + 1);
    idata->uidnext = maxuid + 1;
  }
  if (idata->uidnext > 1)
    mutt_hcache_store_raw(idata->hcache, "/UIDNEXT", 8, &idata->uidnext,
                          sizeof(idata->uidnext));

  /* We currently only sync CONDSTORE and QRESYNC on the initial download.
   * To do it more often, we'll need to deal with flag updates combined with
   * unsync'ed local flag changes.  We'll also need to properly sync flags to
   * the header cache on close.  I'm not sure it's worth the added complexity.
   */
  if (initial_download)
  {
    if (has_condstore || has_qresync)
      mutt_hcache_store_raw(idata->hcache, "/MODSEQ", 7, &idata->modseq,
                            sizeof(idata->modseq));
    else
      mutt_hcache_delete(idata->hcache, "/MODSEQ", 7);

    if (has_qresync)
      imap_hcache_store_uid_seqset(idata);
    else
      imap_hcache_clear_uid_seqset(idata);
  }
#endif /* USE_HCACHE */

  if (ctx->msgcount > oldmsgcount)
  {
    /* TODO: it's not clear to me why we are calling mx_alloc_memory
     *       yet again. */
    mx_alloc_memory(ctx);
    mx_update_context(ctx, ctx->msgcount - oldmsgcount);
  }

  idata->reopen |= IMAP_REOPEN_ALLOW;

  retval = msn_end;

bail:
#if USE_HCACHE
  imap_hcache_close(idata);
  FREE(&uid_seqset);
#endif /* USE_HCACHE */

  return retval;
}

/**
 * imap_msg_open - Implements MxOps::msg_open()
 */
int imap_msg_open(struct Context *ctx, struct Message *msg, int msgno)
{
  struct Envelope *newenv = NULL;
  char buf[LONG_STRING];
  char path[PATH_MAX];
  char *pc = NULL;
  unsigned int bytes;
  struct Progress progressbar;
  unsigned int uid;
  int cacheno;
  struct ImapCache *cache = NULL;
  bool retried = false;
  bool read;
  int rc;

  /* Sam's weird courier server returns an OK response even when FETCH
   * fails. Thanks Sam. */
  bool fetched = false;
  int output_progress;

  struct ImapData *idata = ctx->data;
  struct Header *h = ctx->hdrs[msgno];

  msg->fp = msg_cache_get(idata, h);
  if (msg->fp)
  {
    if (HEADER_DATA(h)->parsed)
      return 0;
    else
      goto parsemsg;
  }

  /* we still do some caching even if imap_cachedir is unset */
  /* see if we already have the message in our cache */
  cacheno = HEADER_DATA(h)->uid % IMAP_CACHE_LEN;
  cache = &idata->cache[cacheno];

  if (cache->path)
  {
    /* don't treat cache errors as fatal, just fall back. */
    if (cache->uid == HEADER_DATA(h)->uid && (msg->fp = fopen(cache->path, "r")))
      return 0;
    else
    {
      unlink(cache->path);
      FREE(&cache->path);
    }
  }

  /* This function is called in a few places after endwin()
   * e.g. mutt_pipe_message(). */
  output_progress = !isendwin();
  if (output_progress)
    mutt_message(_("Fetching message..."));

  msg->fp = msg_cache_put(idata, h);
  if (!msg->fp)
  {
    cache->uid = HEADER_DATA(h)->uid;
    mutt_mktemp(path, sizeof(path));
    cache->path = mutt_str_strdup(path);
    msg->fp = mutt_file_fopen(path, "w+");
    if (!msg->fp)
    {
      FREE(&cache->path);
      return -1;
    }
  }

  /* mark this header as currently inactive so the command handler won't
   * also try to update it. HACK until all this code can be moved into the
   * command handler */
  h->active = false;

  snprintf(buf, sizeof(buf), "UID FETCH %u %s", HEADER_DATA(h)->uid,
           (mutt_bit_isset(idata->capabilities, IMAP4REV1) ?
                (ImapPeek ? "BODY.PEEK[]" : "BODY[]") :
                "RFC822"));

  imap_cmd_start(idata, buf);
  do
  {
    rc = imap_cmd_step(idata);
    if (rc != IMAP_CMD_CONTINUE)
      break;

    pc = idata->buf;
    pc = imap_next_word(pc);
    pc = imap_next_word(pc);

    if (mutt_str_strncasecmp("FETCH", pc, 5) == 0)
    {
      while (*pc)
      {
        pc = imap_next_word(pc);
        if (pc[0] == '(')
          pc++;
        if (mutt_str_strncasecmp("UID", pc, 3) == 0)
        {
          pc = imap_next_word(pc);
          if (mutt_str_atoui(pc, &uid) < 0)
            goto bail;
          if (uid != HEADER_DATA(h)->uid)
          {
            mutt_error(_(
                "The message index is incorrect. Try reopening the mailbox."));
          }
        }
        else if ((mutt_str_strncasecmp("RFC822", pc, 6) == 0) ||
                 (mutt_str_strncasecmp("BODY[]", pc, 6) == 0))
        {
          pc = imap_next_word(pc);
          if (imap_get_literal_count(pc, &bytes) < 0)
          {
            imap_error("imap_msg_open()", buf);
            goto bail;
          }
          if (output_progress)
          {
            mutt_progress_init(&progressbar, _("Fetching message..."),
                               MUTT_PROGRESS_SIZE, NetInc, bytes);
          }
          if (imap_read_literal(msg->fp, idata, bytes,
                                output_progress ? &progressbar : NULL) < 0)
          {
            goto bail;
          }
          /* pick up trailing line */
          rc = imap_cmd_step(idata);
          if (rc != IMAP_CMD_CONTINUE)
            goto bail;
          pc = idata->buf;

          fetched = true;
        }
        /* UW-IMAP will provide a FLAGS update here if the FETCH causes a
         * change (eg from \Unseen to \Seen).
         * Uncommitted changes in neomutt take precedence. If we decide to
         * incrementally update flags later, this won't stop us syncing */
        else if ((mutt_str_strncasecmp("FLAGS", pc, 5) == 0) && !h->changed)
        {
          pc = imap_set_flags(idata, h, pc, NULL);
          if (!pc)
            goto bail;
        }
      }
    }
  } while (rc == IMAP_CMD_CONTINUE);

  /* see comment before command start. */
  h->active = true;

  fflush(msg->fp);
  if (ferror(msg->fp))
  {
    mutt_perror(cache->path);
    goto bail;
  }

  if (rc != IMAP_CMD_OK)
    goto bail;

  if (!fetched || !imap_code(idata->buf))
    goto bail;

  msg_cache_commit(idata, h);

parsemsg:
  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.
   */
  rewind(msg->fp);
  /* It may be that the Status header indicates a message is read, but the
   * IMAP server doesn't know the message has been \Seen. So we capture
   * the server's notion of 'read' and if it differs from the message info
   * picked up in mutt_rfc822_read_header, we mark the message (and context
   * changed). Another possibility: ignore Status on IMAP? */
  read = h->read;
  newenv = mutt_rfc822_read_header(msg->fp, h, false, false);
  mutt_env_merge(h->env, &newenv);

  /* see above. We want the new status in h->read, so we unset it manually
   * and let mutt_set_flag set it correctly, updating context. */
  if (read != h->read)
  {
    h->read = read;
    mutt_set_flag(ctx, h, MUTT_NEW, read);
  }

  h->lines = 0;
  fgets(buf, sizeof(buf), msg->fp);
  while (!feof(msg->fp))
  {
    h->lines++;
    fgets(buf, sizeof(buf), msg->fp);
  }

  h->content->length = ftell(msg->fp) - h->content->offset;

  mutt_clear_error();
  rewind(msg->fp);
  HEADER_DATA(h)->parsed = true;

  /* retry message parse if cached message is empty */
  if (!retried && ((h->lines == 0) || (h->content->length == 0)))
  {
    imap_cache_del(idata, h);
    retried = true;
    goto parsemsg;
  }

  return 0;

bail:
  mutt_file_fclose(&msg->fp);
  imap_cache_del(idata, h);
  if (cache->path)
  {
    unlink(cache->path);
    FREE(&cache->path);
  }

  return -1;
}

/**
 * imap_msg_close - Close an email
 * @param ctx Mailbox
 * @param msg Message to close
 * @retval 0 Success
 *
 * @note May also return EOF Failure, see errno
 */
int imap_msg_close(struct Context *ctx, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * imap_msg_commit - Implements MxOps::msg_commit()
 *
 * @note May also return EOF Failure, see errno
 */
int imap_msg_commit(struct Context *ctx, struct Message *msg)
{
  int r = mutt_file_fclose(&msg->fp);
  if (r != 0)
    return r;

  return imap_append_message(ctx, msg);
}

/**
 * imap_append_message - Write an email back to the server
 * @param ctx Context
 * @param msg Message to save
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_append_message(struct Context *ctx, struct Message *msg)
{
  FILE *fp = NULL;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mailbox[LONG_STRING];
  char internaldate[IMAP_DATELEN];
  char imap_flags[SHORT_STRING];
  size_t len;
  struct Progress progressbar;
  size_t sent;
  int c, last;
  struct ImapMbox mx;
  int rc;

  struct ImapData *idata = ctx->data;

  if (imap_parse_path(ctx->path, &mx))
    return -1;

  imap_fix_path(idata, mx.mbox, mailbox, sizeof(mailbox));
  if (!*mailbox)
    mutt_str_strfcpy(mailbox, "INBOX", sizeof(mailbox));

  fp = fopen(msg->path, "r");
  if (!fp)
  {
    mutt_perror(msg->path);
    goto fail;
  }

  /* currently we set the \Seen flag on all messages, but probably we
   * should scan the message Status header for flag info. Since we're
   * already rereading the whole file for length it isn't any more
   * expensive (it'd be nice if we had the file size passed in already
   * by the code that writes the file, but that's a lot of changes.
   * Ideally we'd have a Header structure with flag info here... */
  for (last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if (c == '\n' && last != '\r')
      len++;

    len++;
  }
  rewind(fp);

  mutt_progress_init(&progressbar, _("Uploading message..."),
                     MUTT_PROGRESS_SIZE, NetInc, len);

  imap_munge_mbox_name(idata, mbox, sizeof(mbox), mailbox);
  mutt_date_make_imap(internaldate, sizeof(internaldate), msg->received);

  imap_flags[0] = 0;
  imap_flags[1] = 0;

  if (msg->flags.read)
    mutt_str_strcat(imap_flags, sizeof(imap_flags), " \\Seen");
  if (msg->flags.replied)
    mutt_str_strcat(imap_flags, sizeof(imap_flags), " \\Answered");
  if (msg->flags.flagged)
    mutt_str_strcat(imap_flags, sizeof(imap_flags), " \\Flagged");
  if (msg->flags.draft)
    mutt_str_strcat(imap_flags, sizeof(imap_flags), " \\Draft");

  snprintf(buf, sizeof(buf), "APPEND %s (%s) \"%s\" {%lu}", mbox,
           imap_flags + 1, internaldate, (unsigned long) len);

  imap_cmd_start(idata, buf);

  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_RESPOND)
  {
    mutt_debug(1, "#1 command failed: %s\n", idata->buf);

    char *pc = idata->buf + SEQLEN;
    SKIPWS(pc);
    pc = imap_next_word(pc);
    mutt_error("%s", pc);
    mutt_file_fclose(&fp);
    goto fail;
  }

  for (last = EOF, sent = len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if (c == '\n' && last != '\r')
      buf[len++] = '\r';

    buf[len++] = c;

    if (len > sizeof(buf) - 3)
    {
      sent += len;
      flush_buffer(buf, &len, idata->conn);
      mutt_progress_update(&progressbar, sent, -1);
    }
  }

  if (len)
    flush_buffer(buf, &len, idata->conn);

  mutt_socket_send(idata->conn, "\r\n");
  mutt_file_fclose(&fp);

  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (!imap_code(idata->buf))
  {
    mutt_debug(1, "#2 command failed: %s\n", idata->buf);
    char *pc = idata->buf + SEQLEN;
    SKIPWS(pc);
    pc = imap_next_word(pc);
    mutt_error("%s", pc);
    goto fail;
  }

  FREE(&mx.mbox);
  return 0;

fail:
  FREE(&mx.mbox);
  return -1;
}

/**
 * imap_copy_messages - Server COPY messages to another folder
 * @param ctx    Context
 * @param h      Header of the email
 * @param dest   Destination folder
 * @param delete Delete the original?
 * @retval -1 Error
 * @retval  0 Success
 * @retval  1 Non-fatal error - try fetch/append
 */
int imap_copy_messages(struct Context *ctx, struct Header *h, char *dest, bool delete)
{
  struct Buffer cmd, sync_cmd;
  char mbox[PATH_MAX];
  char mmbox[PATH_MAX];
  char prompt[PATH_MAX + 64];
  int rc;
  struct ImapMbox mx;
  int err_continue = MUTT_NO;
  int triedcreate = 0;

  struct ImapData *idata = ctx->data;

  if (imap_parse_path(dest, &mx))
  {
    mutt_debug(1, "bad destination %s\n", dest);
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (mutt_account_match(&(idata->conn->account), &(mx.account)) == 0)
  {
    mutt_debug(3, "%s not same server as %s\n", dest, ctx->path);
    return 1;
  }

  if (h && h->attach_del)
  {
    mutt_debug(3, "#1 Message contains attachments to be deleted\n");
    return 1;
  }

  imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));
  if (!*mbox)
    mutt_str_strfcpy(mbox, "INBOX", sizeof(mbox));
  imap_munge_mbox_name(idata, mmbox, sizeof(mmbox), mbox);

  /* loop in case of TRYCREATE */
  do
  {
    mutt_buffer_init(&sync_cmd);
    mutt_buffer_init(&cmd);

    /* Null Header* means copy tagged messages */
    if (!h)
    {
      /* if any messages have attachments to delete, fall through to FETCH
       * and APPEND. TODO: Copy what we can with COPY, fall through for the
       * remainder. */
      for (int i = 0; i < ctx->msgcount; i++)
      {
        if (!message_is_tagged(ctx, i))
          continue;

        if (ctx->hdrs[i]->attach_del)
        {
          mutt_debug(3, "#2 Message contains attachments to be deleted\n");
          return 1;
        }

        if (ctx->hdrs[i]->active && ctx->hdrs[i]->changed)
        {
          rc = imap_sync_message_for_copy(idata, ctx->hdrs[i], &sync_cmd, &err_continue);
          if (rc < 0)
          {
            mutt_debug(1, "#1 could not sync\n");
            goto out;
          }
        }
      }

      rc = imap_exec_msgset(idata, "UID COPY", mmbox, MUTT_TAG, false, false);
      if (!rc)
      {
        mutt_debug(1, "No messages tagged\n");
        rc = -1;
        goto out;
      }
      else if (rc < 0)
      {
        mutt_debug(1, "#1 could not queue copy\n");
        goto out;
      }
      else
      {
        mutt_message(ngettext("Copying %d message to %s...", "Copying %d messages to %s...", rc),
                     rc, mbox);
      }
    }
    else
    {
      mutt_message(_("Copying message %d to %s..."), h->index + 1, mbox);
      mutt_buffer_printf(&cmd, "UID COPY %u %s", HEADER_DATA(h)->uid, mmbox);

      if (h->active && h->changed)
      {
        rc = imap_sync_message_for_copy(idata, h, &sync_cmd, &err_continue);
        if (rc < 0)
        {
          mutt_debug(1, "#2 could not sync\n");
          goto out;
        }
      }
      rc = imap_exec(idata, cmd.data, IMAP_CMD_QUEUE);
      if (rc < 0)
      {
        mutt_debug(1, "#2 could not queue copy\n");
        goto out;
      }
    }

    /* let's get it on */
    rc = imap_exec(idata, NULL, IMAP_CMD_FAIL_OK);
    if (rc == -2)
    {
      if (triedcreate)
      {
        mutt_debug(1, "Already tried to create mailbox %s\n", mbox);
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (mutt_str_strncasecmp(imap_get_qualifier(idata->buf), "[TRYCREATE]", 11) != 0)
        break;
      mutt_debug(3, "server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), mbox);
      if (Confirmcreate && mutt_yesorno(prompt, 1) != MUTT_YES)
      {
        mutt_clear_error();
        goto out;
      }
      if (imap_create_mailbox(idata, mbox) < 0)
        break;
      triedcreate = 1;
    }
  } while (rc == -2);

  if (rc != 0)
  {
    imap_error("imap_copy_messages", idata->buf);
    goto out;
  }

  /* cleanup */
  if (delete)
  {
    if (!h)
    {
      for (int i = 0; i < ctx->msgcount; i++)
      {
        if (!message_is_tagged(ctx, i))
          continue;

        mutt_set_flag(ctx, ctx->hdrs[i], MUTT_DELETE, 1);
        mutt_set_flag(ctx, ctx->hdrs[i], MUTT_PURGE, 1);
        if (DeleteUntag)
          mutt_set_flag(ctx, ctx->hdrs[i], MUTT_TAG, 0);
      }
    }
    else
    {
      mutt_set_flag(ctx, h, MUTT_DELETE, 1);
      mutt_set_flag(ctx, h, MUTT_PURGE, 1);
      if (DeleteUntag)
        mutt_set_flag(ctx, h, MUTT_TAG, 0);
    }
  }

  rc = 0;

out:
  if (cmd.data)
    FREE(&cmd.data);
  if (sync_cmd.data)
    FREE(&sync_cmd.data);
  FREE(&mx.mbox);

  return (rc < 0) ? -1 : rc;
}

/**
 * imap_cache_del - Delete an email from the body cache
 * @param idata Server data
 * @param h     Email header
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_cache_del(struct ImapData *idata, struct Header *h)
{
  if (!idata || !h)
    return -1;

  idata->bcache = msg_cache_open(idata);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);
  return mutt_bcache_del(idata->bcache, id);
}

/**
 * imap_cache_clean - Delete all the entries in the message cache
 * @param idata Server data
 * @retval 0 Always
 */
int imap_cache_clean(struct ImapData *idata)
{
  idata->bcache = msg_cache_open(idata);
  mutt_bcache_list(idata->bcache, msg_cache_clean_cb, idata);

  return 0;
}

/**
 * imap_free_header_data - free ImapHeader structure
 * @param data Header data to free
 */
void imap_free_header_data(struct ImapHeaderData **data)
{
  if (!data || !*data)
    return;

  /* this should be safe even if the list wasn't used */
  FREE(&((*data)->flags_system));
  FREE(&((*data)->flags_remote));
  FREE(data);
}

/**
 * imap_set_flags - fill the message header according to the server flags
 * @param[in]  idata          Server data
 * @param[in]  h              Email Header
 * @param[in]  s              Command string
 * @param[out] server_changes Flags have changed
 * @retval ptr  The end of flags string
 * @retval NULL Failure
 *
 * Expects a flags line of the form "FLAGS (flag flag ...)"
 *
 * imap_set_flags: fill out the message header according to the flags from
 * the server. Expects a flags line of the form "FLAGS (flag flag ...)"
 *
 * Sets server_changes to 1 if a change to a flag is made, or in the
 * case of h->changed, if a change to a flag _would_ have been
 * made.
 */
char *imap_set_flags(struct ImapData *idata, struct Header *h, char *s, int *server_changes)
{
  struct Context *ctx = idata->ctx;
  struct ImapHeader newh = { 0 };
  struct ImapHeaderData old_hd;
  bool readonly;
  int local_changes;

  local_changes = h->changed;

  struct ImapHeaderData *hd = h->data;
  newh.data = hd;

  memcpy(&old_hd, hd, sizeof(old_hd));

  mutt_debug(2, "parsing FLAGS\n");
  s = msg_parse_flags(&newh, s);
  if (!s)
    return NULL;

  /* Update tags system */
  driver_tags_replace(&h->tags, mutt_str_strdup(hd->flags_remote));

  /* YAUH (yet another ugly hack): temporarily set context to
   * read-write even if it's read-only, so *server* updates of
   * flags can be processed by mutt_set_flag. ctx->changed must
   * be restored afterwards */
  readonly = ctx->readonly;
  ctx->readonly = false;

  /* This is redundant with the following two checks. Removing:
   * mutt_set_flag (ctx, h, MUTT_NEW, !(hd->read || hd->old));
   */
  set_changed_flag(ctx, h, local_changes, server_changes, MUTT_OLD, old_hd.old,
                   hd->old, h->old);
  set_changed_flag(ctx, h, local_changes, server_changes, MUTT_READ,
                   old_hd.read, hd->read, h->read);
  set_changed_flag(ctx, h, local_changes, server_changes, MUTT_DELETE,
                   old_hd.deleted, hd->deleted, h->deleted);
  set_changed_flag(ctx, h, local_changes, server_changes, MUTT_FLAG,
                   old_hd.flagged, hd->flagged, h->flagged);
  set_changed_flag(ctx, h, local_changes, server_changes, MUTT_REPLIED,
                   old_hd.replied, hd->replied, h->replied);

  /* this message is now definitively *not* changed (mutt_set_flag
   * marks things changed as a side-effect) */
  if (!local_changes)
    h->changed = false;
  ctx->changed &= !readonly;
  ctx->readonly = readonly;

  return s;
}
