/**
 * @file
 * Manage IMAP messages
 *
 * @authors
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page imap_message IMAP messages
 *
 * Manage IMAP messages
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "message.h"
#include "lib.h"
#include "bcache/lib.h"
#include "progress/lib.h"
#include "question/lib.h"
#include "adata.h"
#include "edata.h"
#include "external.h"
#include "globals.h" // IWYU pragma: keep
#include "mdata.h"
#include "msg_set.h"
#include "msn.h"
#include "mutt_logging.h"
#include "mx.h"
#include "protos.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_HCACHE
#include "hcache/lib.h"
#endif

struct BodyCache;

/**
 * msg_cache_open - Open a message cache
 * @param m     Selected Imap Mailbox
 * @retval ptr  Success, using existing cache (or opened new cache)
 * @retval NULL Failure
 */
static struct BodyCache *msg_cache_open(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!adata || (adata->mailbox != m))
    return NULL;

  if (mdata->bcache)
    return mdata->bcache;

  struct Buffer *mailbox = buf_pool_get();
  imap_cachepath(adata->delim, mdata->name, mailbox);

  struct BodyCache *bc = mutt_bcache_open(&adata->conn->account, buf_string(mailbox));
  buf_pool_release(&mailbox);

  return bc;
}

/**
 * msg_cache_get - Get the message cache entry for an email
 * @param m     Selected Imap Mailbox
 * @param e     Email
 * @retval ptr  Success, handle of cache entry
 * @retval NULL Failure
 */
static FILE *msg_cache_get(struct Mailbox *m, struct Email *e)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!e || !adata || (adata->mailbox != m))
    return NULL;

  mdata->bcache = msg_cache_open(m);
  char id[64] = { 0 };
  snprintf(id, sizeof(id), "%u-%u", mdata->uidvalidity, imap_edata_get(e)->uid);
  return mutt_bcache_get(mdata->bcache, id);
}

/**
 * msg_cache_put - Put an email into the message cache
 * @param m     Selected Imap Mailbox
 * @param e     Email
 * @retval ptr  Success, handle of cache entry
 * @retval NULL Failure
 */
static FILE *msg_cache_put(struct Mailbox *m, struct Email *e)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!e || !adata || (adata->mailbox != m))
    return NULL;

  mdata->bcache = msg_cache_open(m);
  char id[64] = { 0 };
  snprintf(id, sizeof(id), "%u-%u", mdata->uidvalidity, imap_edata_get(e)->uid);
  return mutt_bcache_put(mdata->bcache, id);
}

/**
 * msg_cache_commit - Add to the message cache
 * @param m     Selected Imap Mailbox
 * @param e     Email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int msg_cache_commit(struct Mailbox *m, struct Email *e)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!e || !adata || (adata->mailbox != m))
    return -1;

  mdata->bcache = msg_cache_open(m);
  char id[64] = { 0 };
  snprintf(id, sizeof(id), "%u-%u", mdata->uidvalidity, imap_edata_get(e)->uid);

  return mutt_bcache_commit(mdata->bcache, id);
}

/**
 * msg_cache_clean_cb - Delete an entry from the message cache - Implements ::bcache_list_t - @ingroup bcache_list_api
 * @retval 0 Always
 */
static int msg_cache_clean_cb(const char *id, struct BodyCache *bcache, void *data)
{
  uint32_t uv;
  unsigned int uid;
  struct ImapMboxData *mdata = data;

  if (sscanf(id, "%u-%u", &uv, &uid) != 2)
    return 0;

  /* bad UID */
  if ((uv != mdata->uidvalidity) || !mutt_hash_int_find(mdata->uid_hash, uid))
    mutt_bcache_del(bcache, id);

  return 0;
}

/**
 * msg_parse_flags - Read a FLAGS token into an ImapHeader
 * @param h Header to store flags
 * @param s Command string containing flags
 * @retval ptr  The end of flags string
 * @retval NULL Failure
 */
static char *msg_parse_flags(struct ImapHeader *h, char *s)
{
  struct ImapEmailData *edata = h->edata;

  /* sanity-check string */
  size_t plen = mutt_istr_startswith(s, "FLAGS");
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
  s++;

  FREE(&edata->flags_system);
  FREE(&edata->flags_remote);

  edata->deleted = false;
  edata->flagged = false;
  edata->replied = false;
  edata->read = false;
  edata->old = false;

  /* start parsing */
  while (*s && (*s != ')'))
  {
    if ((plen = mutt_istr_startswith(s, "\\deleted")))
    {
      s += plen;
      edata->deleted = true;
    }
    else if ((plen = mutt_istr_startswith(s, "\\flagged")))
    {
      s += plen;
      edata->flagged = true;
    }
    else if ((plen = mutt_istr_startswith(s, "\\answered")))
    {
      s += plen;
      edata->replied = true;
    }
    else if ((plen = mutt_istr_startswith(s, "\\seen")))
    {
      s += plen;
      edata->read = true;
    }
    else if ((plen = mutt_istr_startswith(s, "\\recent")))
    {
      s += plen;
    }
    else if ((plen = mutt_istr_startswith(s, "old")))
    {
      s += plen;
      edata->old = cs_subset_bool(NeoMutt->sub, "mark_old");
    }
    else
    {
      char ctmp;
      char *flag_word = s;
      bool is_system_keyword = mutt_istr_startswith(s, "\\");

      while (*s && !isspace(*s) && (*s != ')'))
        s++;

      ctmp = *s;
      *s = '\0';

      if (is_system_keyword)
      {
        /* store other system flags as well (mainly \\Draft) */
        mutt_str_append_item(&edata->flags_system, flag_word, ' ');
      }
      else
      {
        /* store custom flags as well */
        mutt_str_append_item(&edata->flags_remote, flag_word, ' ');
      }

      *s = ctmp;
    }
    SKIPWS(s);
  }

  /* wrap up, or note bad flags response */
  if (*s == ')')
  {
    s++;
  }
  else
  {
    mutt_debug(LL_DEBUG1, "Unterminated FLAGS response: %s\n", s);
    return NULL;
  }

  return s;
}

/**
 * msg_parse_fetch - Handle headers returned from header fetch
 * @param h IMAP Header
 * @param s Command string
 * @retval  0 Success
 * @retval -1 String is corrupted
 * @retval -2 Fetch contains a body or header lines that still need to be parsed
 */
