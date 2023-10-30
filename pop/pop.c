/**
 * @file
 * POP network mailbox
 *
 * @authors
 * Copyright (C) 2000-2002 Vsevolod Volkov <vvv@mutt.org.ua>
 * Copyright (C) 2006-2007,2009 Rocco Rutte <pdmef@gmx.net>
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
 * @page pop_pop POP network mailbox
 *
 * POP network mailbox
 *
 * Implementation: #MxPopOps
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "lib.h"
#include "bcache/lib.h"
#include "ncrypt/lib.h"
#include "progress/lib.h"
#include "question/lib.h"
#include "adata.h"
#include "edata.h"
#include "hook.h"
#include "mutt_account.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "mx.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_HCACHE
#include "hcache/lib.h"
#endif

struct BodyCache;
struct stat;

#define HC_FNAME "neomutt" /* filename for hcache as POP lacks paths */
#define HC_FEXT "hcache"   /* extension for hcache as POP lacks paths */

/**
 * cache_id - Make a message-cache-compatible id
 * @param id POP message id
 * @retval ptr Sanitised string
 *
 * The POP message id may contain '/' and other awkward characters.
 *
 * @note This function returns a pointer to a static buffer.
 */
static const char *cache_id(const char *id)
{
  static char clean[128];
  mutt_str_copy(clean, id, sizeof(clean));
  mutt_file_sanitize_filename(clean, true);
  return clean;
}

/**
 * fetch_message - Parse a Message response - Implements ::pop_fetch_t - @ingroup pop_fetch_api
 * @param line String to write
 * @param data FILE pointer to write to
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Save a Message to a file.
 */
static int fetch_message(const char *line, void *data)
{
  FILE *fp = data;

  fputs(line, fp);
  if (fputc('\n', fp) == EOF)
    return -1;

  return 0;
}

/**
 * pop_read_header - Read header
 * @param adata POP Account data
 * @param e     Email
 * @retval  0 Success
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Error writing to tempfile
 */
static int pop_read_header(struct PopAccountData *adata, struct Email *e)
{
  FILE *fp = mutt_file_mkstemp();
  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    return -3;
  }

  int index = 0;
  size_t length = 0;
  char buf[1024] = { 0 };

  struct PopEmailData *edata = pop_edata_get(e);

  snprintf(buf, sizeof(buf), "LIST %d\r\n", edata->refno);
  int rc = pop_query(adata, buf, sizeof(buf));
  if (rc == 0)
  {
    sscanf(buf, "+OK %d %zu", &index, &length);

    snprintf(buf, sizeof(buf), "TOP %d 0\r\n", edata->refno);
    rc = pop_fetch_data(adata, buf, NULL, fetch_message, fp);

    if (adata->cmd_top == 2)
    {
      if (rc == 0)
      {
        adata->cmd_top = 1;

        mutt_debug(LL_DEBUG1, "set TOP capability\n");
      }

      if (rc == -2)
      {
        adata->cmd_top = 0;

        mutt_debug(LL_DEBUG1, "unset TOP capability\n");
        snprintf(adata->err_msg, sizeof(adata->err_msg), "%s",
                 _("Command TOP is not supported by server"));
      }
    }
  }

  switch (rc)
  {
    case 0:
    {
      rewind(fp);
      e->env = mutt_rfc822_read_header(fp, e, false, false);
      e->body->length = length - e->body->offset + 1;
      rewind(fp);
      while (!feof(fp))
      {
        e->body->length--;
        if (!fgets(buf, sizeof(buf), fp))
          break;
      }
      break;
    }
    case -2:
    {
      mutt_error("%s", adata->err_msg);
      break;
    }
    case -3:
    {
      mutt_error(_("Can't write header to temporary file"));
      break;
    }
  }

  mutt_file_fclose(&fp);
  return rc;
}

