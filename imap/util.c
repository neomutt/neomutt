/**
 * @file
 * IMAP helper functions
 *
 * @authors
 * Copyright (C) 1996-1998,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012 Brendan Cully <brendan@kublai.com>
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
 * @page imap_util IMAP helper functions
 *
 * IMAP helper functions
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt_account.h"
#include "mutt_globals.h"
#include "options.h"
#include "bcache/lib.h"
#include "imap/lib.h"
#ifdef USE_HCACHE
#include "message.h"
#include "hcache/lib.h"
#endif

/* These Config Variables are only used in imap/util.c */
char *C_ImapDelimChars; ///< Config: (imap) Characters that denote separators in IMAP folders
char *C_ImapLogin; ///< Config: (imap) Login name for the IMAP server (defaults to #C_ImapUser)
char *C_ImapOauthRefreshCommand; ///< Config: (imap) External command to generate OAUTH refresh token
char *C_ImapPass; ///< Config: (imap) Password for the IMAP server
short C_ImapPipelineDepth; ///< Config: (imap) Number of IMAP commands that may be queued up

/**
 * imap_adata_free - Release and clear storage in an ImapAccountData structure
 * @param[out] ptr Imap Account data
 */
void imap_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapAccountData *adata = *ptr;

  FREE(&adata->capstr);
  mutt_buffer_dealloc(&adata->cmdbuf);
  FREE(&adata->buf);
  FREE(&adata->cmds);

  if (adata->conn)
  {
    if (adata->conn->close)
      adata->conn->close(adata->conn);
    FREE(&adata->conn);
  }

  FREE(ptr);
}

/**
 * imap_adata_new - Allocate and initialise a new ImapAccountData structure
 * @retval ptr New ImapAccountData
 */
struct ImapAccountData *imap_adata_new(struct Account *a)
{
  struct ImapAccountData *adata = mutt_mem_calloc(1, sizeof(struct ImapAccountData));
  adata->account = a;

  static unsigned char new_seqid = 'a';

  adata->seqid = new_seqid;
  adata->cmdslots = C_ImapPipelineDepth + 2;
  adata->cmds = mutt_mem_calloc(adata->cmdslots, sizeof(*adata->cmds));

  if (++new_seqid > 'z')
    new_seqid = 'a';

  return adata;
}

/**
 * imap_adata_get - Get the Account data for this mailbox
 * @param m Mailbox
 * @retval ptr Private data
 */
struct ImapAccountData *imap_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_IMAP) || !m->account)
    return NULL;
  return m->account->adata;
}

/**
 * imap_adata_find - Find the Account data for this path
 * @param path  Path to search for
 * @param adata Imap Account data
 * @param mdata Imap Mailbox data
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_adata_find(const char *path, struct ImapAccountData **adata,
                    struct ImapMboxData **mdata)
{
  struct ConnAccount cac = { { 0 } };
  struct ImapAccountData *tmp_adata = NULL;
  char tmp[1024];

  if (imap_parse_path(path, &cac, tmp, sizeof(tmp)) < 0)
    return -1;

  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    if (np->type != MUTT_IMAP)
      continue;

    tmp_adata = np->adata;
    if (!tmp_adata)
      continue;
    if (imap_account_match(&tmp_adata->conn->account, &cac))
    {
      *mdata = imap_mdata_new(tmp_adata, tmp);
      *adata = tmp_adata;
      return 0;
    }
  }
  mutt_debug(LL_DEBUG3, "no ImapAccountData found\n");
  return -1;
}

/**
 * imap_mdata_new - Allocate and initialise a new ImapMboxData structure
 * @param adata Imap Account data
 * @param name  Name for Mailbox
 * @retval ptr New ImapMboxData
 */
struct ImapMboxData *imap_mdata_new(struct ImapAccountData *adata, const char *name)
{
  char buf[1024];
  struct ImapMboxData *mdata = mutt_mem_calloc(1, sizeof(struct ImapMboxData));

  mdata->real_name = mutt_str_dup(name);

  imap_fix_path(adata->delim, name, buf, sizeof(buf));
  if (buf[0] == '\0')
    mutt_str_copy(buf, "INBOX", sizeof(buf));
  mdata->name = mutt_str_dup(buf);

  imap_munge_mbox_name(adata->unicode, buf, sizeof(buf), mdata->name);
  mdata->munge_name = mutt_str_dup(buf);

  mdata->reopen &= IMAP_REOPEN_ALLOW;