static int msg_parse_fetch(struct ImapHeader *h, char *s)
{
  if (!s)
    return -1;

  char tmp[128] = { 0 };
  char *ptmp = NULL;
  size_t plen = 0;

  while (*s)
  {
    SKIPWS(s);

    if (mutt_istr_startswith(s, "FLAGS"))
    {
      s = msg_parse_flags(h, s);
      if (!s)
        return -1;
    }
    else if ((plen = mutt_istr_startswith(s, "UID")))
    {
      s += plen;
      SKIPWS(s);
      if (!mutt_str_atoui(s, &h->edata->uid))
        return -1;

      s = imap_next_word(s);
    }
    else if ((plen = mutt_istr_startswith(s, "INTERNALDATE")))
    {
      s += plen;
      SKIPWS(s);
      if (*s != '\"')
      {
        mutt_debug(LL_DEBUG1, "bogus INTERNALDATE entry: %s\n", s);
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
    else if ((plen = mutt_istr_startswith(s, "RFC822.SIZE")))
    {
      s += plen;
      SKIPWS(s);
      ptmp = tmp;
      while (isdigit((unsigned char) *s) && (ptmp != (tmp + sizeof(tmp) - 1)))
        *ptmp++ = *s++;
      *ptmp = '\0';
      if (!mutt_str_atol(tmp, &h->content_length))
        return -1;
    }
    else if (mutt_istr_startswith(s, "BODY") || mutt_istr_startswith(s, "RFC822.HEADER"))
    {
      /* handle above, in msg_fetch_header */
      return -2;
    }
    else if ((plen = mutt_istr_startswith(s, "MODSEQ")))
    {
      s += plen;
      SKIPWS(s);
      if (*s != '(')
      {
        mutt_debug(LL_DEBUG1, "bogus MODSEQ response: %s\n", s);
        return -1;
      }
      s++;
      while (*s && (*s != ')'))
        s++;
      if (*s == ')')
      {
        s++;
      }
      else
      {
        mutt_debug(LL_DEBUG1, "Unterminated MODSEQ response: %s\n", s);
        return -1;
      }
    }
    else if (*s == ')')
    {
      s++; /* end of request */
    }
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
 * msg_fetch_header - Import IMAP FETCH response into an ImapHeader
 * @param m   Mailbox
 * @param ih  ImapHeader
 * @param buf Server string containing FETCH response
 * @param fp  Connection to server
 * @retval  0 Success
 * @retval -1 String is not a fetch response
 * @retval -2 String is a corrupt fetch response
 *
 * Expects string beginning with * n FETCH.
 */
static int msg_fetch_header(struct Mailbox *m, struct ImapHeader *ih, char *buf, FILE *fp)
{
  int rc = -1; /* default now is that string isn't FETCH response */

  struct ImapAccountData *adata = imap_adata_get(m);

  if (buf[0] != '*')
    return rc;

  /* skip to message number */
  buf = imap_next_word(buf);
  if (!mutt_str_atoui(buf, &ih->edata->msn))
    return rc;

  /* find FETCH tag */
  buf = imap_next_word(buf);
  if (!mutt_istr_startswith(buf, "FETCH"))
    return rc;

  rc = -2; /* we've got a FETCH response, for better or worse */
  buf = strchr(buf, '(');
  if (!buf)
    return rc;
  buf++;

  /* FIXME: current implementation - call msg_parse_fetch - if it returns -2,
   *   read header lines and call it again. Silly. */
  int parse_rc = msg_parse_fetch(ih, buf);
  if (parse_rc == 0)
    return 0;
  if ((parse_rc != -2) || !fp)
    return rc;

  unsigned int bytes = 0;
  if (imap_get_literal_count(buf, &bytes) == 0)
  {
    imap_read_literal(fp, adata, bytes, NULL);

    /* we may have other fields of the FETCH _after_ the literal
     * (eg Domino puts FLAGS here). Nothing wrong with that, either.
     * This all has to go - we should accept literals and nonliterals
     * interchangeably at any time. */
    if (imap_cmd_step(adata) != IMAP_RES_CONTINUE)
      return rc;

    if (msg_parse_fetch(ih, adata->buf) == -1)
      return rc;
  }

  rc = 0; /* success */

  /* subtract headers from message size - unfortunately only the subset of
   * headers we've requested. */
  ih->content_length -= bytes;

  return rc;
}

/**
 * flush_buffer - Write data to a connection
 * @param buf  Buffer containing data
 * @param len  Length of buffer
 * @param conn Network connection
 * @retval >0 Number of bytes written
 * @retval -1 Error
 */
static int flush_buffer(char *buf, size_t *len, struct Connection *conn)
{
  buf[*len] = '\0';
  int rc = mutt_socket_write_n(conn, buf, *len);
  *len = 0;
  return rc;
}

/**
 * query_abort_header_download - Ask the user whether to abort the download
 * @param adata Imap Account data
 * @retval true Abort the download
 *
 * If the user hits ctrl-c during an initial header download for a mailbox,
 * prompt whether to completely abort the download and close the mailbox.
 */
static bool query_abort_header_download(struct ImapAccountData *adata)
{
  bool abort = false;

  mutt_flushinp();
  /* L10N: This prompt is made if the user hits Ctrl-C when opening an IMAP mailbox */
  if (mutt_yesorno(_("Abort download and close mailbox?"), MUTT_YES) == MUTT_YES)
  {
    abort = true;
    imap_close_connection(adata);
  }
  SigInt = false;

  return abort;
}

/**
 * imap_alloc_uid_hash - Create a Hash Table for the UIDs
 * @param adata Imap Account data
 * @param msn_count Number of MSNs in use
 *
 * This function is run after imap_imap_msn_reserve, so we skip the
 * malicious msn_count size check.
 */
static void imap_alloc_uid_hash(struct ImapAccountData *adata, unsigned int msn_count)
{
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  if (!mdata->uid_hash)
    mdata->uid_hash = mutt_hash_int_new(MAX(6 * msn_count / 5, 30), MUTT_HASH_NO_FLAGS);
}

/**
 * imap_fetch_msn_seqset - Generate a sequence set
 * @param[in]  buf           Buffer for the result
 * @param[in]  adata         Imap Account data
 * @param[in]  evalhc        If true, check the Header Cache
 * @param[in]  msn_begin     First Message Sequence Number
 * @param[in]  msn_end       Last Message Sequence Number
 * @param[out] fetch_msn_end Highest Message Sequence Number fetched
 * @retval num MSN count
 *
 * Generates a more complicated sequence set after using the header cache,
 * in case there are missing MSNs in the middle.
 *
 * This can happen if during a sync/close, messages are deleted from
 * the cache, but the server doesn't get the updates (via a dropped
 * network connection, or just plain refusing the updates).
 */
static unsigned int imap_fetch_msn_seqset(struct Buffer *buf, struct ImapAccountData *adata,
                                          bool evalhc, unsigned int msn_begin,
                                          unsigned int msn_end, unsigned int *fetch_msn_end)
{
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  unsigned int max_headers_per_fetch = UINT_MAX;
  bool first_chunk = true;
  int state = 0; /* 1: single msn, 2: range of msn */
  unsigned int msn;
  unsigned int range_begin = 0;
  unsigned int range_end = 0;
  unsigned int msn_count = 0;

  buf_reset(buf);
  if (msn_end < msn_begin)
    return 0;

  const long c_imap_fetch_chunk_size = cs_subset_long(NeoMutt->sub, "imap_fetch_chunk_size");
  if (c_imap_fetch_chunk_size > 0)
    max_headers_per_fetch = c_imap_fetch_chunk_size;

  if (!evalhc)
  {
    if ((msn_end - msn_begin + 1) <= max_headers_per_fetch)
      *fetch_msn_end = msn_end;
    else
      *fetch_msn_end = msn_begin + max_headers_per_fetch - 1;
    buf_printf(buf, "%u:%u", msn_begin, *fetch_msn_end);
    return (*fetch_msn_end - msn_begin + 1);
  }

  for (msn = msn_begin; msn <= (msn_end + 1); msn++)
  {
    if ((msn_count < max_headers_per_fetch) && (msn <= msn_end) &&
        !imap_msn_get(&mdata->msn, msn - 1))
    {
      msn_count++;

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
      if (first_chunk)
        first_chunk = false;
      else
        buf_addch(buf, ',');

      if (state == 1)
        buf_add_printf(buf, "%u", range_begin);
      else if (state == 2)
        buf_add_printf(buf, "%u:%u", range_begin, range_end);
      state = 0;

      if ((buf_len(buf) > 500) || (msn_count >= max_headers_per_fetch))
        break;
    }
  }

  /* The loop index goes one past to terminate the range if needed. */
  *fetch_msn_end = msn - 1;

  return msn_count;
}

/**
 * set_changed_flag - Have the flags of an email changed
 * @param[in]  m              Mailbox
 * @param[in]  e              Email
 * @param[in]  local_changes  Has the local mailbox been changed?
 * @param[out] server_changes Set to true if the flag has changed
 * @param[in]  flag_name      Flag to check, e.g. #MUTT_FLAG
 * @param[in]  old_hd_flag    Old header flags
 * @param[in]  new_hd_flag    New header flags
 * @param[in]  h_flag         Email's value for flag_name
 *
 * Sets server_changes to 1 if a change to a flag is made, or in the
 * case of local_changes, if a change to a flag _would_ have been
 * made.
 */
static void set_changed_flag(struct Mailbox *m, struct Email *e, int local_changes,
                             bool *server_changes, enum MessageType flag_name,
                             bool old_hd_flag, bool new_hd_flag, bool h_flag)
{
  /* If there are local_changes, we only want to note if the server
   * flags have changed, so we can set a reopen flag in
   * cmd_parse_fetch().  We don't want to count a local modification
   * to the header flag as a "change".  */
  if ((old_hd_flag == new_hd_flag) && (local_changes != 0))
    return;

  if (new_hd_flag == h_flag)
    return;

  if (server_changes)
    *server_changes = true;

  /* Local changes have priority */
  if (local_changes == 0)
    mutt_set_flag(m, e, flag_name, new_hd_flag, true);
}

#ifdef USE_HCACHE
/**
 * read_headers_normal_eval_cache - Retrieve data from the header cache
 * @param adata              Imap Account data
 * @param msn_end            Last Message Sequence number
 * @param uid_next           UID of next email
 * @param store_flag_updates if true, save flags to the header cache
 * @param eval_condstore     if true, use CONDSTORE to fetch flags
 * @retval  0 Success
 * @retval -1 Error
 *
 * Without CONDSTORE or QRESYNC, we need to query all the current
 * UIDs and update their flag state and current MSN.
 *
 * For CONDSTORE, we still need to grab the existing UIDs and
 * their MSN.  The current flag state will be queried in
 * read_headers_condstore_qresync_updates().
 */
static int read_headers_normal_eval_cache(struct ImapAccountData *adata,
                                          unsigned int msn_end, unsigned int uid_next,
                                          bool store_flag_updates, bool eval_condstore)
{
  struct Progress *progress = NULL;
  char buf[1024] = { 0 };
  int rc = -1;

  struct Mailbox *m = adata->mailbox;
  struct ImapMboxData *mdata = imap_mdata_get(m);
  int idx = m->msg_count;

  if (m->verbose)
  {
    /* L10N: Comparing the cached data with the IMAP server's data */
    progress = progress_new(_("Evaluating cache..."), MUTT_PROGRESS_READ, msn_end);
  }

  /* If we are using CONDSTORE's "FETCH CHANGEDSINCE", then we keep
   * the flags in the header cache, and update them further below.
   * Otherwise, we fetch the current state of the flags here. */
  snprintf(buf, sizeof(buf), "UID FETCH 1:%u (UID%s)", uid_next - 1,
           eval_condstore ? "" : " FLAGS");

  imap_cmd_start(adata, buf);

  rc = IMAP_RES_CONTINUE;
  int mfhrc = 0;
  struct ImapHeader h;
  for (int msgno = 1; rc == IMAP_RES_CONTINUE; msgno++)
  {
    if (SigInt && query_abort_header_download(adata))
      goto fail;

    if (m->verbose)
      progress_update(progress, msgno, -1);

    memset(&h, 0, sizeof(h));
    h.edata = imap_edata_new();
    do
    {
      rc = imap_cmd_step(adata);
      if (rc != IMAP_RES_CONTINUE)
        break;

      mfhrc = msg_fetch_header(m, &h, adata->buf, NULL);
      if (mfhrc < 0)
        continue;

      if (!h.edata->uid)
      {
        mutt_debug(LL_DEBUG2, "skipping hcache FETCH response for message number %d missing a UID\n",
                   h.edata->msn);
        continue;
      }

      if ((h.edata->msn < 1) || (h.edata->msn > msn_end))
      {
        mutt_debug(LL_DEBUG1, "skipping hcache FETCH response for unknown message number %d\n",
                   h.edata->msn);
        continue;
      }

      if (imap_msn_get(&mdata->msn, h.edata->msn - 1))
      {
        mutt_debug(LL_DEBUG2, "skipping hcache FETCH for duplicate message %d\n",
                   h.edata->msn);
        continue;
      }

      struct Email *e = imap_hcache_get(mdata, h.edata->uid);
      m->emails[idx] = e;
      if (e)
      {
        imap_msn_set(&mdata->msn, h.edata->msn - 1, e);
        mutt_hash_int_insert(mdata->uid_hash, h.edata->uid, e);

        e->index = h.edata->uid;
        /* messages which have not been expunged are ACTIVE (borrowed from mh
         * folders) */
        e->active = true;
        e->changed = false;
        if (eval_condstore)
        {
          h.edata->read = e->read;
          h.edata->old = e->old;
          h.edata->deleted = e->deleted;
          h.edata->flagged = e->flagged;
          h.edata->replied = e->replied;
        }
        else
        {
          e->read = h.edata->read;
          e->old = h.edata->old;
          e->deleted = h.edata->deleted;
          e->flagged = h.edata->flagged;
          e->replied = h.edata->replied;
        }

        /*  mailbox->emails[msgno]->received is restored from mutt_hcache_restore */
        e->edata = h.edata;
        e->edata_free = imap_edata_free;

        /* We take a copy of the tags so we can split the string */
        char *tags_copy = mutt_str_dup(h.edata->flags_remote);
        driver_tags_replace(&e->tags, tags_copy);
        FREE(&tags_copy);

        m->msg_count++;
        mailbox_size_add(m, e);

        /* If this is the first time we are fetching, we need to
         * store the current state of flags back into the header cache */
        if (!eval_condstore && store_flag_updates)
          imap_hcache_put(mdata, e);

        h.edata = NULL;
        idx++;
      }
    } while (mfhrc == -1);

    imap_edata_free((void **) &h.edata);

    if ((mfhrc < -1) || ((rc != IMAP_RES_CONTINUE) && (rc != IMAP_RES_OK)))
      goto fail;
  }

  rc = 0;
fail:
  progress_free(&progress);
  return rc;
}

/**
 * read_headers_qresync_eval_cache - Retrieve data from the header cache
 * @param adata Imap Account data
 * @param uid_seqset Sequence Set of UIDs
 * @retval >=0 Success
 * @retval  -1 Error
 *
 * For QRESYNC, we grab the UIDs in order by MSN from the header cache.
 *
 * In read_headers_condstore_qresync_updates().  We will update change flags
 * using CHANGEDSINCE and find out what UIDs have been expunged using VANISHED.
 */
static int read_headers_qresync_eval_cache(struct ImapAccountData *adata, char *uid_seqset)
{
  int rc;
  unsigned int uid = 0;

  mutt_debug(LL_DEBUG2, "Reading uid seqset from header cache\n");
  struct Mailbox *m = adata->mailbox;
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  unsigned int msn = 1;

  if (m->verbose)
    mutt_message(_("Evaluating cache..."));

  struct SeqsetIterator *iter = mutt_seqset_iterator_new(uid_seqset);
  if (!iter)
    return -1;

  while ((rc = mutt_seqset_iterator_next(iter, &uid)) == 0)
  {
    /* The seqset may contain more headers than the fetch request, so
     * we need to watch and reallocate the context and msn_index */
    imap_msn_reserve(&mdata->msn, msn);

    struct Email *e = imap_hcache_get(mdata, uid);
    if (e)
    {
      imap_msn_set(&mdata->msn, msn - 1, e);

      mx_alloc_memory(m, m->msg_count);

      struct ImapEmailData *edata = imap_edata_new();
      e->edata = edata;
      e->edata_free = imap_edata_free;

      e->index = uid;
      e->active = true;
      e->changed = false;
      edata->read = e->read;
      edata->old = e->old;
      edata->deleted = e->deleted;
      edata->flagged = e->flagged;
      edata->replied = e->replied;

      edata->msn = msn;
      edata->uid = uid;
      mutt_hash_int_insert(mdata->uid_hash, uid, e);

      mailbox_size_add(m, e);
      m->emails[m->msg_count++] = e;

      msn++;
    }
    else if (!uid)
    {
      /* A non-zero uid missing from the header cache is either the
       * result of an expunged message (not recorded in the uid seqset)
       * or a hole in the header cache.
       *
       * We have to assume it's an earlier expunge and compact the msn's
       * in that case, because cmd_parse_vanished() won't find it in the
       * uid_hash and decrement later msn's there.
       *
       * Thus we only increment the uid if the uid was 0: an actual
       * stored "blank" in the uid seqset.
       */
      msn++;
    }
  }

  mutt_seqset_iterator_free(&iter);

  return rc;
}

/**
 * read_headers_condstore_qresync_updates - Retrieve updates from the server
 * @param adata        Imap Account data
 * @param msn_end      Last Message Sequence number
 * @param uid_next     UID of next email
 * @param hc_modseq    Timestamp of last Header Cache update
 * @param eval_qresync If true, use QRESYNC
 * @retval  0 Success
 * @retval -1 Error
 *
 * CONDSTORE and QRESYNC use FETCH extensions to grab updates.
 */
static int read_headers_condstore_qresync_updates(struct ImapAccountData *adata,
                                                  unsigned int msn_end, unsigned int uid_next,
                                                  unsigned long long hc_modseq, bool eval_qresync)
{
  struct Progress *progress = NULL;
  char buf[1024] = { 0 };
  unsigned int header_msn = 0;

  struct Mailbox *m = adata->mailbox;
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (m->verbose)
  {
    /* L10N: Fetching IMAP flag changes, using the CONDSTORE extension */
    progress = progress_new(_("Fetching flag updates..."), MUTT_PROGRESS_READ, msn_end);
  }

  snprintf(buf, sizeof(buf), "UID FETCH 1:%u (FLAGS) (CHANGEDSINCE %llu%s)",
           uid_next - 1, hc_modseq, eval_qresync ? " VANISHED" : "");

  imap_cmd_start(adata, buf);

  int rc = IMAP_RES_CONTINUE;
  for (int msgno = 1; rc == IMAP_RES_CONTINUE; msgno++)
  {
    if (SigInt && query_abort_header_download(adata))
      goto fail;

    if (m->verbose)
      progress_update(progress, msgno, -1);

    /* cmd_parse_fetch will update the flags */
    rc = imap_cmd_step(adata);
    if (rc != IMAP_RES_CONTINUE)
      break;

    /* so we just need to grab the header and persist it back into
     * the header cache */
    char *fetch_buf = adata->buf;
    if (fetch_buf[0] != '*')
      continue;

    fetch_buf = imap_next_word(fetch_buf);
    if (!isdigit((unsigned char) *fetch_buf) || !mutt_str_atoui(fetch_buf, &header_msn))
      continue;

    if ((header_msn < 1) || (header_msn > msn_end) ||
        !imap_msn_get(&mdata->msn, header_msn - 1))
    {
      mutt_debug(LL_DEBUG1, "skipping CONDSTORE flag update for unknown message number %u\n",
                 header_msn);
      continue;
    }

    imap_hcache_put(mdata, imap_msn_get(&mdata->msn, header_msn - 1));
  }

  if (rc != IMAP_RES_OK)
    goto fail;

  /* The IMAP flag setting as part of cmd_parse_fetch() ends up
   * flipping these on. */
  mdata->check_status &= ~IMAP_FLAGS_PENDING;
  m->changed = false;

  /* VANISHED handling: we need to empty out the messages */
  if (mdata->reopen & IMAP_EXPUNGE_PENDING)
  {
    imap_hcache_close(mdata);
    imap_expunge_mailbox(m, false);

    imap_hcache_open(adata, mdata);
    mdata->reopen &= ~IMAP_EXPUNGE_PENDING;
  }

  /* undo expunge count updates.
   * mview_update() will do this at the end of the header fetch. */
  m->vcount = 0;
  m->msg_tagged = 0;
  m->msg_deleted = 0;
  m->msg_new = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->changed = false;

  rc = 0;
fail:
  progress_free(&progress);
  return rc;
}

/**
 * imap_verify_qresync - Check to see if QRESYNC got jumbled
 * @param m  Imap Selected Mailbox
 * @retval  0 Success
 * @retval -1 Error
 *
 * If so, wipe the context and try again with a normal download.
 */
static int imap_verify_qresync(struct Mailbox *m)
{
  assert(m);
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

  const size_t max_msn = imap_msn_highest(&mdata->msn);

  unsigned int msn;
  unsigned int uid;
  struct Email *e = NULL;
  struct Email *uidh = NULL;

  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    const struct ImapEmailData *edata = imap_edata_get(e);
    if (!edata)
      goto fail;

    msn = imap_edata_get(e)->msn;
    uid = imap_edata_get(e)->uid;

    if ((msn < 1) || (msn > max_msn) || imap_msn_get(&mdata->msn, msn - 1) != e)
      goto fail;

    uidh = (struct Email *) mutt_hash_int_find(mdata->uid_hash, uid);
    if (uidh != e)
      goto fail;
  }

  return 0;

fail:
  imap_msn_free(&mdata->msn);
  mutt_hash_free(&mdata->uid_hash);
  mutt_hash_free(&m->subj_hash);
  mutt_hash_free(&m->id_hash);
  mutt_hash_free(&m->label_hash);

  for (int i = 0; i < m->msg_count; i++)
  {
    if (m->emails[i] && m->emails[i]->edata)
      imap_edata_free(&m->emails[i]->edata);
    email_free(&m->emails[i]);
  }
  m->msg_count = 0;
  m->size = 0;
  mutt_hcache_delete_record(mdata->hcache, "/MODSEQ", 7);
  imap_hcache_clear_uid_seqset(mdata);
  imap_hcache_close(mdata);

  if (m->verbose)
  {
    /* L10N: After opening an IMAP mailbox using QRESYNC, Mutt performs a quick
       sanity check.  If that fails, Mutt reopens the mailbox using a normal
       download.  */
    mutt_error(_("QRESYNC failed.  Reopening mailbox."));
  }
  return -1;
}

#endif /* USE_HCACHE */

/**
 * read_headers_fetch_new - Retrieve new messages from the server
 * @param[in]  m                Imap Selected Mailbox
 * @param[in]  msn_begin        First Message Sequence number
 * @param[in]  msn_end          Last Message Sequence number
 * @param[in]  evalhc           If true, check the Header Cache
 * @param[out] maxuid           Highest UID seen
 * @param[in]  initial_download true, if this is the first opening of the mailbox
 * @retval  0 Success
 * @retval -1 Error
 */
static int read_headers_fetch_new(struct Mailbox *m, unsigned int msn_begin,
                                  unsigned int msn_end, bool evalhc,
                                  unsigned int *maxuid, bool initial_download)
{
  int rc = -1;
  unsigned int fetch_msn_end = 0;
  struct Progress *progress = NULL;
  char *hdrreq = NULL;
  struct Buffer *tempfile = NULL;
  FILE *fp = NULL;
  struct ImapHeader h;
  struct Buffer *buf = NULL;
  static const char *const want_headers = "DATE FROM SENDER SUBJECT TO CC MESSAGE-ID REFERENCES "
                                          "CONTENT-TYPE CONTENT-DESCRIPTION IN-REPLY-TO REPLY-TO "
                                          "LINES LIST-POST LIST-SUBSCRIBE LIST-UNSUBSCRIBE X-LABEL "
                                          "X-ORIGINAL-TO";

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!adata || (adata->mailbox != m))
    return -1;

  struct Buffer *hdr_list = buf_pool_get();
  buf_strcpy(hdr_list, want_headers);
  const char *const c_imap_headers = cs_subset_string(NeoMutt->sub, "imap_headers");
  if (c_imap_headers)
  {
    buf_addch(hdr_list, ' ');
    buf_addstr(hdr_list, c_imap_headers);
  }
#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  if (c_autocrypt)
  {
    buf_addch(hdr_list, ' ');
    buf_addstr(hdr_list, "AUTOCRYPT");
  }
#endif

  if (adata->capabilities & IMAP_CAP_IMAP4REV1)
  {
    mutt_str_asprintf(&hdrreq, "BODY.PEEK[HEADER.FIELDS (%s)]", buf_string(hdr_list));
  }
  else if (adata->capabilities & IMAP_CAP_IMAP4)
  {
    mutt_str_asprintf(&hdrreq, "RFC822.HEADER.LINES (%s)", buf_string(hdr_list));
  }
  else
  { /* Unable to fetch headers for lower versions */
    mutt_error(_("Unable to fetch headers from this IMAP server version"));
    goto bail;
  }

  buf_pool_release(&hdr_list);

  /* instead of downloading all headers and then parsing them, we parse them
   * as they come in. */
  tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  fp = mutt_file_fopen(buf_string(tempfile), "w+");
  if (!fp)
  {
    mutt_error(_("Could not create temporary file %s"), buf_string(tempfile));
    goto bail;
  }
  unlink(buf_string(tempfile));
  buf_pool_release(&tempfile);

  if (m->verbose)
  {
    progress = progress_new(_("Fetching message headers..."), MUTT_PROGRESS_READ, msn_end);
  }

  buf = buf_pool_get();

  /* NOTE:
   *   The (fetch_msn_end < msn_end) used to be important to prevent
   *   an infinite loop, in the event the server did not return all
   *   the headers (due to a pending expunge, for example).
   *
   *   I believe the new chunking imap_fetch_msn_seqset()
   *   implementation and "msn_begin = fetch_msn_end + 1" assignment
   *   at the end of the loop makes the comparison unneeded, but to be
   *   cautious I'm keeping it.
   */
  struct ImapEmailData *edata = imap_edata_new();
  while ((fetch_msn_end < msn_end) &&
         imap_fetch_msn_seqset(buf, adata, evalhc, msn_begin, msn_end, &fetch_msn_end))
  {
    char *cmd = NULL;
    mutt_str_asprintf(&cmd, "FETCH %s (UID FLAGS INTERNALDATE RFC822.SIZE %s)",
                      buf_string(buf), hdrreq);
    imap_cmd_start(adata, cmd);
    FREE(&cmd);

    int msgno = msn_begin;

    while (true)
    {
      rewind(fp);
      memset(&h, 0, sizeof(h));
      h.edata = edata;

      if (initial_download && SigInt && query_abort_header_download(adata))
      {
        goto bail;
      }

      const int rc2 = imap_cmd_step(adata);
      if (rc2 != IMAP_RES_CONTINUE)
      {
        if (rc2 != IMAP_RES_OK)
        {
          goto bail;
        }
        break;
      }

      switch (msg_fetch_header(m, &h, adata->buf, fp))
      {
        case 0:
          break;
        case -1:
          continue;
        case -2:
          goto bail;
      }

      if (!ftello(fp))
      {
        mutt_debug(LL_DEBUG2, "ignoring fetch response with no body\n");
        continue;
      }

      /* make sure we don't get remnants from older larger message headers */
      fputs("\n\n", fp);

      if ((h.edata->msn < 1) || (h.edata->msn > fetch_msn_end))
      {
        mutt_debug(LL_DEBUG1, "skipping FETCH response for unknown message number %d\n",
                   h.edata->msn);
        continue;
      }

      /* May receive FLAGS updates in a separate untagged response */
      if (imap_msn_get(&mdata->msn, h.edata->msn - 1))
      {
        mutt_debug(LL_DEBUG2, "skipping FETCH response for duplicate message %d\n",
                   h.edata->msn);
        continue;
      }

      if (m->verbose)
      {
        progress_update(progress, msgno++, -1);
      }

      struct Email *e = email_new();
      mx_alloc_memory(m, m->msg_count);

      m->emails[m->msg_count++] = e;

      imap_msn_set(&mdata->msn, h.edata->msn - 1, e);
      mutt_hash_int_insert(mdata->uid_hash, h.edata->uid, e);

      e->index = h.edata->uid;
      /* messages which have not been expunged are ACTIVE (borrowed from mh
       * folders) */
      e->active = true;
      e->changed = false;
      e->read = h.edata->read;
      e->old = h.edata->old;
      e->deleted = h.edata->deleted;
      e->flagged = h.edata->flagged;
      e->replied = h.edata->replied;
      e->received = h.received;
      e->edata = (void *) imap_edata_clone(h.edata);
      e->edata_free = imap_edata_free;
      STAILQ_INIT(&e->tags);

      /* We take a copy of the tags so we can split the string */
      char *tags_copy = mutt_str_dup(h.edata->flags_remote);
      driver_tags_replace(&e->tags, tags_copy);
      FREE(&tags_copy);

      if (*maxuid < h.edata->uid)
        *maxuid = h.edata->uid;

      rewind(fp);
      /* NOTE: if Date: header is missing, mutt_rfc822_read_header depends
       *   on h.received being set */
      e->env = mutt_rfc822_read_header(fp, e, false, false);
      /* body built as a side-effect of mutt_rfc822_read_header */
      e->body->length = h.content_length;
      mailbox_size_add(m, e);

#ifdef USE_HCACHE
      imap_hcache_put(mdata, e);
#endif /* USE_HCACHE */
    }

    /* In case we get new mail while fetching the headers. */
    if (mdata->reopen & IMAP_NEWMAIL_PENDING)
    {
      msn_end = mdata->new_mail_count;
      mx_alloc_memory(m, msn_end);
      imap_msn_reserve(&mdata->msn, msn_end);
      mdata->reopen &= ~IMAP_NEWMAIL_PENDING;
      mdata->new_mail_count = 0;
    }

    /* Note: RFC3501 section 7.4.1 and RFC7162 section 3.2.10.2 say we
     * must not get any EXPUNGE/VANISHED responses in the middle of a
     * FETCH, nor when no command is in progress (e.g. between the
     * chunked FETCH commands).  We previously tried to be robust by
     * setting:
     *   msn_begin = mdata->max_msn + 1;
     * but with chunking and header cache holes this
     * may not be correct.  So here we must assume the msn values have
     * not been altered during or after the fetch.  */
    msn_begin = fetch_msn_end + 1;
  }

  rc = 0;

bail:
  buf_pool_release(&hdr_list);
  buf_pool_release(&buf);
  buf_pool_release(&tempfile);
  mutt_file_fclose(&fp);
  FREE(&hdrreq);
  imap_edata_free((void **) &edata);
  progress_free(&progress);

  return rc;
}

/**
 * imap_read_headers - Read headers from the server
 * @param m                Imap Selected Mailbox
 * @param msn_begin        First Message Sequence Number
 * @param msn_end          Last Message Sequence Number
 * @param initial_download true, if this is the first opening of the mailbox
 * @retval num Last MSN
 * @retval -1  Failure
 *
 * Changed to read many headers instead of just one. It will return the msn of
 * the last message read. It will return a value other than msn_end if mail
 * comes in while downloading headers (in theory).
 */
int imap_read_headers(struct Mailbox *m, unsigned int msn_begin,
                      unsigned int msn_end, bool initial_download)
{
  unsigned int maxuid = 0;
  int rc = -1;
  bool evalhc = false;

#ifdef USE_HCACHE
  uint32_t uidvalidity = 0;
  unsigned int uid_next = 0;
  unsigned long long modseq = 0;
  bool has_condstore = false;
  bool has_qresync = false;
  bool eval_condstore = false;
  bool eval_qresync = false;
  char *uid_seqset = NULL;
  const unsigned int msn_begin_save = msn_begin;
#endif /* USE_HCACHE */

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || (adata->mailbox != m))
    return -1;

#ifdef USE_HCACHE
retry:
#endif /* USE_HCACHE */

  /* make sure context has room to hold the mailbox */
  mx_alloc_memory(m, msn_end);
  imap_msn_reserve(&mdata->msn, msn_end);
  imap_alloc_uid_hash(adata, msn_end);

  mdata->reopen &= ~(IMAP_REOPEN_ALLOW | IMAP_NEWMAIL_PENDING);
  mdata->new_mail_count = 0;

#ifdef USE_HCACHE
  imap_hcache_open(adata, mdata);

  if (mdata->hcache && initial_download)
  {
    mutt_hcache_fetch_obj(mdata->hcache, "/UIDVALIDITY", 12, &uidvalidity);
    mutt_hcache_fetch_obj(mdata->hcache, "/UIDNEXT", 8, &uid_next);
    if (mdata->modseq)
    {
      const bool c_imap_condstore = cs_subset_bool(NeoMutt->sub, "imap_condstore");
      if ((adata->capabilities & IMAP_CAP_CONDSTORE) && c_imap_condstore)
        has_condstore = true;

      /* If IMAP_CAP_QRESYNC and ImapQResync then NeoMutt sends ENABLE QRESYNC.
       * If we receive an ENABLED response back, then adata->qresync is set.  */
      if (adata->qresync)
        has_qresync = true;
    }

    if (uidvalidity && uid_next && uidvalidity == mdata->uidvalidity)
    {
      evalhc = true;
      if (mutt_hcache_fetch_obj(mdata->hcache, "/MODSEQ", 7, &modseq))
      {
        if (has_qresync)
        {
          uid_seqset = imap_hcache_get_uid_seqset(mdata);
          if (uid_seqset)
            eval_qresync = true;
        }

        if (!eval_qresync && has_condstore)
          eval_condstore = true;
      }
    }
  }
  if (evalhc)
  {
    if (eval_qresync)
    {
      if (read_headers_qresync_eval_cache(adata, uid_seqset) < 0)
        goto bail;
    }
    else
    {
      if (read_headers_normal_eval_cache(adata, msn_end, uid_next, has_condstore || has_qresync,
                                         eval_condstore) < 0)
        goto bail;
    }

    if ((eval_condstore || eval_qresync) && (modseq != mdata->modseq))
    {
      if (read_headers_condstore_qresync_updates(adata, msn_end, uid_next,
                                                 modseq, eval_qresync) < 0)
      {
        goto bail;
      }
    }

    /* Look for the first empty MSN and start there */
    while (msn_begin <= msn_end)
    {
      if (!imap_msn_get(&mdata->msn, msn_begin - 1))
        break;
      msn_begin++;
    }
  }
#endif /* USE_HCACHE */

  if (read_headers_fetch_new(m, msn_begin, msn_end, evalhc, &maxuid, initial_download) < 0)
    goto bail;

#ifdef USE_HCACHE
  if (eval_qresync && initial_download)
  {
    if (imap_verify_qresync(m) != 0)
    {
      eval_qresync = false;
      eval_condstore = false;
      evalhc = false;
      modseq = 0;
      maxuid = 0;
      FREE(&uid_seqset);
      uidvalidity = 0;
      uid_next = 0;
      msn_begin = msn_begin_save;

      goto retry;
    }
  }
#endif /* USE_HCACHE */

  if (maxuid && (mdata->uid_next < maxuid + 1))
    mdata->uid_next = maxuid + 1;

#ifdef USE_HCACHE
  mutt_hcache_store_raw(mdata->hcache, "/UIDVALIDITY", 12, &mdata->uidvalidity,
                        sizeof(mdata->uidvalidity));
  if (maxuid && (mdata->uid_next < maxuid + 1))
  {
    mutt_debug(LL_DEBUG2, "Overriding UIDNEXT: %u -> %u\n", mdata->uid_next, maxuid + 1);
    mdata->uid_next = maxuid + 1;
  }
  if (mdata->uid_next > 1)
  {
    mutt_hcache_store_raw(mdata->hcache, "/UIDNEXT", 8, &mdata->uid_next,
                          sizeof(mdata->uid_next));
  }

  /* We currently only sync CONDSTORE and QRESYNC on the initial download.
   * To do it more often, we'll need to deal with flag updates combined with
   * unsync'ed local flag changes.  We'll also need to properly sync flags to
   * the header cache on close.  I'm not sure it's worth the added complexity.  */
  if (initial_download)
  {
    if (has_condstore || has_qresync)
    {
      mutt_hcache_store_raw(mdata->hcache, "/MODSEQ", 7, &mdata->modseq,
                            sizeof(mdata->modseq));
    }
    else
    {
      mutt_hcache_delete_record(mdata->hcache, "/MODSEQ", 7);
    }

    if (has_qresync)
      imap_hcache_store_uid_seqset(mdata);
    else
      imap_hcache_clear_uid_seqset(mdata);
  }
#endif /* USE_HCACHE */

  /* TODO: it's not clear to me why we are calling mx_alloc_memory yet again. */
  mx_alloc_memory(m, m->msg_count);

  mdata->reopen |= IMAP_REOPEN_ALLOW;

  rc = msn_end;

bail:
#ifdef USE_HCACHE
  imap_hcache_close(mdata);
  FREE(&uid_seqset);
#endif /* USE_HCACHE */

  return rc;
}

/**
 * imap_append_message - Write an email back to the server
 * @param m   Mailbox
 * @param msg Message to save
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_append_message(struct Mailbox *m, struct Message *msg)
{
  if (!m || !msg)
    return -1;

  FILE *fp = NULL;
  char buf[2048];
  char internaldate[IMAP_DATELEN] = { 0 };
  char imap_flags[128] = { 0 };
  size_t len;
  struct Progress *progress = NULL;
  size_t sent;
  int c, last;
  int rc;

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

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
   * Ideally we'd have an Email structure with flag info here... */
  for (last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if ((c == '\n') && (last != '\r'))
      len++;

    len++;
  }
  rewind(fp);

  if (m->verbose)
    progress = progress_new(_("Uploading message..."), MUTT_PROGRESS_NET, len);

  mutt_date_make_imap(internaldate, sizeof(internaldate), msg->received);

  imap_flags[0] = '\0';
  imap_flags[1] = '\0';

  if (msg->flags.read)
    mutt_str_cat(imap_flags, sizeof(imap_flags), " \\Seen");
  if (msg->flags.replied)
    mutt_str_cat(imap_flags, sizeof(imap_flags), " \\Answered");
  if (msg->flags.flagged)
    mutt_str_cat(imap_flags, sizeof(imap_flags), " \\Flagged");
  if (msg->flags.draft)
    mutt_str_cat(imap_flags, sizeof(imap_flags), " \\Draft");

  snprintf(buf, sizeof(buf), "APPEND %s (%s) \"%s\" {%lu}", mdata->munge_name,
           imap_flags + 1, internaldate, (unsigned long) len);

  imap_cmd_start(adata, buf);

  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

  if (rc != IMAP_RES_RESPOND)
    goto cmd_step_fail;

  for (last = EOF, sent = len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if ((c == '\n') && (last != '\r'))
      buf[len++] = '\r';

    buf[len++] = c;

    if (len > sizeof(buf) - 3)
    {
      sent += len;
      if (flush_buffer(buf, &len, adata->conn) < 0)
        goto fail;
      if (m->verbose)
        progress_update(progress, sent, -1);
    }
  }

  if (len)
    if (flush_buffer(buf, &len, adata->conn) < 0)
      goto fail;

  if (mutt_socket_send(adata->conn, "\r\n") < 0)
    goto fail;
  mutt_file_fclose(&fp);

  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

  if (rc != IMAP_RES_OK)
    goto cmd_step_fail;

  progress_free(&progress);
  return 0;

cmd_step_fail:
  mutt_debug(LL_DEBUG1, "command failed: %s\n", adata->buf);
  if (rc != IMAP_RES_BAD)
  {
    char *pc = imap_next_word(adata->buf); /* skip sequence number or token */
    pc = imap_next_word(pc);               /* skip response code */
    if (*pc != '\0')
      mutt_error("%s", pc);
  }

fail:
  mutt_file_fclose(&fp);
  progress_free(&progress);
  return -1;
}