/**
 * fetch_uidl - Parse UIDL response - Implements ::pop_fetch_t - @ingroup pop_fetch_api
 * @param line String to parse
 * @param data Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
static int fetch_uidl(const char *line, void *data)
{
  struct Mailbox *m = data;
  struct PopAccountData *adata = pop_adata_get(m);
  char *endp = NULL;

  errno = 0;
  int index = strtol(line, &endp, 10);
  if (errno)
    return -1;
  while (*endp == ' ')
    endp++;
  line = endp;

  /* uid must be at least be 1 byte */
  if (strlen(line) == 0)
    return -1;

  int i;
  for (i = 0; i < m->msg_count; i++)
  {
    struct PopEmailData *edata = pop_edata_get(m->emails[i]);
    if (mutt_str_equal(line, edata->uid))
      break;
  }

  if (i == m->msg_count)
  {
    mutt_debug(LL_DEBUG1, "new header %d %s\n", index, line);

    mx_alloc_memory(m, i);

    m->msg_count++;
    m->emails[i] = email_new();

    m->emails[i]->edata = pop_edata_new(line);
    m->emails[i]->edata_free = pop_edata_free;
  }
  else if (m->emails[i]->index != index - 1)
  {
    adata->clear_cache = true;
  }

  m->emails[i]->index = index - 1;

  struct PopEmailData *edata = pop_edata_get(m->emails[i]);
  edata->refno = index;

  return 0;
}

/**
 * pop_bcache_delete - Delete an entry from the message cache - Implements ::bcache_list_t - @ingroup bcache_list_api
 */
static int pop_bcache_delete(const char *id, struct BodyCache *bcache, void *data)
{
  struct Mailbox *m = data;
  if (!m)
    return -1;

  struct PopAccountData *adata = pop_adata_get(m);
  if (!adata)
    return -1;

#ifdef USE_HCACHE
  /* keep hcache file if hcache == bcache */
  if (mutt_str_equal(HC_FNAME "." HC_FEXT, id))
    return 0;
#endif

  for (int i = 0; i < m->msg_count; i++)
  {
    struct PopEmailData *edata = pop_edata_get(m->emails[i]);
    /* if the id we get is known for a header: done (i.e. keep in cache) */
    if (edata->uid && mutt_str_equal(edata->uid, id))
      return 0;
  }

  /* message not found in context -> remove it from cache
   * return the result of bcache, so we stop upon its first error */
  return mutt_bcache_del(bcache, cache_id(id));
}

#ifdef USE_HCACHE
/**
 * pop_hcache_namer - Create a header cache filename for a POP mailbox - Implements ::hcache_namer_t - @ingroup hcache_namer_api
 */
static void pop_hcache_namer(const char *path, struct Buffer *dest)
{
  buf_printf(dest, "%s." HC_FEXT, path);
}

/**
 * pop_hcache_open - Open the header cache
 * @param adata POP Account data
 * @param path  Path to the mailbox
 * @retval ptr Header cache
 */
static struct HeaderCache *pop_hcache_open(struct PopAccountData *adata, const char *path)
{
  const char *const c_header_cache = cs_subset_path(NeoMutt->sub, "header_cache");
  if (!adata || !adata->conn)
    return hcache_open(c_header_cache, path, NULL);

  struct Url url = { 0 };
  char p[1024] = { 0 };

  mutt_account_tourl(&adata->conn->account, &url);
  url.path = HC_FNAME;
  url_tostring(&url, p, sizeof(p), U_PATH);
  return hcache_open(c_header_cache, p, pop_hcache_namer);
}
#endif

/**
 * pop_fetch_headers - Read headers
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Error writing to tempfile
 */