  STAILQ_INIT(&mdata->flags);

#ifdef USE_HCACHE
  imap_hcache_open(adata, mdata);
  if (mdata->hcache)
  {
    size_t dlen = 0;
    void *uidvalidity = mutt_hcache_fetch_raw(mdata->hcache, "/UIDVALIDITY", 12, &dlen);
    void *uidnext = mutt_hcache_fetch_raw(mdata->hcache, "/UIDNEXT", 8, &dlen);
    unsigned long long *modseq =
        mutt_hcache_fetch_raw(mdata->hcache, "/MODSEQ", 7, &dlen);
    if (uidvalidity)
    {
      mdata->uidvalidity = *(uint32_t *) uidvalidity;
      mdata->uid_next = uidnext ? *(unsigned int *) uidnext : 0;
      mdata->modseq = modseq ? *modseq : 0;
      mutt_debug(LL_DEBUG3, "hcache uidvalidity %u, uidnext %u, modseq %llu\n",
                 mdata->uidvalidity, mdata->uid_next, mdata->modseq);
    }
    mutt_hcache_free_raw(mdata->hcache, &uidvalidity);
    mutt_hcache_free_raw(mdata->hcache, &uidnext);
    mutt_hcache_free_raw(mdata->hcache, (void **) &modseq);
    imap_hcache_close(mdata);
  }
#endif

  return mdata;
}

/**
 * imap_mdata_cache_reset - Release and clear cache data of ImapMboxData structure
 * @param mdata Imap Mailbox data
 */
void imap_mdata_cache_reset(struct ImapMboxData *mdata)
{
  mutt_hash_free(&mdata->uid_hash);
  FREE(&mdata->msn_index);
  mdata->msn_index_size = 0;
  mdata->max_msn = 0;
  mutt_bcache_close(&mdata->bcache);
}

/**
 * imap_mdata_free - Release and clear storage in an ImapMboxData structure
 * @param[out] ptr Imap Mailbox data
 */
void imap_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ImapMboxData *mdata = *ptr;

  imap_mdata_cache_reset(mdata);
  mutt_list_free(&mdata->flags);
  FREE(&mdata->name);
  FREE(&mdata->real_name);
  FREE(&mdata->munge_name);
  FREE(ptr);
}

/**
 * imap_mdata_get - Get the Mailbox data for this mailbox
 * @param m Mailbox
 */
struct ImapMboxData *imap_mdata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_IMAP) || !m->mdata)
    return NULL;
  return m->mdata;
}

/**
 * imap_get_parent - Get an IMAP folder's parent
 * @param mbox   Mailbox whose parent is to be determined
 * @param delim  Path delimiter
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 */
void imap_get_parent(const char *mbox, char delim, char *buf, size_t buflen)
{
  /* Make a copy of the mailbox name, but only if the pointers are different */
  if (mbox != buf)
    mutt_str_copy(buf, mbox, buflen);

  int n = mutt_str_len(buf);

  /* Let's go backwards until the next delimiter
   *
   * If buf[n] is a '/', the first n-- will allow us
   * to ignore it. If it isn't, then buf looks like
   * "/aaaaa/bbbb". There is at least one "b", so we can't skip
   * the "/" after the 'a's.
   *
   * If buf == '/', then n-- => n == 0, so the loop ends
   * immediately */
  for (n--; (n >= 0) && (buf[n] != delim); n--)
    ; // do nothing

  /* We stopped before the beginning. There is a trailing slash.  */
  if (n > 0)
  {
    /* Strip the trailing delimiter.  */
    buf[n] = '\0';
  }
  else
  {
    buf[0] = (n == 0) ? delim : '\0';
  }
}

/**
 * imap_get_parent_path - Get the path of the parent folder
 * @param path   Mailbox whose parent is to be determined
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 *
 * Provided an imap path, returns in buf the parent directory if
 * existent. Else returns the same path.
 */
void imap_get_parent_path(const char *path, char *buf, size_t buflen)
{
  struct ImapAccountData *adata = NULL;
  struct ImapMboxData *mdata = NULL;
  char mbox[1024];

  if (imap_adata_find(path, &adata, &mdata) < 0)
  {
    mutt_str_copy(buf, path, buflen);
    return;
  }

  /* Gets the parent mbox in mbox */
  imap_get_parent(mdata->name, adata->delim, mbox, sizeof(mbox));

  /* Returns a fully qualified IMAP url */
  imap_qualify_path(buf, buflen, &adata->conn->account, mbox);
  imap_mdata_free((void *) &mdata);
}

/**
 * imap_clean_path - Cleans an IMAP path using imap_fix_path
 * @param path Path to be cleaned
 * @param plen Length of the buffer
 *
 * Does it in place.
 */