/**
 * emails_to_uid_array - Extract IMAP UIDs from Emails
 * @param ea   Array of Emails
 * @param uida Empty UID array
 * @retval num Number of UIDs in the array
 */
static int emails_to_uid_array(struct EmailArray *ea, struct UidArray *uida)
{
  struct Email **ep = NULL;
  ARRAY_FOREACH(ep, ea)
  {
    struct Email *e = *ep;
    struct ImapEmailData *edata = imap_edata_get(e);

    ARRAY_ADD(uida, edata->uid);
  }
  ARRAY_SORT(uida, imap_sort_uid);

  return ARRAY_SIZE(uida);
}

/**
 * imap_copy_messages - Server COPY messages to another folder
 * @param m        Mailbox
 * @param ea       Array of Emails to copy
 * @param dest     Destination folder
 * @param save_opt Copy or move, e.g. #SAVE_MOVE
 * @retval -1 Error
 * @retval  0 Success
 * @retval  1 Non-fatal error - try fetch/append
 */
int imap_copy_messages(struct Mailbox *m, struct EmailArray *ea,
                       const char *dest, enum MessageSaveOpt save_opt)
{
  if (!m || !ea || ARRAY_EMPTY(ea) || !dest)
    return -1;

  struct Buffer cmd, sync_cmd;
  char buf[PATH_MAX] = { 0 };
  char mbox[PATH_MAX] = { 0 };
  char mmbox[PATH_MAX] = { 0 };
  char prompt[PATH_MAX + 64];
  int rc;
  struct ConnAccount cac = { { 0 } };
  enum QuadOption err_continue = MUTT_NO;
  int triedcreate = 0;
  struct Email *e_cur = *ARRAY_GET(ea, 0);
  bool single = (ARRAY_SIZE(ea) == 1);
  struct ImapAccountData *adata = imap_adata_get(m);

  if (single && e_cur->attach_del)
  {
    mutt_debug(LL_DEBUG3, "#1 Message contains attachments to be deleted\n");
    return 1;
  }

  if (imap_parse_path(dest, &cac, buf, sizeof(buf)))
  {
    mutt_debug(LL_DEBUG1, "bad destination %s\n", dest);
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (!imap_account_match(&adata->conn->account, &cac))
  {
    mutt_debug(LL_DEBUG3, "%s not same server as %s\n", dest, mailbox_path(m));
    return 1;
  }

  imap_fix_path(adata->delim, buf, mbox, sizeof(mbox));
  if (*mbox == '\0')
    mutt_str_copy(mbox, "INBOX", sizeof(mbox));
  imap_munge_mbox_name(adata->unicode, mmbox, sizeof(mmbox), mbox);

  /* loop in case of TRYCREATE */
  do
  {
    buf_init(&sync_cmd);
    buf_init(&cmd);

    if (single)
    {
      mutt_message(_("Copying message %d to %s..."), e_cur->index + 1, mbox);
      buf_add_printf(&cmd, "UID COPY %u %s", imap_edata_get(e_cur)->uid, mmbox);

      if (e_cur->active && e_cur->changed)
      {
        rc = imap_sync_message_for_copy(m, e_cur, &sync_cmd, &err_continue);
        if (rc < 0)
        {
          mutt_debug(LL_DEBUG1, "#2 could not sync\n");
          goto out;
        }
      }
      rc = imap_exec(adata, cmd.data, IMAP_CMD_QUEUE);
      if (rc != IMAP_EXEC_SUCCESS)
      {
        mutt_debug(LL_DEBUG1, "#2 could not queue copy\n");
        goto out;
      }
    }
    else /* copy tagged messages */
    {
      /* if any messages have attachments to delete, fall through to FETCH
       * and APPEND. TODO: Copy what we can with COPY, fall through for the
       * remainder. */
      struct Email **ep = NULL;
      ARRAY_FOREACH(ep, ea)
      {
        struct Email *e = *ep;
        if (e->attach_del)
        {
          mutt_debug(LL_DEBUG3, "#2 Message contains attachments to be deleted\n");
          return 1;
        }

        if (e->active && e->changed)
        {
          rc = imap_sync_message_for_copy(m, e, &sync_cmd, &err_continue);
          if (rc < 0)
          {
            mutt_debug(LL_DEBUG1, "#1 could not sync\n");
            goto out;
          }
        }
      }

      struct UidArray uida = ARRAY_HEAD_INITIALIZER;
      emails_to_uid_array(ea, &uida);
      rc = imap_exec_msg_set(adata, "UID COPY", mmbox, &uida);
      ARRAY_FREE(&uida);

      if (rc == 0)
      {
        mutt_debug(LL_DEBUG1, "No messages tagged\n");
        rc = -1;
        goto out;
      }
      else if (rc < 0)
      {
        mutt_debug(LL_DEBUG1, "#1 could not queue copy\n");
        goto out;
      }
      else
      {
        mutt_message(ngettext("Copying %d message to %s...", "Copying %d messages to %s...", rc),
                     rc, mbox);
      }
    }

    /* let's get it on */
    rc = imap_exec(adata, NULL, IMAP_CMD_NO_FLAGS);
    if (rc == IMAP_EXEC_ERROR)
    {
      if (triedcreate)
      {
        mutt_debug(LL_DEBUG1, "Already tried to create mailbox %s\n", mbox);
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (!mutt_istr_startswith(imap_get_qualifier(adata->buf), "[TRYCREATE]"))
        break;
      mutt_debug(LL_DEBUG3, "server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), mbox);
      const bool c_confirm_create = cs_subset_bool(NeoMutt->sub, "confirm_create");
      if (c_confirm_create && (mutt_yesorno(prompt, MUTT_YES) != MUTT_YES))
      {
        mutt_clear_error();
        goto out;
      }
      if (imap_create_mailbox(adata, mbox) < 0)
        break;
      triedcreate = 1;
    }
  } while (rc == IMAP_EXEC_ERROR);

  if (rc != 0)
  {
    imap_error("imap_copy_messages", adata->buf);
    goto out;
  }

  /* cleanup */
  if (save_opt == SAVE_MOVE)
  {
    struct Email **ep = NULL;
    ARRAY_FOREACH(ep, ea)
    {
      struct Email *e = *ep;
      mutt_set_flag(m, e, MUTT_DELETE, true, true);
      mutt_set_flag(m, e, MUTT_PURGE, true, true);
    }
  }

  rc = 0;

out:
  FREE(&cmd.data);
  FREE(&sync_cmd.data);

  return (rc < 0) ? -1 : rc;
}

/**
 * imap_cache_del - Delete an email from the body cache
 * @param m     Selected Imap Mailbox
 * @param e     Email
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_cache_del(struct Mailbox *m, struct Email *e)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!e || !adata || (adata->mailbox != m))
    return -1;

  mdata->bcache = msg_cache_open(m);
  char id[64] = { 0 };
  snprintf(id, sizeof(id), "%u-%u", mdata->uidvalidity, imap_edata_get(e)->uid);
  return mutt_bcache_del(mdata->bcache, id);
}

/**
 * imap_cache_clean - Delete all the entries in the message cache
 * @param m  SelectedImap Mailbox
 * @retval 0 Always
 */
int imap_cache_clean(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!adata || (adata->mailbox != m))
    return -1;

  mdata->bcache = msg_cache_open(m);
  mutt_bcache_list(mdata->bcache, msg_cache_clean_cb, mdata);

  return 0;
}

/**
 * imap_set_flags - Fill the message header according to the server flags
 * @param[in]  m              Imap Selected Mailbox
 * @param[in]  e              Email
 * @param[in]  s              Command string
 * @param[out] server_changes Set to true if the flags have changed
 * @retval ptr  The end of flags string
 * @retval NULL Failure
 *
 * Expects a flags line of the form "FLAGS (flag flag ...)"
 *
 * imap_set_flags: fill out the message header according to the flags from
 * the server. Expects a flags line of the form "FLAGS (flag flag ...)"
 *
 * Sets server_changes to 1 if a change to a flag is made, or in the
 * case of e->changed, if a change to a flag _would_ have been
 * made.
 */
char *imap_set_flags(struct Mailbox *m, struct Email *e, char *s, bool *server_changes)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || (adata->mailbox != m))
    return NULL;

  struct ImapHeader newh = { 0 };
  struct ImapEmailData old_edata = { 0 };
  int local_changes = e->changed;

  struct ImapEmailData *edata = e->edata;
  newh.edata = edata;

  mutt_debug(LL_DEBUG2, "parsing FLAGS\n");
  s = msg_parse_flags(&newh, s);
  if (!s)
    return NULL;

  /* Update tags system */
  /* We take a copy of the tags so we can split the string */
  char *tags_copy = mutt_str_dup(edata->flags_remote);
  driver_tags_replace(&e->tags, tags_copy);
  FREE(&tags_copy);

  /* YAUH (yet another ugly hack): temporarily set context to
   * read-write even if it's read-only, so *server* updates of
   * flags can be processed by mutt_set_flag. mailbox->changed must
   * be restored afterwards */
  bool readonly = m->readonly;
  m->readonly = false;

  /* This is redundant with the following two checks. Removing:
   * mutt_set_flag (m, e, MUTT_NEW, !(edata->read || edata->old), true); */
  set_changed_flag(m, e, local_changes, server_changes, MUTT_OLD, old_edata.old,
                   edata->old, e->old);
  set_changed_flag(m, e, local_changes, server_changes, MUTT_READ,
                   old_edata.read, edata->read, e->read);
  set_changed_flag(m, e, local_changes, server_changes, MUTT_DELETE,
                   old_edata.deleted, edata->deleted, e->deleted);
  set_changed_flag(m, e, local_changes, server_changes, MUTT_FLAG,
                   old_edata.flagged, edata->flagged, e->flagged);
  set_changed_flag(m, e, local_changes, server_changes, MUTT_REPLIED,
                   old_edata.replied, edata->replied, e->replied);

  /* this message is now definitively *not* changed (mutt_set_flag
   * marks things changed as a side-effect) */
  if (local_changes == 0)
    e->changed = false;
  m->changed &= !readonly;
  m->readonly = readonly;

  return s;
}