static int pop_fetch_headers(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct PopAccountData *adata = pop_adata_get(m);
  struct Progress *progress = NULL;

#ifdef USE_HCACHE
  struct HeaderCache *hc = pop_hcache_open(adata, mailbox_path(m));
#endif

  adata->check_time = mutt_date_now();
  adata->clear_cache = false;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct PopEmailData *edata = pop_edata_get(m->emails[i]);
    edata->refno = -1;
  }

  const int old_count = m->msg_count;
  int rc = pop_fetch_data(adata, "UIDL\r\n", NULL, fetch_uidl, m);
  const int new_count = m->msg_count;
  m->msg_count = old_count;

  if (adata->cmd_uidl == 2)
  {
    if (rc == 0)
    {
      adata->cmd_uidl = 1;

      mutt_debug(LL_DEBUG1, "set UIDL capability\n");
    }

    if ((rc == -2) && (adata->cmd_uidl == 2))
    {
      adata->cmd_uidl = 0;

      mutt_debug(LL_DEBUG1, "unset UIDL capability\n");
      snprintf(adata->err_msg, sizeof(adata->err_msg), "%s",
               _("Command UIDL is not supported by server"));
    }
  }

  if (m->verbose)
  {
    progress = progress_new(_("Fetching message headers..."),
                            MUTT_PROGRESS_READ, new_count - old_count);
  }

  if (rc == 0)
  {
    int i, deleted;
    for (i = 0, deleted = 0; i < old_count; i++)
    {
      struct PopEmailData *edata = pop_edata_get(m->emails[i]);
      if (edata->refno == -1)
      {
        m->emails[i]->deleted = true;
        deleted++;
      }
    }
    if (deleted > 0)
    {
      mutt_error(ngettext("%d message has been lost. Try reopening the mailbox.",
                          "%d messages have been lost. Try reopening the mailbox.", deleted),
                 deleted);
    }

    bool hcached = false;
    for (i = old_count; i < new_count; i++)
    {
      progress_update(progress, i + 1 - old_count, -1);
      struct PopEmailData *edata = pop_edata_get(m->emails[i]);
#ifdef USE_HCACHE
      struct HCacheEntry hce = hcache_fetch(hc, edata->uid, strlen(edata->uid), 0);
      if (hce.email)
      {
        /* Detach the private data */
        m->emails[i]->edata = NULL;

        int index = m->emails[i]->index;
        /* - POP dynamically numbers headers and relies on e->refno
         *   to map messages; so restore header and overwrite restored
         *   refno with current refno, same for index
         * - e->data needs to a separate pointer as it's driver-specific
         *   data freed separately elsewhere
         *   (the old e->data should point inside a malloc'd block from
         *   hcache so there shouldn't be a memleak here) */
        email_free(&m->emails[i]);
        m->emails[i] = hce.email;
        m->emails[i]->index = index;

        /* Reattach the private data */
        m->emails[i]->edata = edata;
        m->emails[i]->edata_free = pop_edata_free;
        rc = 0;
        hcached = true;
      }
      else
#endif
          if ((rc = pop_read_header(adata, m->emails[i])) < 0)
        break;
#ifdef USE_HCACHE
      else
      {
        hcache_store(hc, edata->uid, strlen(edata->uid), m->emails[i], 0);
      }
#endif

      /* faked support for flags works like this:
       * - if 'hcached' is true, we have the message in our hcache:
       *        - if we also have a body: read
       *        - if we don't have a body: old
       *          (if $mark_old is set which is maybe wrong as
       *          $mark_old should be considered for syncing the
       *          folder and not when opening it XXX)
       * - if 'hcached' is false, we don't have the message in our hcache:
       *        - if we also have a body: read
       *        - if we don't have a body: new */
      const bool bcached = (mutt_bcache_exists(adata->bcache, cache_id(edata->uid)) == 0);
      m->emails[i]->old = false;
      m->emails[i]->read = false;
      if (hcached)
      {
        const bool c_mark_old = cs_subset_bool(NeoMutt->sub, "mark_old");
        if (bcached)
          m->emails[i]->read = true;
        else if (c_mark_old)
          m->emails[i]->old = true;
      }
      else
      {
        if (bcached)
          m->emails[i]->read = true;
      }

      m->msg_count++;
    }
  }
  progress_free(&progress);

#ifdef USE_HCACHE
  hcache_close(&hc);
#endif

  if (rc < 0)
  {
    for (int i = m->msg_count; i < new_count; i++)
      email_free(&m->emails[i]);
    return rc;
  }

  /* after putting the result into our structures,
   * clean up cache, i.e. wipe messages deleted outside
   * the availability of our cache */
  const bool c_message_cache_clean = cs_subset_bool(NeoMutt->sub, "message_cache_clean");
  if (c_message_cache_clean)
    mutt_bcache_list(adata->bcache, pop_bcache_delete, m);

  mutt_clear_error();
  return new_count - old_count;
}