void imap_clean_path(char *path, size_t plen)
{
  struct ImapAccountData *adata = NULL;
  struct ImapMboxData *mdata = NULL;

  if (imap_adata_find(path, &adata, &mdata) < 0)
    return;

  /* Returns a fully qualified IMAP url */
  imap_qualify_path(path, plen, &adata->conn->account, mdata->name);
  imap_mdata_free((void *) &mdata);
}

/**
 * imap_get_field - Get connection login credentials - Implements ConnAccount::get_field()
 */
static const char *imap_get_field(enum ConnAccountField field, void *gf_data)
{
  switch (field)
  {
    case MUTT_CA_LOGIN:
      return C_ImapLogin;
    case MUTT_CA_USER:
      return C_ImapUser;
    case MUTT_CA_PASS:
      return C_ImapPass;
    case MUTT_CA_OAUTH_CMD:
      return C_ImapOauthRefreshCommand;
    case MUTT_CA_HOST:
    default:
      return NULL;
  }
}

#ifdef USE_HCACHE
/**
 * imap_msn_index_to_uid_seqset - Convert MSN index of UIDs to Seqset
 * @param buf   Buffer for the result
 * @param mdata Imap Mailbox data
 *
 * Generates a seqseq of the UIDs in msn_index to persist in the header cache.
 * Empty spots are stored as 0.
 */
static void imap_msn_index_to_uid_seqset(struct Buffer *buf, struct ImapMboxData *mdata)
{
  int first = 1, state = 0;
  unsigned int cur_uid = 0, last_uid = 0;
  unsigned int range_begin = 0, range_end = 0;

  for (unsigned int msn = 1; msn <= mdata->max_msn + 1; msn++)
  {
    bool match = false;
    if (msn <= mdata->max_msn)
    {
      struct Email *e_cur = mdata->msn_index[msn - 1];
      cur_uid = e_cur ? imap_edata_get(e_cur)->uid : 0;
      if (!state || (cur_uid && ((cur_uid - 1) == last_uid)))
        match = true;
      last_uid = cur_uid;
    }

    if (match)
    {
      switch (state)
      {
        case 1: /* single: convert to a range */
          state = 2;
          /* fall through */
        case 2: /* extend range ending */
          range_end = cur_uid;
          break;
        default:
          state = 1;
          range_begin = cur_uid;
          break;
      }
    }
    else if (state)
    {
      if (first)
        first = 0;
      else
        mutt_buffer_addch(buf, ',');

      if (state == 1)
        mutt_buffer_add_printf(buf, "%u", range_begin);
      else if (state == 2)
        mutt_buffer_add_printf(buf, "%u:%u", range_begin, range_end);

      state = 1;
      range_begin = cur_uid;
    }
  }
}

/**
 * imap_hcache_namer - Generate a filename for the header cache - Implements ::hcache_namer_t
 */
static void imap_hcache_namer(const char *path, struct Buffer *dest)
{
  mutt_buffer_printf(dest, "%s.hcache", path);
}

/**
 * imap_hcache_open - Open a header cache
 * @param adata Imap Account data
 * @param mdata Imap Mailbox data
 */
void imap_hcache_open(struct ImapAccountData *adata, struct ImapMboxData *mdata)
{
  if (!adata || !mdata)
    return;

  if (mdata->hcache)
    return;

  header_cache_t *hc = NULL;
  struct Buffer *mbox = mutt_buffer_pool_get();
  struct Buffer *cachepath = mutt_buffer_pool_get();

  imap_cachepath(adata->delim, mdata->name, mbox);

  if (strstr(mutt_b2s(mbox), "/../") || mutt_str_equal(mutt_b2s(mbox), "..") ||
      mutt_strn_equal(mutt_b2s(mbox), "../", 3))
  {
    goto cleanup;
  }
  size_t len = mutt_buffer_len(mbox);
  if ((len > 3) && (strcmp(mutt_b2s(mbox) + len - 3, "/..") == 0))
    goto cleanup;

  struct Url url = { 0 };
  mutt_account_tourl(&adata->conn->account, &url);
  url.path = mbox->data;
  url_tobuffer(&url, cachepath, U_PATH);

  hc = mutt_hcache_open(C_HeaderCache, mutt_b2s(cachepath), imap_hcache_namer);

cleanup:
  mutt_buffer_pool_release(&mbox);
  mutt_buffer_pool_release(&cachepath);
  mdata->hcache = hc;
}

/**
 * imap_hcache_close - Close the header cache
 * @param mdata Imap Mailbox data
 */
void imap_hcache_close(struct ImapMboxData *mdata)
{
  if (!mdata->hcache)
    return;

  mutt_hcache_close(mdata->hcache);
  mdata->hcache = NULL;
}

