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
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "message.h"
#include "bcache.h"
#include "context.h"
#include "curs_lib.h"
#include "globals.h"
#include "imap/imap.h"
#include "mailbox.h"
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
 * imap_edata_free - free ImapHeader structure
 * @retval ptr Private Email data
 */
void imap_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapEmailData *edata = *ptr;
  /* this should be safe even if the list wasn't used */
  FREE(&edata->flags_system);
  FREE(&edata->flags_remote);
  FREE(ptr);
}

/**
 * imap_edata_new - Create a new ImapEmailData
 * @retval ptr New ImapEmailData
 */
static struct ImapEmailData *imap_edata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ImapEmailData));
}

/**
 * imap_edata_get - Get the private data for this Email
 * @retval ptr Private Email data
 */
struct ImapEmailData *imap_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}

/**
 * msg_cache_open - Open a message cache
 * @param m     Selected Imap Mailbox
 * @retval ptr  Success, using existing cache
 * @retval ptr  Success, opened new cache
 * @retval NULL Failure
 */
static struct BodyCache *msg_cache_open(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!adata || adata->mailbox != m)
    return NULL;

  char mailbox[PATH_MAX];

  if (mdata->bcache)
    return mdata->bcache;

  imap_cachepath(adata->delim, mdata->name, mailbox, sizeof(mailbox));

  return mutt_bcache_open(&adata->conn->account, mailbox);
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

  if (!e || !adata || adata->mailbox != m)
    return NULL;

  mdata->bcache = msg_cache_open(m);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", mdata->uid_validity, imap_edata_get(e)->uid);
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

  if (!e || !adata || adata->mailbox != m)
    return NULL;

  mdata->bcache = msg_cache_open(m);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", mdata->uid_validity, imap_edata_get(e)->uid);
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

  if (!e || !adata || adata->mailbox != m)
    return -1;

  mdata->bcache = msg_cache_open(m);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", mdata->uid_validity, imap_edata_get(e)->uid);

  return mutt_bcache_commit(mdata->bcache, id);
}

/**
 * msg_cache_clean_cb - Delete an entry from the message cache - Implements ::bcache_list_t
 * @retval 0 Always
 */