/**
 * pop_clear_cache - Delete all cached messages
 * @param adata POP Account data
 */
static void pop_clear_cache(struct PopAccountData *adata)
{
  if (!adata->clear_cache)
    return;

  mutt_debug(LL_DEBUG1, "delete cached messages\n");

  for (int i = 0; i < POP_CACHE_LEN; i++)
  {
    if (adata->cache[i].path)
    {
      unlink(adata->cache[i].path);
      FREE(&adata->cache[i].path);
    }
  }
}

/**
 * pop_fetch_mail - Fetch messages and save them in $spool_file
 */
void pop_fetch_mail(void)
{
  const char *const c_pop_host = cs_subset_string(NeoMutt->sub, "pop_host");
  if (!c_pop_host)
  {
    mutt_error(_("POP host is not defined"));
    return;
  }

  char buf[1024] = { 0 };
  char msgbuf[128] = { 0 };
  int last = 0, msgs, bytes, rset = 0, rc;
  struct ConnAccount cac = { { 0 } };

  char *p = mutt_mem_calloc(strlen(c_pop_host) + 7, sizeof(char));
  char *url = p;
  if (url_check_scheme(c_pop_host) == U_UNKNOWN)
  {
    strcpy(url, "pop://");
    p = strchr(url, '\0');
  }
  strcpy(p, c_pop_host);

  rc = pop_parse_path(url, &cac);
  FREE(&url);
  if (rc)
  {
    mutt_error(_("%s is an invalid POP path"), c_pop_host);
    return;
  }

  struct Connection *conn = mutt_conn_find(&cac);
  if (!conn)
    return;

  struct PopAccountData *adata = pop_adata_new();
  adata->conn = conn;

  if (pop_open_connection(adata) < 0)
  {
    pop_adata_free((void **) &adata);
    return;
  }

  mutt_message(_("Checking for new messages..."));

  /* find out how many messages are in the mailbox. */
  mutt_str_copy(buf, "STAT\r\n", sizeof(buf));
  rc = pop_query(adata, buf, sizeof(buf));
  if (rc == -1)
    goto fail;
  if (rc == -2)
  {
    mutt_error("%s", adata->err_msg);
    goto finish;
  }

  sscanf(buf, "+OK %d %d", &msgs, &bytes);

  /* only get unread messages */
  const bool c_pop_last = cs_subset_bool(NeoMutt->sub, "pop_last");
  if ((msgs > 0) && c_pop_last)
  {
    mutt_str_copy(buf, "LAST\r\n", sizeof(buf));
    rc = pop_query(adata, buf, sizeof(buf));
    if (rc == -1)
      goto fail;
    if (rc == 0)
      sscanf(buf, "+OK %d", &last);
  }

  if (msgs <= last)
  {
    mutt_message(_("No new mail in POP mailbox"));
    goto finish;
  }

  const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
  struct Mailbox *m_spool = mx_path_resolve(c_spool_file);

  if (!mx_mbox_open(m_spool, MUTT_OPEN_NO_FLAGS))
  {
    mailbox_free(&m_spool);
    goto finish;
  }
  bool old_append = m_spool->append;
  m_spool->append = true;

  enum QuadOption delanswer = query_quadoption(_("Delete messages from server?"),
                                               NeoMutt->sub, "pop_delete");

  snprintf(msgbuf, sizeof(msgbuf),
           ngettext("Reading new messages (%d byte)...",
                    "Reading new messages (%d bytes)...", bytes),
           bytes);
  mutt_message("%s", msgbuf);

  for (int i = last + 1; i <= msgs; i++)
  {
    struct Message *msg = mx_msg_open_new(m_spool, NULL, MUTT_ADD_FROM);
    if (msg)
    {
      snprintf(buf, sizeof(buf), "RETR %d\r\n", i);
      rc = pop_fetch_data(adata, buf, NULL, fetch_message, msg->fp);
      if (rc == -3)
        rset = 1;

      if ((rc == 0) && (mx_msg_commit(m_spool, msg) != 0))
      {
        rset = 1;
        rc = -3;
      }

      mx_msg_close(m_spool, &msg);
    }
    else
    {
      rc = -3;
    }

    if ((rc == 0) && (delanswer == MUTT_YES))
    {
      /* delete the message on the server */
      snprintf(buf, sizeof(buf), "DELE %d\r\n", i);
      rc = pop_query(adata, buf, sizeof(buf));
    }

    if (rc == -1)
    {
      m_spool->append = old_append;
      mx_mbox_close(m_spool);
      goto fail;
    }
    if (rc == -2)
    {
      mutt_error("%s", adata->err_msg);
      break;
    }
    if (rc == -3)
    {
      mutt_error(_("Error while writing mailbox"));
      break;
    }

    /* L10N: The plural is picked by the second numerical argument, i.e.
       the %d right before 'messages', i.e. the total number of messages. */
    mutt_message(ngettext("%s [%d of %d message read]",
                          "%s [%d of %d messages read]", msgs - last),
                 msgbuf, i - last, msgs - last);
  }

  m_spool->append = old_append;
  mx_mbox_close(m_spool);

  if (rset)
  {
    /* make sure no messages get deleted */
    mutt_str_copy(buf, "RSET\r\n", sizeof(buf));
    if (pop_query(adata, buf, sizeof(buf)) == -1)
      goto fail;
  }

finish:
  /* exit gracefully */
  mutt_str_copy(buf, "QUIT\r\n", sizeof(buf));
  if (pop_query(adata, buf, sizeof(buf)) == -1)
    goto fail;
  mutt_socket_close(conn);
  pop_adata_free((void **) &adata);
  return;

fail:
  mutt_error(_("Server closed connection"));
  mutt_socket_close(conn);
  pop_adata_free((void **) &adata);
}