/**
 * imap_hcache_get - Get a header cache entry by its UID
 * @param mdata Imap Mailbox data
 * @param uid   UID to find
 * @retval ptr Email
 * @retval NULL Failure
 */
struct Email *imap_hcache_get(struct ImapMboxData *mdata, unsigned int uid)
{
  if (!mdata->hcache)
    return NULL;

  char key[16];

  sprintf(key, "/%u", uid);
  struct HCacheEntry hce =
      mutt_hcache_fetch(mdata->hcache, key, mutt_str_len(key), mdata->uidvalidity);
  if (!hce.email && hce.uidvalidity)
  {
    mutt_debug(LL_DEBUG3, "hcache uidvalidity mismatch: %u\n", hce.uidvalidity);
  }

  return hce.email;
}

/**
 * imap_hcache_put - Add an entry to the header cache
 * @param mdata Imap Mailbox data
 * @param e     Email
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_hcache_put(struct ImapMboxData *mdata, struct Email *e)
{
  if (!mdata->hcache)
    return -1;

  char key[16];

  sprintf(key, "/%u", imap_edata_get(e)->uid);
  return mutt_hcache_store(mdata->hcache, key, mutt_str_len(key), e, mdata->uidvalidity);
}

/**
 * imap_hcache_del - Delete an item from the header cache
 * @param mdata Imap Mailbox data
 * @param uid   UID of entry to delete
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_hcache_del(struct ImapMboxData *mdata, unsigned int uid)
{
  if (!mdata->hcache)
    return -1;

  char key[16];

  sprintf(key, "/%u", uid);
  return mutt_hcache_delete_header(mdata->hcache, key, mutt_str_len(key));
}

/**
 * imap_hcache_store_uid_seqset - Store a UID Sequence Set in the header cache
 * @param mdata Imap Mailbox data
 * @retval  0 Success
 * @retval -1 Error
 */
int imap_hcache_store_uid_seqset(struct ImapMboxData *mdata)
{
  if (!mdata->hcache)
    return -1;

  /* The seqset is likely large.  Preallocate to reduce reallocs */
  struct Buffer buf = mutt_buffer_make(8192);
  imap_msn_index_to_uid_seqset(&buf, mdata);

  int rc = mutt_hcache_store_raw(mdata->hcache, "/UIDSEQSET", 10, buf.data,
                                 mutt_buffer_len(&buf) + 1);
  mutt_debug(LL_DEBUG3, "Stored /UIDSEQSET %s\n", buf.data);
  mutt_buffer_dealloc(&buf);
  return rc;
}

/**
 * imap_hcache_clear_uid_seqset - Delete a UID Sequence Set from the header cache
 * @param mdata Imap Mailbox data
 * @retval  0 Success
 * @retval -1 Error
 */
int imap_hcache_clear_uid_seqset(struct ImapMboxData *mdata)
{
  if (!mdata->hcache)
    return -1;

  return mutt_hcache_delete_header(mdata->hcache, "/UIDSEQSET", 10);
}

/**
 * imap_hcache_get_uid_seqset - Get a UID Sequence Set from the header cache
 * @param mdata Imap Mailbox data
 * @retval ptr  UID Sequence Set
 * @retval NULL Error
 */
char *imap_hcache_get_uid_seqset(struct ImapMboxData *mdata)
{
  if (!mdata->hcache)
    return NULL;

  char *seqset = NULL;
  size_t dlen = 0;
  char *hc_seqset = mutt_hcache_fetch_raw(mdata->hcache, "/UIDSEQSET", 10, &dlen);
  if (hc_seqset)
  {
    seqset = strndup(hc_seqset, dlen);
    mutt_hcache_free_raw(mdata->hcache, (void **) &hc_seqset);
  }
  mutt_debug(LL_DEBUG3, "Retrieved /UIDSEQSET %s\n", NONULL(seqset));

  return seqset;
}
#endif

/**
 * imap_parse_path - Parse an IMAP mailbox name into ConnAccount, name
 * @param path       Mailbox path to parse
 * @param cac        Account credentials
 * @param mailbox    Buffer for mailbox name
 * @param mailboxlen Length of buffer
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Given an IMAP mailbox name, return host, port and a path IMAP servers will
 * recognize.
 */