/**
 * imap_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open() - @ingroup mx_msg_open
 */
bool imap_msg_open(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  struct Envelope *newenv = NULL;
  char buf[1024] = { 0 };
  char *pc = NULL;
  unsigned int bytes;
  unsigned int uid;
  bool retried = false;
  bool read;
  int rc;

  /* Sam's weird courier server returns an OK response even when FETCH
   * fails. Thanks Sam. */
  bool fetched = false;

  struct ImapAccountData *adata = imap_adata_get(m);

  if (!adata || (adata->mailbox != m))
    return false;

  msg->fp = msg_cache_get(m, e);
  if (msg->fp)
  {
    if (imap_edata_get(e)->parsed)
      return true;
    goto parsemsg;
  }

  /* This function is called in a few places after endwin()
   * e.g. mutt_pipe_message(). */
  bool output_progress = !isendwin() && m->verbose;
  if (output_progress)
    mutt_message(_("Fetching message..."));

  msg->fp = msg_cache_put(m, e);
  if (!msg->fp)
  {
    struct Buffer *path = buf_pool_get();
    buf_mktemp(path);
    msg->fp = mutt_file_fopen(buf_string(path), "w+");
    unlink(buf_string(path));
    buf_pool_release(&path);

    if (!msg->fp)
      return false;
  }

  /* mark this header as currently inactive so the command handler won't
   * also try to update it. HACK until all this code can be moved into the
   * command handler */
  e->active = false;

  const bool c_imap_peek = cs_subset_bool(NeoMutt->sub, "imap_peek");
  snprintf(buf, sizeof(buf), "UID FETCH %u %s", imap_edata_get(e)->uid,
           ((adata->capabilities & IMAP_CAP_IMAP4REV1) ?
                (c_imap_peek ? "BODY.PEEK[]" : "BODY[]") :
                "RFC822"));

  imap_cmd_start(adata, buf);
  do
  {
    rc = imap_cmd_step(adata);
    if (rc != IMAP_RES_CONTINUE)
      break;

    pc = adata->buf;
    pc = imap_next_word(pc);
    pc = imap_next_word(pc);

    if (mutt_istr_startswith(pc, "FETCH"))
    {
      while (*pc)
      {
        pc = imap_next_word(pc);
        if (pc[0] == '(')
          pc++;
        if (mutt_istr_startswith(pc, "UID"))
        {
          pc = imap_next_word(pc);
          if (!mutt_str_atoui(pc, &uid))
            goto bail;
          if (uid != imap_edata_get(e)->uid)
          {
            mutt_error(_("The message index is incorrect. Try reopening the mailbox."));
          }
        }
        else if (mutt_istr_startswith(pc, "RFC822") || mutt_istr_startswith(pc, "BODY[]"))
        {
          pc = imap_next_word(pc);
          if (imap_get_literal_count(pc, &bytes) < 0)
          {
            imap_error("imap_msg_open()", buf);
            goto bail;
          }

          const int res = imap_read_literal(msg->fp, adata, bytes, NULL);
          if (res < 0)
          {
            goto bail;
          }
          /* pick up trailing line */
          rc = imap_cmd_step(adata);
          if (rc != IMAP_RES_CONTINUE)
            goto bail;
          pc = adata->buf;

          fetched = true;
        }
        else if (!e->changed && mutt_istr_startswith(pc, "FLAGS"))
        {
          /* UW-IMAP will provide a FLAGS update here if the FETCH causes a
           * change (eg from \Unseen to \Seen).
           * Uncommitted changes in neomutt take precedence. If we decide to
           * incrementally update flags later, this won't stop us syncing */
          pc = imap_set_flags(m, e, pc, NULL);
          if (!pc)
            goto bail;
        }
      }
    }
  } while (rc == IMAP_RES_CONTINUE);

  /* see comment before command start. */
  e->active = true;

  fflush(msg->fp);
  if (ferror(msg->fp))
    goto bail;

  if (rc != IMAP_RES_OK)
    goto bail;

  if (!fetched || !imap_code(adata->buf))
    goto bail;

  msg_cache_commit(m, e);

parsemsg:
  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.  */
  rewind(msg->fp);
  /* It may be that the Status header indicates a message is read, but the
   * IMAP server doesn't know the message has been \Seen. So we capture
   * the server's notion of 'read' and if it differs from the message info
   * picked up in mutt_rfc822_read_header, we mark the message (and context
   * changed). Another possibility: ignore Status on IMAP? */
  read = e->read;
  newenv = mutt_rfc822_read_header(msg->fp, e, false, false);
  mutt_env_merge(e->env, &newenv);

  /* see above. We want the new status in e->read, so we unset it manually
   * and let mutt_set_flag set it correctly, updating context. */
  if (read != e->read)
  {
    e->read = read;
    mutt_set_flag(m, e, MUTT_NEW, read, true);
  }

  e->lines = 0;
  while (fgets(buf, sizeof(buf), msg->fp) && !feof(msg->fp))
  {
    e->lines++;
  }

  e->body->length = ftell(msg->fp) - e->body->offset;

  mutt_clear_error();
  rewind(msg->fp);
  imap_edata_get(e)->parsed = true;

  /* retry message parse if cached message is empty */
  if (!retried && ((e->lines == 0) || (e->body->length == 0)))
  {
    imap_cache_del(m, e);
    retried = true;
    goto parsemsg;
  }

  return true;

bail:
  e->active = true;
  mutt_file_fclose(&msg->fp);
  imap_cache_del(m, e);
  return false;
}

/**
 * imap_msg_commit - Save changes to an email - Implements MxOps::msg_commit() - @ingroup mx_msg_commit
 *
 * @note May also return EOF Failure, see errno
 */
int imap_msg_commit(struct Mailbox *m, struct Message *msg)
{
  int rc = mutt_file_fclose(&msg->fp);
  if (rc != 0)
    return rc;

  return imap_append_message(m, msg);
}

/**
 * imap_msg_close - Close an email - Implements MxOps::msg_close() - @ingroup mx_msg_close
 *
 * @note May also return EOF Failure, see errno
 */
int imap_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * imap_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache() - @ingroup mx_msg_save_hcache
 */
int imap_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  bool close_hc = true;
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!mdata || !adata)
    return -1;
  if (mdata->hcache)
    close_hc = false;
  else
    imap_hcache_open(adata, mdata);
  rc = imap_hcache_put(mdata, e);
  if (close_hc)
    imap_hcache_close(mdata);
#endif
  return rc;
}