/**
 * pop_ac_owns_path - Check whether an Account owns a Mailbox path - Implements MxOps::ac_owns_path() - @ingroup mx_ac_owns_path
 */
static bool pop_ac_owns_path(struct Account *a, const char *path)
{
  struct Url *url = url_parse(path);
  if (!url)
    return false;

  struct PopAccountData *adata = a->adata;
  struct ConnAccount *cac = &adata->conn->account;

  const bool rc = mutt_istr_equal(url->host, cac->host) &&
                  mutt_istr_equal(url->user, cac->user);
  url_free(&url);
  return rc;
}

/**
 * pop_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add() - @ingroup mx_ac_add
 */
static bool pop_ac_add(struct Account *a, struct Mailbox *m)
{
  if (a->adata)
    return true;

  struct ConnAccount cac = { { 0 } };
  if (pop_parse_path(mailbox_path(m), &cac))
  {
    mutt_error(_("%s is an invalid POP path"), mailbox_path(m));
    return false;
  }

  struct PopAccountData *adata = pop_adata_new();
  adata->conn = mutt_conn_new(&cac);
  if (!adata->conn)
  {
    pop_adata_free((void **) &adata);
    return false;
  }
  a->adata = adata;
  a->adata_free = pop_adata_free;

  return true;
}

/**
 * pop_mbox_open - Open a Mailbox - Implements MxOps::mbox_open() - @ingroup mx_mbox_open
 *
 * Fetch only headers
 */
static enum MxOpenReturns pop_mbox_open(struct Mailbox *m)
{
  if (!m->account)
    return MX_OPEN_ERROR;

  char buf[PATH_MAX] = { 0 };
  struct ConnAccount cac = { { 0 } };
  struct Url url = { 0 };

  if (pop_parse_path(mailbox_path(m), &cac))
  {
    mutt_error(_("%s is an invalid POP path"), mailbox_path(m));
    return MX_OPEN_ERROR;
  }

  mutt_account_tourl(&cac, &url);
  url.path = NULL;
  url_tostring(&url, buf, sizeof(buf), U_NO_FLAGS);