int imap_parse_path(const char *path, struct ConnAccount *cac, char *mailbox, size_t mailboxlen)
{
  static unsigned short ImapPort = 0;
  static unsigned short ImapsPort = 0;

  if (ImapPort == 0)
  {
    struct servent *service = getservbyname("imap", "tcp");
    if (service)
      ImapPort = ntohs(service->s_port);
    else
      ImapPort = IMAP_PORT;
    mutt_debug(LL_DEBUG3, "Using default IMAP port %d\n", ImapPort);
  }

  if (ImapsPort == 0)
  {
    struct servent *service = getservbyname("imaps", "tcp");
    if (service)
      ImapsPort = ntohs(service->s_port);
    else
      ImapsPort = IMAP_SSL_PORT;
    mutt_debug(LL_DEBUG3, "Using default IMAPS port %d\n", ImapsPort);
  }

  /* Defaults */
  cac->port = ImapPort;
  cac->type = MUTT_ACCT_TYPE_IMAP;
  cac->service = "imap";
  cac->get_field = imap_get_field;

  struct Url *url = url_parse(path);
  if (!url)
    return -1;

  if ((url->scheme != U_IMAP) && (url->scheme != U_IMAPS))
  {
    url_free(&url);
    return -1;
  }

  if ((mutt_account_fromurl(cac, url) < 0) || (cac->host[0] == '\0'))
  {
    url_free(&url);
    return -1;
  }

  if (url->scheme == U_IMAPS)
    cac->flags |= MUTT_ACCT_SSL;

  mutt_str_copy(mailbox, url->path, mailboxlen);

  url_free(&url);

  if ((cac->flags & MUTT_ACCT_SSL) && !(cac->flags & MUTT_ACCT_PORT))
    cac->port = ImapsPort;

  return 0;
}

/**
 * imap_mxcmp - Compare mailbox names, giving priority to INBOX
 * @param mx1 First mailbox name
 * @param mx2 Second mailbox name
 * @retval <0 First mailbox precedes Second mailbox
 * @retval  0 Mailboxes are the same
 * @retval >0 Second mailbox precedes First mailbox
 *
 * Like a normal sort function except that "INBOX" will be sorted to the
 * beginning of the list.
 */
int imap_mxcmp(const char *mx1, const char *mx2)
{
  char *b1 = NULL;
  char *b2 = NULL;
  int rc;

  if (!mx1 || (*mx1 == '\0'))
    mx1 = "INBOX";
  if (!mx2 || (*mx2 == '\0'))
    mx2 = "INBOX";
  if (mutt_istr_equal(mx1, "INBOX") && mutt_istr_equal(mx2, "INBOX"))
  {
    return 0;
  }

  b1 = mutt_mem_malloc(strlen(mx1) + 1);
  b2 = mutt_mem_malloc(strlen(mx2) + 1);

  imap_fix_path('\0', mx1, b1, strlen(mx1) + 1);
  imap_fix_path('\0', mx2, b2, strlen(mx2) + 1);

  rc = mutt_str_cmp(b1, b2);
  FREE(&b1);
  FREE(&b2);

  return rc;
}

/**
 * imap_pretty_mailbox - Prettify an IMAP mailbox name
 * @param path    Mailbox name to be tidied
 * @param pathlen Length of path
 * @param folder  Path to use for '+' abbreviations
 *
 * Called by mutt_pretty_mailbox() to make IMAP paths look nice.
 */
void imap_pretty_mailbox(char *path, size_t pathlen, const char *folder)
{
  struct ConnAccount cac_target = { { 0 } };
  struct ConnAccount cac_home = { { 0 } };
  struct Url url = { 0 };
  char *delim = NULL;
  int tlen;
  int hlen = 0;
  bool home_match = false;
  char target_mailbox[1024];
  char home_mailbox[1024];

  if (imap_parse_path(path, &cac_target, target_mailbox, sizeof(target_mailbox)) < 0)
    return;

  if (imap_path_probe(folder, NULL) != MUTT_IMAP)
    goto fallback;

  if (imap_parse_path(folder, &cac_home, home_mailbox, sizeof(home_mailbox)) < 0)
    goto fallback;

  tlen = mutt_str_len(target_mailbox);
  hlen = mutt_str_len(home_mailbox);

  /* check whether we can do '+' substitution */
  if (tlen && imap_account_match(&cac_home, &cac_target) &&
      mutt_strn_equal(home_mailbox, target_mailbox, hlen))
  {
    if (hlen == 0)
      home_match = true;
    else if (C_ImapDelimChars)
    {
      for (delim = C_ImapDelimChars; *delim != '\0'; delim++)
        if (target_mailbox[hlen] == *delim)
          home_match = true;
    }
  }

  /* do the '+' substitution */
  if (home_match)
  {
    *path++ = '+';
    /* copy remaining path, skipping delimiter */
    if (hlen == 0)
      hlen = -1;
    memcpy(path, target_mailbox + hlen + 1, tlen - hlen - 1);
    path[tlen - hlen - 1] = '\0';
    return;
  }

fallback:
  mutt_account_tourl(&cac_target, &url);
  url.path = target_mailbox;
  url_tostring(&url, path, pathlen, 0);
}