static int msg_cache_clean_cb(const char *id, struct BodyCache *bcache, void *data)
{
  unsigned int uv, uid;
  struct ImapMboxData *mdata = data;

  if (sscanf(id, "%u-%u", &uv, &uid) != 2)
    return 0;

  /* bad UID */
  if (uv != mdata->uid_validity || !mutt_hash_int_find(mdata->uid_hash, uid))
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
  struct ImapEmailData *edata = h->data;

  /* sanity-check string */
  size_t plen = mutt_str_startswith(s, "FLAGS", CASE_IGNORE);
  if (plen == 0)
  {
    mutt_debug(1, "not a FLAGS response: %s\n", s);
    return NULL;
  }
  s += plen;
  SKIPWS(s);
  if (*s != '(')
  {
    mutt_debug(1, "bogus FLAGS response: %s\n", s);
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
  while (*s && *s != ')')
  {
    if ((plen = mutt_str_startswith(s, "\\deleted", CASE_IGNORE)))
    {
      s += plen;
      edata->deleted = true;
    }
    else if ((plen = mutt_str_startswith(s, "\\flagged", CASE_IGNORE)))
    {
      s += plen;
      edata->flagged = true;
    }
    else if ((plen = mutt_str_startswith(s, "\\answered", CASE_IGNORE)))
    {
      s += plen;
      edata->replied = true;
    }
    else if ((plen = mutt_str_startswith(s, "\\seen", CASE_IGNORE)))
    {
      s += plen;
      edata->read = true;
    }
    else if ((plen = mutt_str_startswith(s, "\\recent", CASE_IGNORE)))
    {
      s += plen;
    }
    else if ((plen = mutt_str_startswith(s, "old", CASE_IGNORE)))
    {
      s += plen;
      edata->old = MarkOld ? true : false;
    }
    else
    {
      char ctmp;
      char *flag_word = s;
      bool is_system_keyword = mutt_str_startswith(s, "\\", CASE_IGNORE);

      while (*s && !ISSPACE(*s) && *s != ')')
        s++;

      ctmp = *s;
      *s = '\0';

      /* store other system flags as well (mainly \\Draft) */
      if (is_system_keyword)
        mutt_str_append_item(&edata->flags_system, flag_word, ' ');
      /* store custom flags as well */
      else
        mutt_str_append_item(&edata->flags_remote, flag_word, ' ');

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
  size_t plen = 0;

  if (!s)
    return -1;

  while (*s)
  {
    SKIPWS(s);

    if (mutt_str_startswith(s, "FLAGS", CASE_IGNORE))
    {
      s = msg_parse_flags(h, s);
      if (!s)
        return -1;
    }
    else if ((plen = mutt_str_startswith(s, "UID", CASE_IGNORE)))
    {
      s += plen;
      SKIPWS(s);
      if (mutt_str_atoui(s, &h->data->uid) < 0)
        return -1;

      s = imap_next_word(s);
    }
    else if ((plen = mutt_str_startswith(s, "INTERNALDATE", CASE_IGNORE)))
    {
      s += plen;
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
    else if ((plen = mutt_str_startswith(s, "RFC822.SIZE", CASE_IGNORE)))
    {
      s += plen;
      SKIPWS(s);
      ptmp = tmp;
      while (isdigit((unsigned char) *s) && (ptmp != (tmp + sizeof(tmp) - 1)))
        *ptmp++ = *s++;
      *ptmp = '\0';
      if (mutt_str_atol(tmp, &h->content_length) < 0)
        return -1;
    }
    else if (mutt_str_startswith(s, "BODY", CASE_IGNORE) ||
             mutt_str_startswith(s, "RFC822.HEADER", CASE_IGNORE))
    {
      /* handle above, in msg_fetch_header */
      return -2;
    }
    else if ((plen = mutt_str_startswith(s, "MODSEQ", CASE_IGNORE)))
    {
      s += plen;
      SKIPWS(s);
      if (*s != '(')
      {
        mutt_debug(1, "bogus MODSEQ response: %s\n", s);
        return -1;
      }
      s++;
      while (*s && *s != ')')
        s++;
      if (*s == ')')
        s++;
      else
      {
        mutt_debug(1, "Unterminated MODSEQ response: %s\n", s);
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
  unsigned int bytes;
  int rc = -1; /* default now is that string isn't FETCH response */
  int parse_rc;

  struct ImapAccountData *adata = imap_adata_get(m);

  if (buf[0] != '*')
    return rc;

  /* skip to message number */
  buf = imap_next_word(buf);
  if (mutt_str_atoui(buf, &ih->data->msn) < 0)
    return rc;

  /* find FETCH tag */
  buf = imap_next_word(buf);
  if (!mutt_str_startswith(buf, "FETCH", CASE_IGNORE))
    return rc;

  rc = -2; /* we've got a FETCH response, for better or worse */
  buf = strchr(buf, '(');
  if (!buf)
    return rc;
  buf++;

  /* FIXME: current implementation - call msg_parse_fetch - if it returns -2,
   *   read header lines and call it again. Silly. */
  parse_rc = msg_parse_fetch(ih, buf);
  if (!parse_rc)
    return 0;
  if (parse_rc != -2 || !fp)
    return rc;

  if (imap_get_literal_count(buf, &bytes) == 0)
  {
    imap_read_literal(fp, adata, bytes, NULL);

    /* we may have other fields of the FETCH _after_ the literal
     * (eg Domino puts FLAGS here). Nothing wrong with that, either.
     * This all has to go - we should accept literals and nonliterals
     * interchangeably at any time. */
    if (imap_cmd_step(adata) != IMAP_CMD_CONTINUE)
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
 */
static void flush_buffer(char *buf, size_t *len, struct Connection *conn)
{
  buf[*len] = '\0';
  mutt_socket_write_n(conn, buf, *len);
  *len = 0;
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
  SigInt = 0;

  return abort;
}

/**
 * alloc_msn_index - Create lookup table of MSN to Header
 * @param adata Imap Account data
 * @param msn_count Number of MSNs in use
 *
 * Mapping from Message Sequence Number to Header
 */
static void alloc_msn_index(struct ImapAccountData *adata, size_t msn_count)
{
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  size_t new_size;

  if (msn_count <= mdata->msn_index_size)
    return;

  /* This is a conservative check to protect against a malicious imap
   * server.  Most likely size_t is bigger than an unsigned int, but
   * if msn_count is this big, we have a serious problem. */
  if (msn_count >= (UINT_MAX / sizeof(struct Email *)))
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  /* Add a little padding, like mx_allloc_memory() */
  new_size = msn_count + 25;

  if (!mdata->msn_index)
    mdata->msn_index = mutt_mem_calloc(new_size, sizeof(struct Email *));
  else
  {
    mutt_mem_realloc(&mdata->msn_index, sizeof(struct Email *) * new_size);
    memset(mdata->msn_index + mdata->msn_index_size, 0,
           sizeof(struct Email *) * (new_size - mdata->msn_index_size));
  }

  mdata->msn_index_size = new_size;
}

/**
 * imap_alloc_uid_hash - Create a Hash Table for the UIDs
 * @param adata Imap Account data
 * @param msn_count Number of MSNs in use
 *
 * This function is run after imap_alloc_msn_index, so we skip the
 * malicious msn_count size check.
 */
static void imap_alloc_uid_hash(struct ImapAccountData *adata, unsigned int msn_count)
{
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  if (!mdata->uid_hash)
    mdata->uid_hash = mutt_hash_int_new(MAX(6 * msn_count / 5, 30), 0);
}

/**
 * imap_fetch_msn_seqset - Generate a sequence set
 * @param b         Buffer for the result
 * @param adata Imap Account data
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
static void imap_fetch_msn_seqset(struct Buffer *b, struct ImapAccountData *adata,
                                  unsigned int msn_begin, unsigned int msn_end)
{
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  int chunks = 0;
  int state = 0; /* 1: single msn, 2: range of msn */
  unsigned int range_begin = 0;
  unsigned int range_end = 0;

  for (unsigned int msn = msn_begin; msn <= (msn_end + 1); msn++)
  {
    if ((msn <= msn_end) && !mdata->msn_index[msn - 1])
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
        mutt_buffer_add_printf(b, "%u", range_begin);
      else if (state == 2)
        mutt_buffer_add_printf(b, "%u:%u", range_begin, range_end);
      state = 0;
    }
  }

  /* Too big.  Just query the whole range then. */
  if ((chunks == 150) || (mutt_str_strlen(b->data) > 500))
  {
    b->dptr = b->data;
    mutt_buffer_add_printf(b, "%u:%u", msn_begin, msn_end);
  }
}

/**
 * set_changed_flag - Have the flags of an email changed
 * @param[in]  m              Mailbox
 * @param[in]  e              Email
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
static void set_changed_flag(struct Mailbox *m, struct Email *e,
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
    mutt_set_flag(m, e, flag_name, new_hd_flag);
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
  struct Progress progress;
  char buf[LONG_STRING];

  struct Mailbox *m = adata->mailbox;
  struct ImapMboxData *mdata = imap_mdata_get(m);
  int idx = m->msg_count;

  /* L10N:
     Comparing the cached data with the IMAP server's data */
  mutt_progress_init(&progress, _("Evaluating cache..."), MUTT_PROGRESS_MSG, ReadInc, msn_end);

  /* If we are using CONDSTORE's "FETCH CHANGEDSINCE", then we keep
   * the flags in the header cache, and update them further below.
   * Otherwise, we fetch the current state of the flags here. */
  snprintf(buf, sizeof(buf), "UID FETCH 1:%u (UID%s)", uid_next - 1,
           eval_condstore ? "" : " FLAGS");

  imap_cmd_start(adata, buf);

  int rc = IMAP_CMD_CONTINUE;
  int mfhrc = 0;
  struct ImapHeader h;
  for (int msgno = 1; rc == IMAP_CMD_CONTINUE; msgno++)
  {
    if (SigInt && query_abort_header_download(adata))
      return -1;

    mutt_progress_update(&progress, msgno, -1);

    memset(&h, 0, sizeof(h));
    h.data = imap_edata_new();
    do
    {
      rc = imap_cmd_step(adata);
      if (rc != IMAP_CMD_CONTINUE)
        break;

      mfhrc = msg_fetch_header(m, &h, adata->buf, NULL);
      if (mfhrc < 0)
        continue;

      if (!h.data->uid)
      {
        mutt_debug(2, "skipping hcache FETCH response for message number %d missing a UID\n",
                   h.data->msn);
        continue;
      }

      if ((h.data->msn < 1) || (h.data->msn > msn_end))
      {
        mutt_debug(1, "skipping hcache FETCH response for unknown message number %d\n",
                   h.data->msn);
        continue;
      }

      if (mdata->msn_index[h.data->msn - 1])
      {
        mutt_debug(2, "skipping hcache FETCH for duplicate message %d\n", h.data->msn);
        continue;
      }

      m->hdrs[idx] = imap_hcache_get(mdata, h.data->uid);
      if (m->hdrs[idx])
      {
        mdata->max_msn = MAX(mdata->max_msn, h.data->msn);
        mdata->msn_index[h.data->msn - 1] = m->hdrs[idx];
        mutt_hash_int_insert(mdata->uid_hash, h.data->uid, m->hdrs[idx]);

        m->hdrs[idx]->index = idx;
        /* messages which have not been expunged are ACTIVE (borrowed from mh
         * folders) */
        m->hdrs[idx]->active = true;
        m->hdrs[idx]->changed = false;
        if (!eval_condstore)
        {
          m->hdrs[idx]->read = h.data->read;
          m->hdrs[idx]->old = h.data->old;
          m->hdrs[idx]->deleted = h.data->deleted;
          m->hdrs[idx]->flagged = h.data->flagged;
          m->hdrs[idx]->replied = h.data->replied;
        }
        else
        {
          h.data->read = m->hdrs[idx]->read;
          h.data->old = m->hdrs[idx]->old;
          h.data->deleted = m->hdrs[idx]->deleted;
          h.data->flagged = m->hdrs[idx]->flagged;
          h.data->replied = m->hdrs[idx]->replied;
        }

        /*  mailbox->hdrs[msgno]->received is restored from mutt_hcache_restore */
        m->hdrs[idx]->edata = h.data;
        m->hdrs[idx]->free_edata = imap_edata_free;
        STAILQ_INIT(&m->hdrs[idx]->tags);

        /* We take a copy of the tags so we can split the string */
        char *tags_copy = mutt_str_strdup(h.data->flags_remote);
        driver_tags_replace(&m->hdrs[idx]->tags, tags_copy);
        FREE(&tags_copy);

        m->msg_count++;
        m->size += m->hdrs[idx]->content->length;

        /* If this is the first time we are fetching, we need to
         * store the current state of flags back into the header cache */
        if (!eval_condstore && store_flag_updates)
          imap_hcache_put(mdata, m->hdrs[idx]);

        h.data = NULL;
        idx++;
      }
    } while (mfhrc == -1);

    imap_edata_free((void **) &h.data);

    if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
      return -1;
  }

  return 0;
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

  mutt_debug(2, "Reading uid seqset from header cache\n");
  struct Mailbox *m = adata->mailbox;
  struct ImapMboxData *mdata = adata->mailbox->mdata;
  unsigned int msn = 1;

  struct SeqsetIterator *iter = mutt_seqset_iterator_new(uid_seqset);
  if (!iter)
    return -1;

  while ((rc = mutt_seqset_iterator_next(iter, &uid)) == 0)
  {
    /* The seqset may contain more headers than the fetch request, so
     * we need to watch and reallocate the context and msn_index */
    if (msn > mdata->msn_index_size)
      alloc_msn_index(adata, msn);

    struct Email *e = imap_hcache_get(mdata, uid);
    if (e)
    {
      mdata->max_msn = MAX(mdata->max_msn, msn);
      mdata->msn_index[msn - 1] = e;

      if (m->msg_count >= m->hdrmax)
        mx_alloc_memory(m);

      struct ImapEmailData *edata = imap_edata_new();
      e->edata = edata;
      e->free_edata = imap_edata_free;

      e->index = m->msg_count;
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

      m->size += e->content->length;
      m->hdrs[m->msg_count++] = e;

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
  struct Progress progress;
  char buf[LONG_STRING];
  unsigned int header_msn = 0;

  struct Mailbox *m = adata->mailbox;
  struct ImapMboxData *mdata = imap_mdata_get(m);

  /* L10N: Fetching IMAP flag changes, using the CONDSTORE extension */
  mutt_progress_init(&progress, _("Fetching flag updates..."),
                     MUTT_PROGRESS_MSG, ReadInc, msn_end);

  snprintf(buf, sizeof(buf), "UID FETCH 1:%u (FLAGS) (CHANGEDSINCE %llu%s)",
           uid_next - 1, hc_modseq, eval_qresync ? " VANISHED" : "");

  imap_cmd_start(adata, buf);

  int rc = IMAP_CMD_CONTINUE;
  for (int msgno = 1; rc == IMAP_CMD_CONTINUE; msgno++)
  {
    if (SigInt && query_abort_header_download(adata))
      return -1;

    mutt_progress_update(&progress, msgno, -1);

    /* cmd_parse_fetch will update the flags */
    rc = imap_cmd_step(adata);
    if (rc != IMAP_CMD_CONTINUE)
      break;

    /* so we just need to grab the header and persist it back into
     * the header cache */
    char *fetch_buf = adata->buf;
    if (fetch_buf[0] != '*')
      continue;

    fetch_buf = imap_next_word(fetch_buf);
    if (!isdigit((unsigned char) *fetch_buf) || (mutt_str_atoui(fetch_buf, &header_msn) < 0))
      continue;

    if ((header_msn < 1) || (header_msn > msn_end) || !mdata->msn_index[header_msn - 1])
    {
      mutt_debug(1, "skipping CONDSTORE flag update for unknown message number %u\n", header_msn);
      continue;
    }

    imap_hcache_put(mdata, mdata->msn_index[header_msn - 1]);
  }

  /* The IMAP flag setting as part of cmd_parse_fetch() ends up
   * flipping these on. */
  mdata->check_status &= ~IMAP_FLAGS_PENDING;
  m->changed = false;

  /* VANISHED handling: we need to empty out the messages */
  if (mdata->reopen & IMAP_EXPUNGE_PENDING)
  {
    imap_expunge_mailbox(m);

    /* undo expunge count updates.
     * mx_update_context() will do this at the end of the header fetch. */
    m->vcount = 0;
    m->msg_tagged = 0;
    m->msg_deleted = 0;
    m->msg_new = 0;
    m->msg_unread = 0;
    m->msg_flagged = 0;
    m->changed = 0;

    mdata->reopen &= ~IMAP_EXPUNGE_PENDING;
  }

  return 0;
}
#endif /* USE_HCACHE */

/**
 * read_headers_fetch_new - Retrieve new messages from the server
 * @param[in]  m                Imap Selected Mailbox
 * @param[in]  msn_begin        First Message Sequence number
 * @param[in]  msn_end          Last Message Sequence number
 * @param[in]  evalhc           if true, check the Header Cache
 * @param[out] maxuid           Highest UID seen
 * @param[in]  initial_download true, if this is the first opening of the mailbox
 * @retval  0 Success
 * @retval -1 Error
 */
static int read_headers_fetch_new(struct Mailbox *m, unsigned int msn_begin,
                                  unsigned int msn_end, bool evalhc,
                                  unsigned int *maxuid, bool initial_download)
{
  int rc, mfhrc = 0, retval = -1;
  unsigned int fetch_msn_end = 0;
  struct Progress progress;
  char *hdrreq = NULL;
  char tempfile[_POSIX_PATH_MAX];
  FILE *fp = NULL;
  struct ImapHeader h;
  static const char *const want_headers =
      "DATE FROM SENDER SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE "
      "CONTENT-DESCRIPTION IN-REPLY-TO REPLY-TO LINES LIST-POST X-LABEL "
      "X-ORIGINAL-TO";

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  int idx = m->msg_count;

  if (!adata || adata->mailbox != m)
    return -1;

  if (mutt_bit_isset(adata->capabilities, IMAP_CAP_IMAP4REV1))
  {
    safe_asprintf(&hdrreq, "BODY.PEEK[HEADER.FIELDS (%s%s%s)]", want_headers,
                  ImapHeaders ? " " : "", NONULL(ImapHeaders));
  }
  else if (mutt_bit_isset(adata->capabilities, IMAP_CAP_IMAP4))
  {
    safe_asprintf(&hdrreq, "RFC822.HEADER.LINES (%s%s%s)", want_headers,
                  ImapHeaders ? " " : "", NONULL(ImapHeaders));
  }
  else
  { /* Unable to fetch headers for lower versions */
    mutt_error(_("Unable to fetch headers from this IMAP server version"));
    goto bail;
  }

  /* instead of downloading all headers and then parsing them, we parse them
   * as they come in. */
  mutt_mktemp(tempfile, sizeof(tempfile));
  fp = mutt_file_fopen(tempfile, "w+");
  if (!fp)
  {
    mutt_error(_("Could not create temporary file %s"), tempfile);
    goto bail;
  }
  unlink(tempfile);

  mutt_progress_init(&progress, _("Fetching message headers..."),
                     MUTT_PROGRESS_MSG, ReadInc, msn_end);

  while ((msn_begin <= msn_end) && (fetch_msn_end < msn_end))
  {
    struct Buffer *b = mutt_buffer_new();
    if (evalhc)
    {
      /* In case there are holes in the header cache. */
      evalhc = false;
      imap_fetch_msn_seqset(b, adata, msn_begin, msn_end);
    }
    else
      mutt_buffer_add_printf(b, "%u:%u", msn_begin, msn_end);

    fetch_msn_end = msn_end;
    char *cmd = NULL;
    safe_asprintf(&cmd, "FETCH %s (UID FLAGS INTERNALDATE RFC822.SIZE %s)", b->data, hdrreq);
    imap_cmd_start(adata, cmd);
    FREE(&cmd);
    mutt_buffer_free(&b);

    rc = IMAP_CMD_CONTINUE;
    for (int msgno = msn_begin; rc == IMAP_CMD_CONTINUE; msgno++)
    {
      if (initial_download && SigInt && query_abort_header_download(adata))
        goto bail;

      mutt_progress_update(&progress, msgno, -1);

      rewind(fp);
      memset(&h, 0, sizeof(h));
      h.data = imap_edata_new();

      /* this DO loop does two things:
       * 1. handles untagged messages, so we can try again on the same msg
       * 2. fetches the tagged response at the end of the last message.
       */
      do
      {
        rc = imap_cmd_step(adata);
        if (rc != IMAP_CMD_CONTINUE)
          break;

        mfhrc = msg_fetch_header(m, &h, adata->buf, fp);
        if (mfhrc < 0)
          continue;

        if (!ftello(fp))
        {
          mutt_debug(2, "ignoring fetch response with no body\n");
          continue;
        }

        /* make sure we don't get remnants from older larger message headers */
        fputs("\n\n", fp);

        if ((h.data->msn < 1) || (h.data->msn > fetch_msn_end))
        {
          mutt_debug(1, "skipping FETCH response for unknown message number %d\n",
                     h.data->msn);
          continue;
        }

        /* May receive FLAGS updates in a separate untagged response (#2935) */
        if (mdata->msn_index[h.data->msn - 1])
        {
          mutt_debug(2, "skipping FETCH response for duplicate message %d\n",
                     h.data->msn);
          continue;
        }

        m->hdrs[idx] = mutt_email_new();

        mdata->max_msn = MAX(mdata->max_msn, h.data->msn);
        mdata->msn_index[h.data->msn - 1] = m->hdrs[idx];
        mutt_hash_int_insert(mdata->uid_hash, h.data->uid, m->hdrs[idx]);

        m->hdrs[idx]->index = idx;
        /* messages which have not been expunged are ACTIVE (borrowed from mh
         * folders) */
        m->hdrs[idx]->active = true;
        m->hdrs[idx]->changed = false;
        m->hdrs[idx]->read = h.data->read;
        m->hdrs[idx]->old = h.data->old;
        m->hdrs[idx]->deleted = h.data->deleted;
        m->hdrs[idx]->flagged = h.data->flagged;
        m->hdrs[idx]->replied = h.data->replied;
        m->hdrs[idx]->received = h.received;
        m->hdrs[idx]->edata = (void *) (h.data);
        m->hdrs[idx]->free_edata = imap_edata_free;
        STAILQ_INIT(&m->hdrs[idx]->tags);

        /* We take a copy of the tags so we can split the string */
        char *tags_copy = mutt_str_strdup(h.data->flags_remote);
        driver_tags_replace(&m->hdrs[idx]->tags, tags_copy);
        FREE(&tags_copy);

        if (*maxuid < h.data->uid)
          *maxuid = h.data->uid;

        rewind(fp);
        /* NOTE: if Date: header is missing, mutt_rfc822_read_header depends
         *   on h.received being set */
        m->hdrs[idx]->env = mutt_rfc822_read_header(fp, m->hdrs[idx], false, false);
        /* content built as a side-effect of mutt_rfc822_read_header */
        m->hdrs[idx]->content->length = h.content_length;
        m->size += h.content_length;

#ifdef USE_HCACHE
        imap_hcache_put(mdata, m->hdrs[idx]);
#endif /* USE_HCACHE */

        m->msg_count++;

        h.data = NULL;
        idx++;
      } while (mfhrc == -1);

      imap_edata_free((void **) &h.data);

      if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
        goto bail;
    }

    /* In case we get new mail while fetching the headers.
     *
     * Note: The RFC says we shouldn't get any EXPUNGE responses in the
     * middle of a FETCH.  But just to be cautious, use the current state
     * of max_msn, not fetch_msn_end to set the next start range.
     */
    if (mdata->reopen & IMAP_NEWMAIL_PENDING)
    {
      /* update to the last value we actually pulled down */
      fetch_msn_end = mdata->max_msn;
      msn_begin = mdata->max_msn + 1;
      msn_end = mdata->new_mail_count;
      while (msn_end > m->hdrmax)
        mx_alloc_memory(m);
      alloc_msn_index(adata, msn_end);
      mdata->reopen &= ~IMAP_NEWMAIL_PENDING;
      mdata->new_mail_count = 0;
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
  int oldmsgcount;
  unsigned int maxuid = 0;
  int retval = -1;
  bool evalhc = false;

#ifdef USE_HCACHE
  void *uid_validity = NULL;
  void *puid_next = NULL;
  unsigned int uid_next = 0;
  bool has_condstore = false;
  bool has_qresync = false;
  bool eval_condstore = false;
  bool eval_qresync = false;
  unsigned long long *pmodseq = NULL;
  unsigned long long hc_modseq = 0;
  char *uid_seqset = NULL;
#endif /* USE_HCACHE */

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || adata->mailbox != m)
    return -1;

  /* make sure context has room to hold the mailbox */
  while (msn_end > m->hdrmax)
    mx_alloc_memory(m);
  alloc_msn_index(adata, msn_end);
  imap_alloc_uid_hash(adata, msn_end);

  oldmsgcount = m->msg_count;
  mdata->reopen &= ~(IMAP_REOPEN_ALLOW | IMAP_NEWMAIL_PENDING);
  mdata->new_mail_count = 0;

#ifdef USE_HCACHE
  if (mdata->hcache && initial_download)
  {
    uid_validity = mutt_hcache_fetch_raw(mdata->hcache, "/UIDVALIDITY", 12);
    puid_next = mutt_hcache_fetch_raw(mdata->hcache, "/UIDNEXT", 8);
    if (puid_next)
    {
      uid_next = *(unsigned int *) puid_next;
      mutt_hcache_free(mdata->hcache, &puid_next);
    }

    if (mdata->modseq)
    {
      if (mutt_bit_isset(adata->capabilities, IMAP_CAP_CONDSTORE) && ImapCondStore)
        has_condstore = true;

      /* If mutt_bit_isset(QRESYNC) and option(OPTIMAPQRESYNC) then Mutt
       * sends ENABLE QRESYNC.  If we receive an ENABLED response back, then
       * adata->qresync is set.
       */
      if (adata->qresync)
        has_qresync = true;
    }

    if (uid_validity && uid_next && (*(unsigned int *) uid_validity == mdata->uid_validity))
    {
      evalhc = true;
      pmodseq = mutt_hcache_fetch_raw(mdata->hcache, "/MODSEQ", 7);
      if (pmodseq)
      {
        hc_modseq = *pmodseq;
        mutt_hcache_free(mdata->hcache, (void **) &pmodseq);
      }
      if (hc_modseq)
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
    mutt_hcache_free(mdata->hcache, &uid_validity);
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

    if ((eval_condstore || eval_qresync) && (hc_modseq != mdata->modseq))
    {
      if (read_headers_condstore_qresync_updates(adata, msn_end, uid_next,
                                                 hc_modseq, eval_qresync) < 0)
      {
        goto bail;
      }
    }

    /* Look for the first empty MSN and start there */
    while (msn_begin <= msn_end)
    {
      if (!mdata->msn_index[msn_begin - 1])
        break;
      msn_begin++;
    }
  }
#endif /* USE_HCACHE */

  if (read_headers_fetch_new(m, msn_begin, msn_end, evalhc, &maxuid, initial_download) < 0)
    goto bail;

  if (maxuid && mdata->uid_next < maxuid + 1)
    mdata->uid_next = maxuid + 1;

#ifdef USE_HCACHE
  mutt_hcache_store_raw(mdata->hcache, "/UIDVALIDITY", 12, &mdata->uid_validity,
                        sizeof(mdata->uid_validity));
  if (maxuid && mdata->uid_next < maxuid + 1)
  {
    mutt_debug(2, "Overriding UIDNEXT: %u -> %u\n", mdata->uid_next, maxuid + 1);
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
   * the header cache on close.  I'm not sure it's worth the added complexity.
   */
  if (initial_download)
  {
    if (has_condstore || has_qresync)
    {
      mutt_hcache_store_raw(mdata->hcache, "/MODSEQ", 7, &mdata->modseq,
                            sizeof(mdata->modseq));
    }
    else
      mutt_hcache_delete(mdata->hcache, "/MODSEQ", 7);

    if (has_qresync)
      imap_hcache_store_uid_seqset(mdata);
    else
      imap_hcache_clear_uid_seqset(mdata);
  }
#endif /* USE_HCACHE */

  if (m->msg_count > oldmsgcount)
  {
    /* TODO: it's not clear to me why we are calling mx_alloc_memory
     *       yet again. */
    mx_alloc_memory(m);
  }

  mdata->reopen |= IMAP_REOPEN_ALLOW;

  retval = msn_end;

bail:
#ifdef USE_HCACHE
  FREE(&uid_seqset);
#endif /* USE_HCACHE */

  return retval;
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
  char buf[LONG_STRING * 2];
  char internaldate[IMAP_DATELEN];
  char imap_flags[SHORT_STRING];
  size_t len;
  struct Progress progressbar;
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

  snprintf(buf, sizeof(buf), "APPEND %s (%s) \"%s\" {%lu}", mdata->munge_name,
           imap_flags + 1, internaldate, (unsigned long) len);

  imap_cmd_start(adata, buf);

  do
    rc = imap_cmd_step(adata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_RESPOND)
  {
    mutt_debug(1, "#1 command failed: %s\n", adata->buf);

    char *pc = adata->buf + SEQLEN;
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
      flush_buffer(buf, &len, adata->conn);
      mutt_progress_update(&progressbar, sent, -1);
    }
  }

  if (len)
    flush_buffer(buf, &len, adata->conn);

  mutt_socket_send(adata->conn, "\r\n");
  mutt_file_fclose(&fp);

  do
    rc = imap_cmd_step(adata);
  while (rc == IMAP_CMD_CONTINUE);

  if (!imap_code(adata->buf))
  {
    mutt_debug(1, "#2 command failed: %s\n", adata->buf);
    char *pc = adata->buf + SEQLEN;
    SKIPWS(pc);
    pc = imap_next_word(pc);
    mutt_error("%s", pc);
    goto fail;
  }

  return 0;

fail:
  return -1;
}

/**
 * imap_copy_messages - Server COPY messages to another folder
 * @param ctx    Mailbox
 * @param e      Email
 * @param dest   Destination folder
 * @param delete Delete the original?
 * @retval -1 Error
 * @retval  0 Success
 * @retval  1 Non-fatal error - try fetch/append
 */
int imap_copy_messages(struct Context *ctx, struct Email *e, char *dest, bool delete)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Buffer cmd, sync_cmd;
  char buf[PATH_MAX];
  char mbox[PATH_MAX];
  char mmbox[PATH_MAX];
  char prompt[PATH_MAX + 64];
  int rc;
  struct ConnAccount conn_account;
  int err_continue = MUTT_NO;
  int triedcreate = 0;

  struct Mailbox *m = ctx->mailbox;

  if (e && e->attach_del)
  {
    mutt_debug(3, "#1 Message contains attachments to be deleted\n");
    return 1;
  }

  struct ImapAccountData *adata = imap_adata_get(m);

  if (imap_parse_path(dest, &conn_account, buf, sizeof(buf)))
  {
    mutt_debug(1, "bad destination %s\n", dest);
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (!mutt_account_match(&adata->conn->account, &conn_account))
  {
    mutt_debug(3, "%s not same server as %s\n", dest, m->path);
    return 1;
  }

  imap_fix_path(adata->delim, buf, mbox, sizeof(mbox));
  if (!*mbox)
    mutt_str_strfcpy(mbox, "INBOX", sizeof(mbox));
  imap_munge_mbox_name(adata->unicode, mmbox, sizeof(mmbox), mbox);

  /* loop in case of TRYCREATE */
  do
  {
    mutt_buffer_init(&sync_cmd);
    mutt_buffer_init(&cmd);

    /* Null Header* means copy tagged messages */
    if (!e)
    {
      /* if any messages have attachments to delete, fall through to FETCH
       * and APPEND. TODO: Copy what we can with COPY, fall through for the
       * remainder. */
      for (int i = 0; i < m->msg_count; i++)
      {
        if (!message_is_tagged(ctx, i))
          continue;

        if (m->hdrs[i]->attach_del)
        {
          mutt_debug(3, "#2 Message contains attachments to be deleted\n");
          return 1;
        }

        if (m->hdrs[i]->active && m->hdrs[i]->changed)
        {
          rc = imap_sync_message_for_copy(m, m->hdrs[i], &sync_cmd, &err_continue);
          if (rc < 0)
          {
            mutt_debug(1, "#1 could not sync\n");
            goto out;
          }
        }
      }

      rc = imap_exec_msgset(m, "UID COPY", mmbox, MUTT_TAG, false, false);
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
      mutt_message(_("Copying message %d to %s..."), e->index + 1, mbox);
      mutt_buffer_add_printf(&cmd, "UID COPY %u %s", imap_edata_get(e)->uid, mmbox);

      if (e->active && e->changed)
      {
        rc = imap_sync_message_for_copy(m, e, &sync_cmd, &err_continue);
        if (rc < 0)
        {
          mutt_debug(1, "#2 could not sync\n");
          goto out;
        }
      }
      rc = imap_exec(adata, cmd.data, IMAP_CMD_QUEUE);
      if (rc != IMAP_EXEC_SUCCESS)
      {
        mutt_debug(1, "#2 could not queue copy\n");
        goto out;
      }
    }

    /* let's get it on */
    rc = imap_exec(adata, NULL, 0);
    if (rc == IMAP_EXEC_ERROR)
    {
      if (triedcreate)
      {
        mutt_debug(1, "Already tried to create mailbox %s\n", mbox);
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (!mutt_str_startswith(imap_get_qualifier(adata->buf), "[TRYCREATE]", CASE_IGNORE))
        break;
      mutt_debug(3, "server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), mbox);
      if (Confirmcreate && mutt_yesorno(prompt, 1) != MUTT_YES)
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
  if (delete)
  {
    if (!e)
    {
      for (int i = 0; i < m->msg_count; i++)
      {
        if (!message_is_tagged(ctx, i))
          continue;

        mutt_set_flag(m, m->hdrs[i], MUTT_DELETE, 1);
        mutt_set_flag(m, m->hdrs[i], MUTT_PURGE, 1);
        if (DeleteUntag)
          mutt_set_flag(m, m->hdrs[i], MUTT_TAG, 0);
      }
    }
    else
    {
      mutt_set_flag(m, e, MUTT_DELETE, 1);
      mutt_set_flag(m, e, MUTT_PURGE, 1);
      if (DeleteUntag)
        mutt_set_flag(m, e, MUTT_TAG, 0);
    }
  }

  rc = 0;

out:
  if (cmd.data)
    FREE(&cmd.data);
  if (sync_cmd.data)
    FREE(&sync_cmd.data);

  return (rc < 0) ? -1 : rc;
}

/**
 * imap_cache_del - Delete an email from the body cache
 * @param m     Selected Imap Mailb
 * @param e     Email
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_cache_del(struct Mailbox *m, struct Email *e)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!e || !adata || adata->mailbox != m)
    return -1;

  mdata->bcache = msg_cache_open(m);
  char id[64];
  snprintf(id, sizeof(id), "%u-%u", mdata->uid_validity, imap_edata_get(e)->uid);
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

  if (!adata || adata->mailbox != m)
    return -1;

  mdata->bcache = msg_cache_open(m);
  mutt_bcache_list(mdata->bcache, msg_cache_clean_cb, mdata);

  return 0;
}

/**
 * imap_set_flags - fill the message header according to the server flags
 * @param[in]  m              Imap Selected Mailbox
 * @param[in]  e              Email
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
 * case of e->changed, if a change to a flag _would_ have been
 * made.
 */
char *imap_set_flags(struct Mailbox *m, struct Email *e, char *s, int *server_changes)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  if (!adata || adata->mailbox != m)
    return NULL;

  struct ImapHeader newh = { 0 };
  struct ImapEmailData old_edata;
  bool readonly;
  int local_changes;

  local_changes = e->changed;

  struct ImapEmailData *edata = e->edata;
  newh.data = edata;

  memcpy(&old_edata, edata, sizeof(old_edata));

  mutt_debug(2, "parsing FLAGS\n");
  s = msg_parse_flags(&newh, s);
  if (!s)
    return NULL;

  /* Update tags system */
  /* We take a copy of the tags so we can split the string */
  char *tags_copy = mutt_str_strdup(edata->flags_remote);
  driver_tags_replace(&e->tags, tags_copy);
  FREE(&tags_copy);

  /* YAUH (yet another ugly hack): temporarily set context to
   * read-write even if it's read-only, so *server* updates of
   * flags can be processed by mutt_set_flag. mailbox->changed must
   * be restored afterwards */
  readonly = m->readonly;
  m->readonly = false;

  /* This is redundant with the following two checks. Removing:
   * mutt_set_flag (adata->ctx, e, MUTT_NEW, !(edata->read || edata->old));
   */
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
  if (!local_changes)
    e->changed = false;
  m->changed &= !readonly;
  m->readonly = readonly;

  return s;
}

/**
 * imap_msg_open - Implements MxOps::msg_open()
 */
int imap_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m || !msg)
    return -1;

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

  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);

  if (!adata || adata->mailbox != m)
    return -1;

  struct Email *e = m->hdrs[msgno];

  msg->fp = msg_cache_get(m, e);
  if (msg->fp)
  {
    if (imap_edata_get(e)->parsed)
      return 0;
    else
      goto parsemsg;
  }

  /* we still do some caching even if imap_cachedir is unset */
  /* see if we already have the message in our cache */
  cacheno = imap_edata_get(e)->uid % IMAP_CACHE_LEN;
  cache = &mdata->cache[cacheno];

  if (cache->path)
  {
    /* don't treat cache errors as fatal, just fall back. */
    if (cache->uid == imap_edata_get(e)->uid && (msg->fp = fopen(cache->path, "r")))
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

  msg->fp = msg_cache_put(m, e);
  if (!msg->fp)
  {
    cache->uid = imap_edata_get(e)->uid;
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
  e->active = false;

  snprintf(buf, sizeof(buf), "UID FETCH %u %s", imap_edata_get(e)->uid,
           (mutt_bit_isset(adata->capabilities, IMAP_CAP_IMAP4REV1) ?
                (ImapPeek ? "BODY.PEEK[]" : "BODY[]") :
                "RFC822"));

  imap_cmd_start(adata, buf);
  do
  {
    rc = imap_cmd_step(adata);
    if (rc != IMAP_CMD_CONTINUE)
      break;

    pc = adata->buf;
    pc = imap_next_word(pc);
    pc = imap_next_word(pc);

    if (mutt_str_startswith(pc, "FETCH", CASE_IGNORE))
    {
      while (*pc)
      {
        pc = imap_next_word(pc);
        if (pc[0] == '(')
          pc++;
        if (mutt_str_startswith(pc, "UID", CASE_IGNORE))
        {
          pc = imap_next_word(pc);
          if (mutt_str_atoui(pc, &uid) < 0)
            goto bail;
          if (uid != imap_edata_get(e)->uid)
          {
            mutt_error(_(
                "The message index is incorrect. Try reopening the mailbox."));
          }
        }
        else if (mutt_str_startswith(pc, "RFC822", CASE_IGNORE) ||
                 mutt_str_startswith(pc, "BODY[]", CASE_IGNORE))
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
          if (imap_read_literal(msg->fp, adata, bytes,
                                output_progress ? &progressbar : NULL) < 0)
          {
            goto bail;
          }
          /* pick up trailing line */
          rc = imap_cmd_step(adata);
          if (rc != IMAP_CMD_CONTINUE)
            goto bail;
          pc = adata->buf;

          fetched = true;
        }
        /* UW-IMAP will provide a FLAGS update here if the FETCH causes a
         * change (eg from \Unseen to \Seen).
         * Uncommitted changes in neomutt take precedence. If we decide to
         * incrementally update flags later, this won't stop us syncing */
        else if (mutt_str_startswith(pc, "FLAGS", CASE_IGNORE) && !e->changed)
        {
          pc = imap_set_flags(m, e, pc, NULL);
          if (!pc)
            goto bail;
        }
      }
    }
  } while (rc == IMAP_CMD_CONTINUE);

  /* see comment before command start. */
  e->active = true;

  fflush(msg->fp);
  if (ferror(msg->fp))
  {
    mutt_perror(cache->path);
    goto bail;
  }

  if (rc != IMAP_CMD_OK)
    goto bail;

  if (!fetched || !imap_code(adata->buf))
    goto bail;

  msg_cache_commit(m, e);

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
  read = e->read;
  newenv = mutt_rfc822_read_header(msg->fp, e, false, false);
  mutt_env_merge(e->env, &newenv);

  /* see above. We want the new status in e->read, so we unset it manually
   * and let mutt_set_flag set it correctly, updating context. */
  if (read != e->read)
  {
    e->read = read;
    mutt_set_flag(m, e, MUTT_NEW, read);
  }

  e->lines = 0;
  fgets(buf, sizeof(buf), msg->fp);
  while (!feof(msg->fp))
  {
    e->lines++;
    fgets(buf, sizeof(buf), msg->fp);
  }

  e->content->length = ftell(msg->fp) - e->content->offset;

  mutt_clear_error();
  rewind(msg->fp);
  imap_edata_get(e)->parsed = true;

  /* retry message parse if cached message is empty */
  if (!retried && ((e->lines == 0) || (e->content->length == 0)))
  {
    imap_cache_del(m, e);
    retried = true;
    goto parsemsg;
  }

  return 0;

bail:
  mutt_file_fclose(&msg->fp);
  imap_cache_del(m, e);
  if (cache->path)
  {
    unlink(cache->path);
    FREE(&cache->path);
  }

  return -1;
}

/**
 * imap_msg_commit - Implements MxOps::msg_commit()
 *
 * @note May also return EOF Failure, see errno
 */
int imap_msg_commit(struct Mailbox *m, struct Message *msg)
{
  int r = mutt_file_fclose(&msg->fp);
  if (r != 0)
    return r;

  return imap_append_message(m, msg);
}

/**
 * imap_msg_close - Implements MxOps::msg_close()
 *
 * @note May also return EOF Failure, see errno
 */
int imap_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}