  buf_strcpy(&m->pathbuf, buf);
  mutt_str_replace(&m->realpath, mailbox_path(m));

  struct PopAccountData *adata = m->account->adata;
  if (!adata)
  {
    adata = pop_adata_new();
    m->account->adata = adata;
    m->account->adata_free = pop_adata_free;
  }

  struct Connection *conn = adata->conn;
  if (!conn)
  {
    adata->conn = mutt_conn_new(&cac);
    conn = adata->conn;
    if (!conn)
      return MX_OPEN_ERROR;
  }

  if (conn->fd < 0)
    mutt_account_hook(m->realpath);

  if (pop_open_connection(adata) < 0)
    return MX_OPEN_ERROR;

  adata->bcache = mutt_bcache_open(&cac, NULL);

  /* init (hard-coded) ACL rights */
  m->rights = MUTT_ACL_SEEN | MUTT_ACL_DELETE;
#ifdef USE_HCACHE
  /* flags are managed using header cache, so it only makes sense to
   * enable them in that case */
  m->rights |= MUTT_ACL_WRITE;
#endif

  while (true)
  {
    if (pop_reconnect(m) < 0)
      return MX_OPEN_ERROR;

    m->size = adata->size;

    mutt_message(_("Fetching list of messages..."));

    const int rc = pop_fetch_headers(m);

    if (rc >= 0)
      return MX_OPEN_OK;

    if (rc < -1)
      return MX_OPEN_ERROR;
  }
}

/**
 * pop_mbox_check - Check for new mail - Implements MxOps::mbox_check() - @ingroup mx_mbox_check
 */
static enum MxStatus pop_mbox_check(struct Mailbox *m)
{
  if (!m || !m->account)
    return MX_STATUS_ERROR;

  struct PopAccountData *adata = pop_adata_get(m);

  const short c_pop_check_interval = cs_subset_number(NeoMutt->sub, "pop_check_interval");
  if ((adata->check_time + c_pop_check_interval) > mutt_date_now())
    return MX_STATUS_OK;

  pop_logout(m);

  mutt_socket_close(adata->conn);

  if (pop_open_connection(adata) < 0)
    return MX_STATUS_ERROR;

  m->size = adata->size;

  mutt_message(_("Checking for new messages..."));

  int old_msg_count = m->msg_count;
  int rc = pop_fetch_headers(m);
  pop_clear_cache(adata);
  if (m->msg_count > old_msg_count)
    mailbox_changed(m, NT_MAILBOX_INVALID);

  if (rc < 0)
    return MX_STATUS_ERROR;

  if (rc > 0)
    return MX_STATUS_NEW_MAIL;

  return MX_STATUS_OK;
}

/**
 * pop_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync() - @ingroup mx_mbox_sync
 *
 * Update POP mailbox, delete messages from server
 */
static enum MxStatus pop_mbox_sync(struct Mailbox *m)
{
  int i, j, rc = 0;
  char buf[1024] = { 0 };
  struct PopAccountData *adata = pop_adata_get(m);
#ifdef USE_HCACHE
  struct HeaderCache *hc = NULL;
#endif

  adata->check_time = 0;

  int num_deleted = 0;
  for (i = 0; i < m->msg_count; i++)
  {
    if (m->emails[i]->deleted)
      num_deleted++;
  }