/**
 * imap_continue - display a message and ask the user if they want to go on
 * @param msg  Location of the error
 * @param resp Message for user
 * @retval #QuadOption Result, e.g. #MUTT_NO
 */
enum QuadOption imap_continue(const char *msg, const char *resp)
{
  imap_error(msg, resp);
  return mutt_yesorno(_("Continue?"), MUTT_NO);
}

/**
 * imap_error - show an error and abort
 * @param where Location of the error
 * @param msg   Message for user
 */
void imap_error(const char *where, const char *msg)
{
  mutt_error("%s [%s]", where, msg);
}

/**
 * imap_fix_path - Fix up the imap path
 * @param delim     Delimiter specified by the server, '\0' for C_ImapDelimChars
 * @param mailbox   Mailbox path
 * @param path      Buffer for the result
 * @param plen      Length of buffer
 * @retval ptr      Fixed-up path
 *
 * @note if delim is '\0', the first character in mailbox matching any of the
 * characters in C_ImapDelimChars is used as a delimiter.
 *
 * This is necessary because the rest of neomutt assumes a hierarchy delimiter of
 * '/', which is not necessarily true in IMAP.  Additionally, the filesystem
 * converts multiple hierarchy delimiters into a single one, ie "///" is equal
 * to "/".  IMAP servers are not required to do this.
 * Moreover, IMAP servers may dislike the path ending with the delimiter.
 */
char *imap_fix_path(char delim, const char *mailbox, char *path, size_t plen)
{
  int i = 0;
  for (; mailbox && *mailbox && (i < plen - 1); i++)
  {
    if (*mailbox == delim || (!delim && strchr(NONULL(C_ImapDelimChars), *mailbox)))
    {
      delim = *mailbox;
      /* Skip multiple occurrences of delim */
      while (*mailbox && *(mailbox + 1) == delim)
        mailbox++;
    }
    path[i] = *mailbox++;
  }

  /* Do not terminate with a delimiter */
  if (i && path[i - 1] == delim)
    i--;

  /* Ensure null termination */
  path[i] = '\0';
  return path;
}

/**
 * imap_cachepath - Generate a cache path for a mailbox
 * @param delim   Imap server delimiter
 * @param mailbox Mailbox name
 * @param dest    Buffer to store cache path
 */
void imap_cachepath(char delim, const char *mailbox, struct Buffer *dest)
{
  const char *p = mailbox;
  mutt_buffer_reset(dest);
  if (!p)
    return;

  while (*p)
  {
    if (p[0] == delim)
    {
      mutt_buffer_addch(dest, '/');
      /* simple way to avoid collisions with UIDs */
      if ((p[1] >= '0') && (p[1] <= '9'))
        mutt_buffer_addch(dest, '_');
    }
    else
      mutt_buffer_addch(dest, *p);
    p++;
  }
}

/**
 * imap_get_literal_count - write number of bytes in an IMAP literal into bytes
 * @param[in]  buf   Number as a string
 * @param[out] bytes Resulting number
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_get_literal_count(const char *buf, unsigned int *bytes)
{
  char *pc = NULL;
  char *pn = NULL;

  if (!buf || !(pc = strchr(buf, '{')))
    return -1;

  pc++;
  pn = pc;
  while (isdigit((unsigned char) *pc))
    pc++;
  *pc = '\0';
  if (mutt_str_atoui(pn, bytes) < 0)
    return -1;

  return 0;
}

/**
 * imap_get_qualifier - Get the qualifier from a tagged response
 * @param buf Command string to process
 * @retval ptr Start of the qualifier
 *
 * In a tagged response, skip tag and status for the qualifier message.
 * Used by imap_copy_message for TRYCREATE
 */
char *imap_get_qualifier(char *buf)
{
  char *s = buf;

  /* skip tag */
  s = imap_next_word(s);
  /* skip OK/NO/BAD response */
  s = imap_next_word(s);

  return s;
}

/**
 * imap_next_word - Find where the next IMAP word begins
 * @param s Command string to process
 * @retval ptr Next IMAP word
 */
char *imap_next_word(char *s)
{
  bool quoted = false;

  while (*s)
  {
    if (*s == '\\')
    {
      s++;
      if (*s)
        s++;
      continue;
    }
    if (*s == '\"')
      quoted = !quoted;
    if (!quoted && IS_SPACE(*s))
      break;
    s++;
  }

  SKIPWS(s);
  return s;
}

/**
 * imap_qualify_path - Make an absolute IMAP folder target
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param cac    ConnAccount of the account
 * @param path   Path relative to the mailbox
 */
void imap_qualify_path(char *buf, size_t buflen, struct ConnAccount *cac, char *path)
{
  struct Url url = { 0 };
  mutt_account_tourl(cac, &url);
  url.path = path;
  url_tostring(&url, buf, buflen, 0);
}

/**
 * imap_quote_string - quote string according to IMAP rules
 * @param dest           Buffer for the result
 * @param dlen           Length of the buffer
 * @param src            String to be quoted
 * @param quote_backtick If true, quote backticks too
 *
 * Surround string with quotes, escape " and \ with backslash
 */
void imap_quote_string(char *dest, size_t dlen, const char *src, bool quote_backtick)
{
  const char *quote = "`\"\\";
  if (!quote_backtick)
    quote++;

  char *pt = dest;
  const char *s = src;

  *pt++ = '"';
  /* save room for quote-chars */
  dlen -= 3;

  for (; *s && dlen; s++)
  {
    if (strchr(quote, *s))
    {
      if (dlen < 2)
        break;
      dlen -= 2;
      *pt++ = '\\';
      *pt++ = *s;
    }
    else
    {
      *pt++ = *s;
      dlen--;
    }
  }
  *pt++ = '"';
  *pt = '\0';
}

/**
 * imap_unquote_string - equally stupid unquoting routine
 * @param s String to be unquoted
 */
void imap_unquote_string(char *s)
{
  char *d = s;

  if (*s == '\"')
    s++;
  else
    return;

  while (*s)
  {
    if (*s == '\"')
    {
      *d = '\0';
      return;
    }
    if (*s == '\\')
    {
      s++;
    }
    if (*s)
    {
      *d = *s;
      d++;
      s++;
    }
  }
  *d = '\0';
}

/**
 * imap_munge_mbox_name - Quote awkward characters in a mailbox name
 * @param unicode true if Unicode is allowed
 * @param dest    Buffer to store safe mailbox name
 * @param dlen    Length of buffer
 * @param src     Mailbox name
 */
void imap_munge_mbox_name(bool unicode, char *dest, size_t dlen, const char *src)
{
  char *buf = mutt_str_dup(src);
  imap_utf_encode(unicode, &buf);

  imap_quote_string(dest, dlen, buf, false);

  FREE(&buf);
}

/**
 * imap_unmunge_mbox_name - Remove quoting from a mailbox name
 * @param unicode true if Unicode is allowed
 * @param s       Mailbox name
 *
 * The string will be altered in-place.
 */
void imap_unmunge_mbox_name(bool unicode, char *s)
{
  imap_unquote_string(s);

  char *buf = mutt_str_dup(s);
  if (buf)
  {
    imap_utf_decode(unicode, &buf);
    strncpy(s, buf, strlen(s));
  }

  FREE(&buf);
}

/**
 * imap_keepalive - poll the current folder to keep the connection alive
 */
void imap_keepalive(void)
{
  time_t now = mutt_date_epoch();
  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    if (np->type != MUTT_IMAP)
      continue;

    struct ImapAccountData *adata = np->adata;
    if (!adata || !adata->mailbox)
      continue;

    if ((adata->state >= IMAP_AUTHENTICATED) && (now >= (adata->lastread + C_ImapKeepalive)))
      imap_check_mailbox(adata->mailbox, true);
  }
}

/**
 * imap_wait_keepalive - Wait for a process to change state
 * @param pid Process ID to listen to
 * @retval num 'wstatus' from waitpid()
 */
int imap_wait_keepalive(pid_t pid)
{
  struct sigaction oldalrm;
  struct sigaction act;
  sigset_t oldmask;
  int rc;

  bool imap_passive = C_ImapPassive;

  C_ImapPassive = true;
  OptKeepQuiet = true;

  sigprocmask(SIG_SETMASK, NULL, &oldmask);

  sigemptyset(&act.sa_mask);
  act.sa_handler = mutt_sig_empty_handler;
#ifdef SA_INTERRUPT
  act.sa_flags = SA_INTERRUPT;
#else
  act.sa_flags = 0;
#endif

  sigaction(SIGALRM, &act, &oldalrm);

  alarm(C_ImapKeepalive);
  while ((waitpid(pid, &rc, 0) < 0) && (errno == EINTR))
  {
    alarm(0); /* cancel a possibly pending alarm */
    imap_keepalive();
    alarm(C_ImapKeepalive);
  }

  alarm(0); /* cancel a possibly pending alarm */

  sigaction(SIGALRM, &oldalrm, NULL);
  sigprocmask(SIG_SETMASK, &oldmask, NULL);

  OptKeepQuiet = false;
  if (!imap_passive)
    C_ImapPassive = false;

  return rc;
}