  while (true)
  {
    if (pop_reconnect(m) < 0)
      return MX_STATUS_ERROR;

#ifdef USE_HCACHE
    hc = pop_hcache_open(adata, mailbox_path(m));
#endif

    struct Progress *progress = NULL;
    if (m->verbose)
    {
      progress = progress_new(_("Marking messages deleted..."),
                              MUTT_PROGRESS_WRITE, num_deleted);
    }

    for (i = 0, j = 0, rc = 0; (rc == 0) && (i < m->msg_count); i++)
    {
      struct PopEmailData *edata = pop_edata_get(m->emails[i]);
      if (m->emails[i]->deleted && (edata->refno != -1))
      {
        j++;
        progress_update(progress, j, -1);
        snprintf(buf, sizeof(buf), "DELE %d\r\n", edata->refno);
        rc = pop_query(adata, buf, sizeof(buf));
        if (rc == 0)
        {
          mutt_bcache_del(adata->bcache, cache_id(edata->uid));
#ifdef USE_HCACHE
          hcache_delete_record(hc, edata->uid, strlen(edata->uid));
#endif
        }
      }

#ifdef USE_HCACHE
      if (m->emails[i]->changed)
      {
        hcache_store(hc, edata->uid, strlen(edata->uid), m->emails[i], 0);
      }
#endif
    }
    progress_free(&progress);

#ifdef USE_HCACHE
    hcache_close(&hc);
#endif

    if (rc == 0)
    {
      mutt_str_copy(buf, "QUIT\r\n", sizeof(buf));
      rc = pop_query(adata, buf, sizeof(buf));
    }

    if (rc == 0)
    {
      adata->clear_cache = true;
      pop_clear_cache(adata);
      adata->status = POP_DISCONNECTED;
      return MX_STATUS_OK;
    }

    if (rc == -2)
    {
      mutt_error("%s", adata->err_msg);
      return MX_STATUS_ERROR;
    }
  }
}

/**
 * pop_mbox_close - Close a Mailbox - Implements MxOps::mbox_close() - @ingroup mx_mbox_close
 */
static enum MxStatus pop_mbox_close(struct Mailbox *m)
{
  struct PopAccountData *adata = pop_adata_get(m);
  if (!adata)
    return MX_STATUS_OK;

  pop_logout(m);

  if (adata->status != POP_NONE)
  {
    mutt_socket_close(adata->conn);
  }

  adata->status = POP_NONE;

  adata->clear_cache = true;
  pop_clear_cache(adata);

  mutt_bcache_close(&adata->bcache);

  return MX_STATUS_OK;
}

/**
 * pop_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open() - @ingroup mx_msg_open
 */
static bool pop_msg_open(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  char buf[1024] = { 0 };
  struct PopAccountData *adata = pop_adata_get(m);
  struct PopEmailData *edata = pop_edata_get(e);
  bool bcache = true;
  bool success = false;
  struct Buffer *path = NULL;

  /* see if we already have the message in body cache */
  msg->fp = mutt_bcache_get(adata->bcache, cache_id(edata->uid));
  if (msg->fp)
    return true;

  /* see if we already have the message in our cache in
   * case $message_cache_dir is unset */
  struct PopCache *cache = &adata->cache[e->index % POP_CACHE_LEN];

  if (cache->path)
  {
    if (cache->index == e->index)
    {
      /* yes, so just return a pointer to the message */
      msg->fp = fopen(cache->path, "r");
      if (msg->fp)
        return true;

      mutt_perror("%s", cache->path);
      return false;
    }
    else
    {
      /* clear the previous entry */
      unlink(cache->path);
      FREE(&cache->path);
    }
  }

  path = buf_pool_get();

  while (true)
  {
    if (pop_reconnect(m) < 0)
      goto cleanup;

    /* verify that massage index is correct */
    if (edata->refno < 0)
    {
      mutt_error(_("The message index is incorrect. Try reopening the mailbox."));
      goto cleanup;
    }

    /* see if we can put in body cache; use our cache as fallback */
    msg->fp = mutt_bcache_put(adata->bcache, cache_id(edata->uid));
    if (!msg->fp)
    {
      /* no */
      bcache = false;
      buf_mktemp(path);
      msg->fp = mutt_file_fopen(buf_string(path), "w+");
      if (!msg->fp)
      {
        mutt_perror("%s", buf_string(path));
        goto cleanup;
      }
    }

    snprintf(buf, sizeof(buf), "RETR %d\r\n", edata->refno);

    struct Progress *progress = progress_new(_("Fetching message..."), MUTT_PROGRESS_NET,
                                             e->body->length + e->body->offset - 1);
    const int rc = pop_fetch_data(adata, buf, progress, fetch_message, msg->fp);
    progress_free(&progress);

    if (rc == 0)
      break;

    mutt_file_fclose(&msg->fp);

    /* if RETR failed (e.g. connection closed), be sure to remove either
     * the file in bcache or from POP's own cache since the next iteration
     * of the loop will re-attempt to put() the message */
    if (!bcache)
      unlink(buf_string(path));

    if (rc == -2)
    {
      mutt_error("%s", adata->err_msg);
      goto cleanup;
    }

    if (rc == -3)
    {
      mutt_error(_("Can't write message to temporary file"));
      goto cleanup;
    }
  }

  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.  */
  if (bcache)
  {
    mutt_bcache_commit(adata->bcache, cache_id(edata->uid));
  }
  else
  {
    cache->index = e->index;
    cache->path = buf_strdup(path);
  }
  rewind(msg->fp);

  /* Detach the private data */
  e->edata = NULL;

  /* we replace envelope, key in subj_hash has to be updated as well */
  if (m->subj_hash && e->env->real_subj)
    mutt_hash_delete(m->subj_hash, e->env->real_subj, e);
  mutt_label_hash_remove(m, e);
  mutt_env_free(&e->env);
  e->env = mutt_rfc822_read_header(msg->fp, e, false, false);
  if (m->subj_hash && e->env->real_subj)
    mutt_hash_insert(m->subj_hash, e->env->real_subj, e);
  mutt_label_hash_add(m, e);

  /* Reattach the private data */
  e->edata = edata;
  e->edata_free = pop_edata_free;

  e->lines = 0;
  while (fgets(buf, sizeof(buf), msg->fp) && !feof(msg->fp))
  {
    e->lines++;
  }

  e->body->length = ftello(msg->fp) - e->body->offset;

  /* This needs to be done in case this is a multipart message */
  if (!WithCrypto)
    e->security = crypt_query(e->body);

  mutt_clear_error();
  rewind(msg->fp);

  success = true;

cleanup:
  buf_pool_release(&path);
  return success;
}