/**
 * imap_allow_reopen - Allow re-opening a folder upon expunge
 * @param m Mailbox
 */
void imap_allow_reopen(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || !adata->mailbox || (adata->mailbox != m) || !mdata)
    return;
  mdata->reopen |= IMAP_REOPEN_ALLOW;
}

/**
 * imap_disallow_reopen - Disallow re-opening a folder upon expunge
 * @param m Mailbox
 */
void imap_disallow_reopen(struct Mailbox *m)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  struct ImapMboxData *mdata = imap_mdata_get(m);
  if (!adata || !adata->mailbox || (adata->mailbox != m) || !mdata)
    return;
  mdata->reopen &= ~IMAP_REOPEN_ALLOW;
}

/**
 * imap_account_match - Compare two Accounts
 * @param a1 First ConnAccount
 * @param a2 Second ConnAccount
 * @retval true Accounts match
 */
bool imap_account_match(const struct ConnAccount *a1, const struct ConnAccount *a2)
{
  if (!a1 || !a2)
    return false;
  if (a1->type != a2->type)
    return false;
  if (!mutt_istr_equal(a1->host, a2->host))
    return false;
  if ((a1->port != 0) && (a2->port != 0) && (a1->port != a2->port))
    return false;
  if (a1->flags & a2->flags & MUTT_ACCT_USER)
    return strcmp(a1->user, a2->user) == 0;

  const char *user = NONULL(Username);

  if ((a1->type == MUTT_ACCT_TYPE_IMAP) && C_ImapUser)
    user = C_ImapUser;

  if (a1->flags & MUTT_ACCT_USER)
    return strcmp(a1->user, user) == 0;
  if (a2->flags & MUTT_ACCT_USER)
    return strcmp(a2->user, user) == 0;

  return true;
}

/**
 * mutt_seqset_iterator_new - Create a new Sequence Set Iterator
 * @param seqset Source Sequence Set
 * @retval ptr Newly allocated Sequence Set Iterator
 */
struct SeqsetIterator *mutt_seqset_iterator_new(const char *seqset)
{
  if (!seqset || (*seqset == '\0'))
    return NULL;

  struct SeqsetIterator *iter = mutt_mem_calloc(1, sizeof(struct SeqsetIterator));
  iter->full_seqset = mutt_str_dup(seqset);
  iter->eostr = strchr(iter->full_seqset, '\0');
  iter->substr_cur = iter->substr_end = iter->full_seqset;

  return iter;
}

/**
 * mutt_seqset_iterator_next - Get the next UID from a Sequence Set
 * @param[in]  iter Sequence Set Iterator
 * @param[out] next Next UID in set
 * @retval  0 Next sequence is generated
 * @retval  1 Iterator is finished
 * @retval -1 error
 */
int mutt_seqset_iterator_next(struct SeqsetIterator *iter, unsigned int *next)
{
  if (!iter || !next)
    return -1;

  if (iter->in_range)
  {
    if ((iter->down && (iter->range_cur == (iter->range_end - 1))) ||
        (!iter->down && (iter->range_cur == (iter->range_end + 1))))
    {
      iter->in_range = 0;
    }
  }

  if (!iter->in_range)
  {
    iter->substr_cur = iter->substr_end;
    if (iter->substr_cur == iter->eostr)
      return 1;

    while (!*(iter->substr_cur))
      iter->substr_cur++;
    iter->substr_end = strchr(iter->substr_cur, ',');
    if (!iter->substr_end)
      iter->substr_end = iter->eostr;
    else
      *(iter->substr_end) = '\0';

    char *range_sep = strchr(iter->substr_cur, ':');
    if (range_sep)
      *range_sep++ = '\0';

    if (mutt_str_atoui(iter->substr_cur, &iter->range_cur) != 0)
      return -1;
    if (range_sep)
    {
      if (mutt_str_atoui(range_sep, &iter->range_end) != 0)
        return -1;
    }
    else
      iter->range_end = iter->range_cur;

    iter->down = (iter->range_end < iter->range_cur);
    iter->in_range = 1;
  }

  *next = iter->range_cur;
  if (iter->down)
    iter->range_cur--;
  else
    iter->range_cur++;

  return 0;
}

/**
 * mutt_seqset_iterator_free - Free a Sequence Set Iterator
 * @param[out] ptr Iterator to free
 */
void mutt_seqset_iterator_free(struct SeqsetIterator **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct SeqsetIterator *iter = *ptr;
  FREE(&iter->full_seqset);
  FREE(ptr);
}