/**
 * pop_msg_close - Close an email - Implements MxOps::msg_close() - @ingroup mx_msg_close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
static int pop_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * pop_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache() - @ingroup mx_msg_save_hcache
 */
static int pop_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  struct PopAccountData *adata = pop_adata_get(m);
  struct PopEmailData *edata = e->edata;
  struct HeaderCache *hc = pop_hcache_open(adata, mailbox_path(m));
  rc = hcache_store(hc, edata->uid, strlen(edata->uid), e, 0);
  hcache_close(&hc);
#endif

  return rc;
}

/**
 * pop_path_probe - Is this a POP Mailbox? - Implements MxOps::path_probe() - @ingroup mx_path_probe
 */
enum MailboxType pop_path_probe(const char *path, const struct stat *st)
{
  if (mutt_istr_startswith(path, "pop://"))
    return MUTT_POP;

  if (mutt_istr_startswith(path, "pops://"))
    return MUTT_POP;

  return MUTT_UNKNOWN;
}

/**
 * pop_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon() - @ingroup mx_path_canon
 */
static int pop_path_canon(struct Buffer *path)
{
  return 0;
}

/**
 * pop_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent() - @ingroup mx_path_parent
 */
static int pop_path_parent(struct Buffer *path)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

/**
 * MxPopOps - POP Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxPopOps = {
  // clang-format off
  .type            = MUTT_POP,
  .name             = "pop",
  .is_local         = false,
  .ac_owns_path     = pop_ac_owns_path,
  .ac_add           = pop_ac_add,
  .mbox_open        = pop_mbox_open,
  .mbox_open_append = NULL,
  .mbox_check       = pop_mbox_check,
  .mbox_check_stats = NULL,
  .mbox_sync        = pop_mbox_sync,
  .mbox_close       = pop_mbox_close,
  .msg_open         = pop_msg_open,
  .msg_open_new     = NULL,
  .msg_commit       = NULL,
  .msg_close        = pop_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = pop_msg_save_hcache,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = pop_path_probe,
  .path_canon       = pop_path_canon,
  .path_parent      = pop_path_parent,
  .path_is_empty    = NULL,
  // clang-format on
};
