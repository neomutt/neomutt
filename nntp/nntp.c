/**
 * @file
 * Usenet network mailbox type; talk to an NNTP server
 *
 * @authors
 * Copyright (C) 2016-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Ian Zimmerman <itz@no-use.mooo.com>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page nntp_nntp Talk to an NNTP server
 *
 * Usenet network mailbox type; talk to an NNTP server
 *
 * Implementation: #MxNntpOps
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "lib.h"
#include "attach/lib.h"
#include "bcache/lib.h"
#include "hcache/lib.h"
#include "ncrypt/lib.h"
#include "progress/lib.h"
#include "question/lib.h"
#include "adata.h"
#include "edata.h"
#include "hook.h"
#include "mdata.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mx.h"
#ifdef USE_HCACHE
#include "protos.h"
#endif
#ifdef USE_SASL_CYRUS
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif
#if defined(USE_SSL) || defined(USE_HCACHE)
#include "mutt.h"
#endif

struct stat;

/// Current news server
struct NntpAccountData *CurrentNewsSrv = NULL;

/// Fields to get from server, if it supports the LIST OVERVIEW.FMT feature
static const char *OverviewFmt = "Subject:\0"
                                 "From:\0"
                                 "Date:\0"
                                 "Message-ID:\0"
                                 "References:\0"
                                 "Content-Length:\0"
                                 "Lines:\0"
                                 "\0";

/**
 * struct FetchCtx - Keep track when getting data from a server
 */
struct FetchCtx
{
  struct Mailbox *mailbox;
  anum_t first;
  anum_t last;
  bool restore;
  unsigned char *messages;
  struct Progress *progress;
  struct HeaderCache *hc;
};

/**
 * struct ChildCtx - Keep track of the children of an article
 */
struct ChildCtx
{
  struct Mailbox *mailbox;
  unsigned int num;
  unsigned int max;
  anum_t *child;
};

/**
 * nntp_hashelem_free - Free our hash table data - Implements ::hash_hdata_free_t - @ingroup hash_hdata_free_api
 */
void nntp_hashelem_free(int type, void *obj, intptr_t data)
{
  nntp_mdata_free(&obj);
}

/**
 * nntp_connect_error - Signal a failed connection
 * @param adata NNTP server
 * @retval -1 Always
 */
static int nntp_connect_error(struct NntpAccountData *adata)
{
  adata->status = NNTP_NONE;
  mutt_error(_("Server closed connection"));
  return -1;
}

/**
 * nntp_capabilities - Get capabilities
 * @param adata NNTP server
 * @retval -1 Error, connection is closed
 * @retval  0 Mode is reader, capabilities set up
 * @retval  1 Need to switch to reader mode
 */
static int nntp_capabilities(struct NntpAccountData *adata)
{
  struct Connection *conn = adata->conn;
  bool mode_reader = false;
  char authinfo[1024] = { 0 };

  adata->hasCAPABILITIES = false;
  adata->hasSTARTTLS = false;
  adata->hasDATE = false;
  adata->hasLIST_NEWSGROUPS = false;
  adata->hasLISTGROUP = false;
  adata->hasLISTGROUPrange = false;
  adata->hasOVER = false;
  FREE(&adata->authenticators);

  struct Buffer *buf = buf_pool_get();

  if ((mutt_socket_send(conn, "CAPABILITIES\r\n") < 0) ||
      (mutt_socket_buffer_readln(buf, conn) < 0))
  {
    buf_pool_release(&buf);
    return nntp_connect_error(adata);
  }

  /* no capabilities */
  if (!mutt_str_startswith(buf_string(buf), "101"))
  {
    buf_pool_release(&buf);
    return 1;
  }
  adata->hasCAPABILITIES = true;

  /* parse capabilities */
  do
  {
    size_t plen = 0;
    if (mutt_socket_buffer_readln(buf, conn) < 0)
    {
      buf_pool_release(&buf);
      return nntp_connect_error(adata);
    }
    if (mutt_str_equal("STARTTLS", buf_string(buf)))
    {
      adata->hasSTARTTLS = true;
    }
    else if (mutt_str_equal("MODE-READER", buf_string(buf)))
    {
      mode_reader = true;
    }
    else if (mutt_str_equal("READER", buf_string(buf)))
    {
      adata->hasDATE = true;
      adata->hasLISTGROUP = true;
      adata->hasLISTGROUPrange = true;
    }
    else if ((plen = mutt_str_startswith(buf_string(buf), "AUTHINFO ")))
    {
      buf_addch(buf, ' ');
      mutt_str_copy(authinfo, buf->data + plen - 1, sizeof(authinfo));
    }
#ifdef USE_SASL_CYRUS
    else if ((plen = mutt_str_startswith(buf_string(buf), "SASL ")))
    {
      char *p = buf->data + plen;
      while (*p == ' ')
        p++;
      adata->authenticators = mutt_str_dup(p);
    }
#endif
    else if (mutt_str_equal("OVER", buf_string(buf)))
    {
      adata->hasOVER = true;
    }
    else if (mutt_str_startswith(buf_string(buf), "LIST "))
    {
      const char *p = buf_find_string(buf, " NEWSGROUPS");
      if (p)
      {
        p += 11;
        if ((*p == '\0') || (*p == ' '))
          adata->hasLIST_NEWSGROUPS = true;
      }
    }
  } while (!mutt_str_equal(".", buf_string(buf)));
  buf_reset(buf);

#ifdef USE_SASL_CYRUS
  if (adata->authenticators && mutt_istr_find(authinfo, " SASL "))
    buf_strcpy(buf, adata->authenticators);
#endif
  if (mutt_istr_find(authinfo, " USER "))
  {
    if (!buf_is_empty(buf))
      buf_addch(buf, ' ');
    buf_addstr(buf, "USER");
  }
  mutt_str_replace(&adata->authenticators, buf_string(buf));
  buf_pool_release(&buf);

  /* current mode is reader */
  if (adata->hasDATE)
    return 0;

  /* server is mode-switching, need to switch to reader mode */
  if (mode_reader)
    return 1;

  mutt_socket_close(conn);
  adata->status = NNTP_BYE;
  mutt_error(_("Server doesn't support reader mode"));
  return -1;
}

/**
 * nntp_attempt_features - Detect supported commands
 * @param adata NNTP server
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_attempt_features(struct NntpAccountData *adata)
{
  struct Connection *conn = adata->conn;
  char buf[1024] = { 0 };
  int rc = -1;

  /* no CAPABILITIES, trying DATE, LISTGROUP, LIST NEWSGROUPS */
  if (!adata->hasCAPABILITIES)
  {
    if ((mutt_socket_send(conn, "DATE\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      goto fail;
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasDATE = true;

    if ((mutt_socket_send(conn, "LISTGROUP\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      goto fail;
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasLISTGROUP = true;

    if ((mutt_socket_send(conn, "LIST NEWSGROUPS +\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      goto fail;
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasLIST_NEWSGROUPS = true;
    if (mutt_str_startswith(buf, "215"))
    {
      do
      {
        if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
          goto fail;
      } while (!mutt_str_equal(".", buf));
    }
  }

  /* no LIST NEWSGROUPS, trying XGTITLE */
  if (!adata->hasLIST_NEWSGROUPS)
  {
    if ((mutt_socket_send(conn, "XGTITLE\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      goto fail;
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasXGTITLE = true;
  }

  /* no OVER, trying XOVER */
  if (!adata->hasOVER)
  {
    if ((mutt_socket_send(conn, "XOVER\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      goto fail;
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasXOVER = true;
  }

  /* trying LIST OVERVIEW.FMT */
  if (adata->hasOVER || adata->hasXOVER)
  {
    if ((mutt_socket_send(conn, "LIST OVERVIEW.FMT\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      goto fail;
    }
    if (!mutt_str_startswith(buf, "215"))
    {
      adata->overview_fmt = mutt_str_dup(OverviewFmt);
    }
    else
    {
      bool cont = false;
      size_t buflen = 2048, off = 0, b = 0;

      FREE(&adata->overview_fmt);
      adata->overview_fmt = MUTT_MEM_MALLOC(buflen, char);

      while (true)
      {
        if ((buflen - off) < 1024)
        {
          buflen *= 2;
          MUTT_MEM_REALLOC(&adata->overview_fmt, buflen, char);
        }

        const int chunk = mutt_socket_readln_d(adata->overview_fmt + off,
                                               buflen - off, conn, MUTT_SOCK_LOG_HDR);
        if (chunk < 0)
        {
          FREE(&adata->overview_fmt);
          goto fail;
        }

        if (!cont && mutt_str_equal(".", adata->overview_fmt + off))
          break;

        cont = (chunk >= (buflen - off));
        off += strlen(adata->overview_fmt + off);
        if (!cont)
        {
          if (adata->overview_fmt[b] == ':')
          {
            memmove(adata->overview_fmt + b, adata->overview_fmt + b + 1, off - b - 1);
            adata->overview_fmt[off - 1] = ':';
          }
          char *colon = strchr(adata->overview_fmt + b, ':');
          if (!colon)
            adata->overview_fmt[off++] = ':';
          else if (!mutt_str_equal(colon + 1, "full"))
            off = colon + 1 - adata->overview_fmt;
          if (strcasecmp(adata->overview_fmt + b, "Bytes:") == 0)
          {
            size_t len = strlen(adata->overview_fmt + b);
            mutt_str_copy(adata->overview_fmt + b, "Content-Length:", len + 1);
            off = b + len;
          }
          adata->overview_fmt[off++] = '\0';
          b = off;
        }
      }
      adata->overview_fmt[off++] = '\0';
      MUTT_MEM_REALLOC(&adata->overview_fmt, off, char);
    }
  }
  rc = 0; // Success

fail:
  if (rc < 0)
    nntp_connect_error(adata);

  return rc;
}

#ifdef USE_SASL_CYRUS
/**
 * nntp_memchr - Look for a char in a binary buf, conveniently
 * @param haystack [in/out] input: start here, output: store address of hit
 * @param sentinel points just beyond (1 byte after) search area
 * @param needle the character to search for
 * @retval true found and updated haystack
 * @retval false not found
 */
static bool nntp_memchr(char **haystack, const char *sentinel, int needle)
{
  char *start = *haystack;
  size_t max_offset = sentinel - start;
  void *vp = memchr(start, max_offset, needle);
  if (!vp)
    return false;
  *haystack = vp;
  return true;
}

/**
 * nntp_log_binbuf - Log a buffer possibly containing NUL bytes
 * @param buf source buffer
 * @param len how many bytes from buf
 * @param pfx logging prefix (protocol etc.)
 * @param dbg which loglevel does message belong
 */
static void nntp_log_binbuf(const char *buf, size_t len, const char *pfx, int dbg)
{
  char tmp[1024] = { 0 };
  char *p = tmp;
  char *sentinel = tmp + len;

  const short c_debug_level = cs_subset_number(NeoMutt->sub, "debug_level");
  if (c_debug_level < dbg)
    return;
  memcpy(tmp, buf, len);
  tmp[len] = '\0';
  while (nntp_memchr(&p, sentinel, '\0'))
    *p = '.';
  mutt_debug(dbg, "%s> %s\n", pfx, tmp);
}
#endif

/**
 * nntp_auth - Get login, password and authenticate
 * @param adata NNTP server
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_auth(struct NntpAccountData *adata)
{
  struct Connection *conn = adata->conn;
  char authenticators[1024] = "USER";
  char *method = NULL, *a = NULL, *p = NULL;
  unsigned char flags = conn->account.flags;
  struct Buffer *buf = buf_pool_get();

  const char *const c_nntp_authenticators = cs_subset_string(NeoMutt->sub, "nntp_authenticators");
  while (true)
  {
    /* get login and password */
    if ((mutt_account_getuser(&conn->account) < 0) || (conn->account.user[0] == '\0') ||
        (mutt_account_getpass(&conn->account) < 0) || (conn->account.pass[0] == '\0'))
    {
      break;
    }

    /* get list of authenticators */
    if (c_nntp_authenticators)
    {
      mutt_str_copy(authenticators, c_nntp_authenticators, sizeof(authenticators));
    }
    else if (adata->hasCAPABILITIES)
    {
      mutt_str_copy(authenticators, adata->authenticators, sizeof(authenticators));
      p = authenticators;
      while (*p)
      {
        if (*p == ' ')
          *p = ':';
        p++;
      }
    }
    p = authenticators;
    while (*p)
    {
      *p = toupper(*p);
      p++;
    }

    mutt_debug(LL_DEBUG1, "available methods: %s\n", adata->authenticators);
    a = authenticators;
    while (true)
    {
      if (!a)
      {
        mutt_error(_("No authenticators available"));
        break;
      }

      method = a;
      a = strchr(a, ':');
      if (a)
        *a++ = '\0';

      /* check authenticator */
      if (adata->hasCAPABILITIES)
      {
        if (!adata->authenticators)
          continue;
        const char *m = mutt_istr_find(adata->authenticators, method);
        if (!m)
          continue;
        if ((m > adata->authenticators) && (*(m - 1) != ' '))
          continue;
        m += strlen(method);
        if ((*m != '\0') && (*m != ' '))
          continue;
      }
      mutt_debug(LL_DEBUG1, "trying method %s\n", method);

      /* AUTHINFO USER authentication */
      if (mutt_str_equal(method, "USER"))
      {
        // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
        mutt_message(_("Authenticating (%s)..."), method);
        buf_printf(buf, "AUTHINFO USER %s\r\n", conn->account.user);
        if ((mutt_socket_send(conn, buf_string(buf)) < 0) ||
            (mutt_socket_buffer_readln_d(buf, conn, MUTT_SOCK_LOG_FULL) < 0))
        {
          break;
        }

        /* authenticated, password is not required */
        if (mutt_str_startswith(buf_string(buf), "281"))
        {
          buf_pool_release(&buf);
          return 0;
        }

        /* username accepted, sending password */
        if (mutt_str_startswith(buf_string(buf), "381"))
        {
          mutt_debug(MUTT_SOCK_LOG_FULL, "%d> AUTHINFO PASS *\n", conn->fd);
          buf_printf(buf, "AUTHINFO PASS %s\r\n", conn->account.pass);
          if ((mutt_socket_send_d(conn, buf_string(buf), MUTT_SOCK_LOG_FULL) < 0) ||
              (mutt_socket_buffer_readln_d(buf, conn, MUTT_SOCK_LOG_FULL) < 0))
          {
            break;
          }

          /* authenticated */
          if (mutt_str_startswith(buf_string(buf), "281"))
          {
            buf_pool_release(&buf);
            return 0;
          }
        }

        /* server doesn't support AUTHINFO USER, trying next method */
        if (buf_at(buf, 0) == '5')
          continue;
      }
      else
      {
#ifdef USE_SASL_CYRUS
        sasl_conn_t *saslconn = NULL;
        sasl_interact_t *interaction = NULL;
        int rc;
        char inbuf[1024] = { 0 };
        const char *mech = NULL;
        const char *client_out = NULL;
        unsigned int client_len, len;

        if (mutt_sasl_client_new(conn, &saslconn) < 0)
        {
          mutt_debug(LL_DEBUG1, "error allocating SASL connection\n");
          continue;
        }

        while (true)
        {
          rc = sasl_client_start(saslconn, method, &interaction, &client_out,
                                 &client_len, &mech);
          if (rc != SASL_INTERACT)
            break;
          mutt_sasl_interact(interaction);
        }
        if ((rc != SASL_OK) && (rc != SASL_CONTINUE))
        {
          sasl_dispose(&saslconn);
          mutt_debug(LL_DEBUG1, "error starting SASL authentication exchange\n");
          continue;
        }

        // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
        mutt_message(_("Authenticating (%s)..."), method);
        buf_printf(buf, "AUTHINFO SASL %s", method);

        /* looping protocol */
        while ((rc == SASL_CONTINUE) || ((rc == SASL_OK) && client_len))
        {
          /* send out client response */
          if (client_len)
          {
            nntp_log_binbuf(client_out, client_len, "SASL", MUTT_SOCK_LOG_FULL);
            if (!buf_is_empty(buf))
              buf_addch(buf, ' ');
            len = buf_len(buf);
            if (sasl_encode64(client_out, client_len, buf->data + len,
                              buf->dsize - len, &len) != SASL_OK)
            {
              mutt_debug(LL_DEBUG1, "error base64-encoding client response\n");
              break;
            }
          }

          buf_addstr(buf, "\r\n");
          if (buf_find_char(buf, ' '))
          {
            mutt_debug(MUTT_SOCK_LOG_CMD, "%d> AUTHINFO SASL %s%s\n", conn->fd,
                       method, client_len ? " sasl_data" : "");
          }
          else
          {
            mutt_debug(MUTT_SOCK_LOG_CMD, "%d> sasl_data\n", conn->fd);
          }
          client_len = 0;
          if ((mutt_socket_send_d(conn, buf_string(buf), MUTT_SOCK_LOG_FULL) < 0) ||
              (mutt_socket_readln_d(inbuf, sizeof(inbuf), conn, MUTT_SOCK_LOG_FULL) < 0))
          {
            break;
          }
          if (!mutt_str_startswith(inbuf, "283 ") && !mutt_str_startswith(inbuf, "383 "))
          {
            mutt_debug(MUTT_SOCK_LOG_FULL, "%d< %s\n", conn->fd, inbuf);
            break;
          }
          inbuf[3] = '\0';
          mutt_debug(MUTT_SOCK_LOG_FULL, "%d< %s sasl_data\n", conn->fd, inbuf);

          if (mutt_str_equal("=", inbuf + 4))
            len = 0;
          else if (sasl_decode64(inbuf + 4, strlen(inbuf + 4), buf->data,
                                 buf->dsize - 1, &len) != SASL_OK)
          {
            mutt_debug(LL_DEBUG1, "error base64-decoding server response\n");
            break;
          }
          else
          {
            nntp_log_binbuf(buf_string(buf), len, "SASL", MUTT_SOCK_LOG_FULL);
          }

          while (true)
          {
            rc = sasl_client_step(saslconn, buf_string(buf), len, &interaction,
                                  &client_out, &client_len);
            if (rc != SASL_INTERACT)
              break;
            mutt_sasl_interact(interaction);
          }
          if (*inbuf != '3')
            break;

          buf_reset(buf);
        } /* looping protocol */

        if ((rc == SASL_OK) && (client_len == 0) && (*inbuf == '2'))
        {
          mutt_sasl_setup_conn(conn, saslconn);
          buf_pool_release(&buf);
          return 0;
        }

        /* terminate SASL session */
        sasl_dispose(&saslconn);
        if (conn->fd < 0)
          break;
        if (mutt_str_startswith(inbuf, "383 "))
        {
          if ((mutt_socket_send(conn, "*\r\n") < 0) ||
              (mutt_socket_readln(inbuf, sizeof(inbuf), conn) < 0))
          {
            break;
          }
        }

        /* server doesn't support AUTHINFO SASL, trying next method */
        if (*inbuf == '5')
          continue;
#else
        continue;
#endif /* USE_SASL_CYRUS */
      }

      // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
      mutt_error(_("%s authentication failed"), method);
      break;
    }
    break;
  }

  /* error */
  adata->status = NNTP_BYE;
  conn->account.flags = flags;
  if (conn->fd < 0)
  {
    mutt_error(_("Server closed connection"));
  }
  else
  {
    mutt_socket_close(conn);
  }

  buf_pool_release(&buf);
  return -1;
}

/**
 * nntp_query - Send data from buffer and receive answer to same buffer
 * @param mdata NNTP Mailbox data
 * @param line      Buffer containing data
 * @param linelen   Length of buffer
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_query(struct NntpMboxData *mdata, char *line, size_t linelen)
{
  struct NntpAccountData *adata = mdata->adata;
  if (adata->status == NNTP_BYE)
    return -1;

  char buf[1024] = { 0 };
  int rc = -1;

  while (true)
  {
    if (adata->status == NNTP_OK)
    {
      int rc_send = 0;

      if (*line)
      {
        rc_send = mutt_socket_send(adata->conn, line);
      }
      else if (mdata->group)
      {
        snprintf(buf, sizeof(buf), "GROUP %s\r\n", mdata->group);
        rc_send = mutt_socket_send(adata->conn, buf);
      }
      if (rc_send >= 0)
        rc_send = mutt_socket_readln(buf, sizeof(buf), adata->conn);
      if (rc_send >= 0)
        break;
    }

    /* reconnect */
    while (true)
    {
      adata->status = NNTP_NONE;
      if (nntp_open_connection(adata) == 0)
        break;

      snprintf(buf, sizeof(buf), _("Connection to %s lost. Reconnect?"),
               adata->conn->account.host);
      if (query_yesorno(buf, MUTT_YES) != MUTT_YES)
      {
        adata->status = NNTP_BYE;
        goto done;
      }
    }

    /* select newsgroup after reconnection */
    if (mdata->group)
    {
      snprintf(buf, sizeof(buf), "GROUP %s\r\n", mdata->group);
      if ((mutt_socket_send(adata->conn, buf) < 0) ||
          (mutt_socket_readln(buf, sizeof(buf), adata->conn) < 0))
      {
        nntp_connect_error(adata);
        goto done;
      }
    }
    if (*line == '\0')
      break;
  }

  mutt_str_copy(line, buf, linelen);
  rc = 0;

done:
  return rc;
}

/**
 * nntp_fetch_lines - Read lines, calling a callback function for each
 * @param mdata NNTP Mailbox data
 * @param query     Query to match
 * @param qlen      Length of query
 * @param msg       Progress message (OPTIONAL)
 * @param func      Callback function
 * @param data      Data for callback function
 * @retval  0 Success
 * @retval  1 Bad response (answer in query buffer)
 * @retval -1 Connection lost
 * @retval -2 Error in func(*line, *data)
 *
 * This function calls func(*line, *data) for each received line,
 * func(NULL, *data) if rewind(*data) needs, exits when fail or done:
 */
static int nntp_fetch_lines(struct NntpMboxData *mdata, char *query, size_t qlen,
                            const char *msg, int (*func)(char *, void *), void *data)
{
  bool done = false;
  int rc;

  while (!done)
  {
    char buf[1024] = { 0 };
    char *line = NULL;
    unsigned int lines = 0;
    size_t off = 0;
    struct Progress *progress = NULL;

    mutt_str_copy(buf, query, sizeof(buf));
    if (nntp_query(mdata, buf, sizeof(buf)) < 0)
      return -1;
    if (buf[0] != '2')
    {
      mutt_str_copy(query, buf, qlen);
      return 1;
    }

    line = MUTT_MEM_MALLOC(sizeof(buf), char);
    rc = 0;

    if (msg)
    {
      progress = progress_new(MUTT_PROGRESS_READ, 0);
      progress_set_message(progress, "%s", msg);
    }

    while (true)
    {
      char *p = NULL;
      int chunk = mutt_socket_readln_d(buf, sizeof(buf), mdata->adata->conn, MUTT_SOCK_LOG_FULL);
      if (chunk < 0)
      {
        mdata->adata->status = NNTP_NONE;
        break;
      }

      p = buf;
      if (!off && (buf[0] == '.'))
      {
        if (buf[1] == '\0')
        {
          done = true;
          break;
        }
        if (buf[1] == '.')
          p++;
      }

      mutt_str_copy(line + off, p, sizeof(buf));

      if (chunk >= sizeof(buf))
      {
        off += strlen(p);
      }
      else
      {
        progress_update(progress, ++lines, -1);

        if ((rc == 0) && (func(line, data) < 0))
          rc = -2;
        off = 0;
      }

      MUTT_MEM_REALLOC(&line, off + sizeof(buf), char);
    }
    FREE(&line);
    func(NULL, data);
    progress_free(&progress);
  }

  return rc;
}

/**
 * fetch_description - Parse newsgroup description
 * @param line String to parse
 * @param data NNTP Server
 * @retval 0 Always
 */
static int fetch_description(char *line, void *data)
{
  if (!line)
    return 0;

  struct NntpAccountData *adata = data;

  char *desc = strpbrk(line, " \t");
  if (desc)
  {
    *desc++ = '\0';
    desc += strspn(desc, " \t");
  }
  else
  {
    desc = strchr(line, '\0');
  }

  struct NntpMboxData *mdata = mutt_hash_find(adata->groups_hash, line);
  if (mdata && !mutt_str_equal(desc, mdata->desc))
  {
    mutt_str_replace(&mdata->desc, desc);
    mutt_debug(LL_DEBUG2, "group: %s, desc: %s\n", line, desc);
  }
  return 0;
}

/**
 * get_description - Fetch newsgroups descriptions
 * @param mdata NNTP Mailbox data
 * @param wildmat   String to match
 * @param msg       Progress message
 * @retval  0 Success
 * @retval  1 Bad response (answer in query buffer)
 * @retval -1 Connection lost
 * @retval -2 Error
 */
static int get_description(struct NntpMboxData *mdata, const char *wildmat, const char *msg)
{
  char buf[256] = { 0 };
  const char *cmd = NULL;

  /* get newsgroup description, if possible */
  struct NntpAccountData *adata = mdata->adata;
  if (!wildmat)
    wildmat = mdata->group;
  if (adata->hasLIST_NEWSGROUPS)
    cmd = "LIST NEWSGROUPS";
  else if (adata->hasXGTITLE)
    cmd = "XGTITLE";
  else
    return 0;

  snprintf(buf, sizeof(buf), "%s %s\r\n", cmd, wildmat);
  int rc = nntp_fetch_lines(mdata, buf, sizeof(buf), msg, fetch_description, adata);
  if (rc > 0)
  {
    mutt_error("%s: %s", cmd, buf);
  }
  return rc;
}

/**
 * nntp_parse_xref - Parse cross-reference
 * @param m Mailbox
 * @param e Email
 *
 * Update read flag and set article number if empty
 */
static void nntp_parse_xref(struct Mailbox *m, struct Email *e)
{
  struct NntpMboxData *mdata = m->mdata;

  char *buf = mutt_str_dup(e->env->xref);
  char *p = buf;
  while (p)
  {
    anum_t anum = 0;

    /* skip to next word */
    p += strspn(p, " \t");
    char *grp = p;

    /* skip to end of word */
    p = strpbrk(p, " \t");
    if (p)
      *p++ = '\0';

    /* find colon */
    char *colon = strchr(grp, ':');
    if (!colon)
      continue;
    *colon++ = '\0';
    if (sscanf(colon, ANUM_FMT, &anum) != 1)
      continue;

    nntp_article_status(m, e, grp, anum);
    if (!nntp_edata_get(e)->article_num && mutt_str_equal(mdata->group, grp))
      nntp_edata_get(e)->article_num = anum;
  }
  FREE(&buf);
}

/**
 * fetch_tempfile - Write line to temporary file
 * @param line Text to write
 * @param data FILE pointer
 * @retval  0 Success
 * @retval -1 Failure
 */
static int fetch_tempfile(char *line, void *data)
{
  FILE *fp = data;

  if (!line)
    rewind(fp);
  else if ((fputs(line, fp) == EOF) || (fputc('\n', fp) == EOF))
    return -1;
  return 0;
}

/**
 * fetch_numbers - Parse article number
 * @param line Article number
 * @param data FetchCtx
 * @retval 0 Always
 */
static int fetch_numbers(char *line, void *data)
{
  struct FetchCtx *fc = data;
  anum_t anum = 0;

  if (!line)
    return 0;
  if (sscanf(line, ANUM_FMT, &anum) != 1)
    return 0;
  if ((anum < fc->first) || (anum > fc->last))
    return 0;
  fc->messages[anum - fc->first] = 1;
  return 0;
}

/**
 * parse_overview_line - Parse overview line
 * @param line String to parse
 * @param data FetchCtx
 * @retval  0 Success
 * @retval -1 Failure
 */
static int parse_overview_line(char *line, void *data)
{
  if (!line || !data)
    return 0;

  struct FetchCtx *fc = data;
  struct Mailbox *m = fc->mailbox;
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  struct Email *e = NULL;
  char *header = NULL, *field = NULL;
  bool save = true;
  anum_t anum = 0;

  /* parse article number */
  field = strchr(line, '\t');
  if (field)
    *field++ = '\0';
  if (sscanf(line, ANUM_FMT, &anum) != 1)
    return 0;
  mutt_debug(LL_DEBUG2, "" ANUM_FMT "\n", anum);

  /* out of bounds */
  if ((anum < fc->first) || (anum > fc->last))
    return 0;

  /* not in LISTGROUP */
  if (!fc->messages[anum - fc->first])
  {
    progress_update(fc->progress, anum - fc->first + 1, -1);
    return 0;
  }

  /* convert overview line to header */
  FILE *fp = mutt_file_mkstemp();
  if (!fp)
    return -1;

  header = mdata->adata->overview_fmt;
  while (field)
  {
    char *b = field;

    if (*header)
    {
      if (!strstr(header, ":full") && (fputs(header, fp) == EOF))
      {
        mutt_file_fclose(&fp);
        return -1;
      }
      header = strchr(header, '\0') + 1;
    }

    field = strchr(field, '\t');
    if (field)
      *field++ = '\0';
    if ((fputs(b, fp) == EOF) || (fputc('\n', fp) == EOF))
    {
      mutt_file_fclose(&fp);
      return -1;
    }
  }
  rewind(fp);

  /* allocate memory for headers */
  mx_alloc_memory(m, m->msg_count);

  /* parse header */
  m->emails[m->msg_count] = email_new();
  e = m->emails[m->msg_count];
  e->env = mutt_rfc822_read_header(fp, e, false, false);
  e->env->newsgroups = mutt_str_dup(mdata->group);
  e->received = e->date_sent;
  mutt_file_fclose(&fp);

#ifdef USE_HCACHE
  if (fc->hc)
  {
    char buf[16] = { 0 };

    /* try to replace with header from cache */
    snprintf(buf, sizeof(buf), ANUM_FMT, anum);
    struct HCacheEntry hce = hcache_fetch_email(fc->hc, buf, strlen(buf), 0);
    if (hce.email)
    {
      mutt_debug(LL_DEBUG2, "hcache_fetch_email %s\n", buf);
      email_free(&e);
      e = hce.email;
      m->emails[m->msg_count] = e;
      e->edata = NULL;
      e->read = false;
      e->old = false;

      /* skip header marked as deleted in cache */
      if (e->deleted && !fc->restore)
      {
        if (mdata->bcache)
        {
          mutt_debug(LL_DEBUG2, "mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }
        save = false;
      }
    }
    else
    {
      /* not cached yet, store header */
      mutt_debug(LL_DEBUG2, "hcache_store_email %s\n", buf);
      hcache_store_email(fc->hc, buf, strlen(buf), e, 0);
    }
  }
#endif

  if (save)
  {
    e->index = m->msg_count++;
    e->read = false;
    e->old = false;
    e->deleted = false;
    e->edata = nntp_edata_new();
    e->edata_free = nntp_edata_free;
    nntp_edata_get(e)->article_num = anum;
    if (fc->restore)
    {
      e->changed = true;
    }
    else
    {
      nntp_article_status(m, e, NULL, anum);
      if (!e->read)
        nntp_parse_xref(m, e);
    }
    if (anum > mdata->last_loaded)
      mdata->last_loaded = anum;
  }
  else
  {
    email_free(&e);
  }

  progress_update(fc->progress, anum - fc->first + 1, -1);
  return 0;
}

/**
 * nntp_fetch_headers - Fetch headers
 * @param m       Mailbox
 * @param hc      Header cache
 * @param first   Number of first header to fetch
 * @param last    Number of last header to fetch
 * @param restore Restore message listed as deleted
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_fetch_headers(struct Mailbox *m, void *hc, anum_t first, anum_t last, bool restore)
{
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  struct FetchCtx fc = { 0 };
  struct Email *e = NULL;
  char buf[8192] = { 0 };
  int rc = 0;
  anum_t current;
  anum_t first_over = first;

  /* if empty group or nothing to do */
  if (!last || (first > last))
    return 0;

  /* init fetch context */
  fc.mailbox = m;
  fc.first = first;
  fc.last = last;
  fc.restore = restore;
  fc.messages = MUTT_MEM_CALLOC(last - first + 1, unsigned char);
  if (!fc.messages)
    return -1;
  fc.hc = hc;

  /* fetch list of articles */
  const bool c_nntp_listgroup = cs_subset_bool(NeoMutt->sub, "nntp_listgroup");
  if (c_nntp_listgroup && mdata->adata->hasLISTGROUP && !mdata->deleted)
  {
    if (m->verbose)
      mutt_message(_("Fetching list of articles..."));
    if (mdata->adata->hasLISTGROUPrange)
    {
      snprintf(buf, sizeof(buf), "LISTGROUP %s " ANUM_FMT "-" ANUM_FMT "\r\n",
               mdata->group, first, last);
    }
    else
    {
      snprintf(buf, sizeof(buf), "LISTGROUP %s\r\n", mdata->group);
    }
    rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_numbers, &fc);
    if (rc > 0)
    {
      mutt_error("LISTGROUP: %s", buf);
    }
    if (rc == 0)
    {
      for (current = first; (current <= last); current++)
      {
        if (fc.messages[current - first])
          continue;

        snprintf(buf, sizeof(buf), ANUM_FMT, current);
        if (mdata->bcache)
        {
          mutt_debug(LL_DEBUG2, "#1 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }

#ifdef USE_HCACHE
        if (fc.hc)
        {
          mutt_debug(LL_DEBUG2, "hcache_delete_email %s\n", buf);
          hcache_delete_email(fc.hc, buf, strlen(buf));
        }
#endif
      }
    }
  }
  else
  {
    for (current = first; current <= last; current++)
      fc.messages[current - first] = 1;
  }

  /* fetching header from cache or server, or fallback to fetch overview */
  if (m->verbose)
  {
    fc.progress = progress_new(MUTT_PROGRESS_READ, last - first + 1);
    progress_set_message(fc.progress, _("Fetching message headers..."));
  }
  for (current = first; (current <= last) && (rc == 0); current++)
  {
    progress_update(fc.progress, current - first + 1, -1);

#ifdef USE_HCACHE
    snprintf(buf, sizeof(buf), ANUM_FMT, current);
#endif

    /* delete header from cache that does not exist on server */
    if (!fc.messages[current - first])
      continue;

    /* allocate memory for headers */
    mx_alloc_memory(m, m->msg_count);

#ifdef USE_HCACHE
    /* try to fetch header from cache */
    struct HCacheEntry hce = hcache_fetch_email(fc.hc, buf, strlen(buf), 0);
    if (hce.email)
    {
      mutt_debug(LL_DEBUG2, "hcache_fetch_email %s\n", buf);
      e = hce.email;
      m->emails[m->msg_count] = e;
      e->edata = NULL;

      /* skip header marked as deleted in cache */
      if (e->deleted && !restore)
      {
        email_free(&e);
        if (mdata->bcache)
        {
          mutt_debug(LL_DEBUG2, "#2 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }
        continue;
      }

      e->read = false;
      e->old = false;
    }
    else
#endif
        if (mdata->deleted)
    {
      /* don't try to fetch header from removed newsgroup */
      continue;
    }
    else if (mdata->adata->hasOVER || mdata->adata->hasXOVER)
    {
      /* fallback to fetch overview */
      if (c_nntp_listgroup && mdata->adata->hasLISTGROUP)
        break;
      else
        continue;
    }
    else
    {
      /* fetch header from server */
      FILE *fp = mutt_file_mkstemp();
      if (!fp)
      {
        mutt_perror(_("Can't create temporary file"));
        rc = -1;
        break;
      }

      snprintf(buf, sizeof(buf), "HEAD " ANUM_FMT "\r\n", current);
      rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_tempfile, fp);
      if (rc)
      {
        mutt_file_fclose(&fp);
        if (rc < 0)
          break;

        /* invalid response */
        if (!mutt_str_startswith(buf, "423"))
        {
          mutt_error("HEAD: %s", buf);
          break;
        }

        /* no such article */
        if (mdata->bcache)
        {
          snprintf(buf, sizeof(buf), ANUM_FMT, current);
          mutt_debug(LL_DEBUG2, "#3 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }
        rc = 0;
        continue;
      }

      /* parse header */
      m->emails[m->msg_count] = email_new();
      e = m->emails[m->msg_count];
      e->env = mutt_rfc822_read_header(fp, e, false, false);
      e->received = e->date_sent;
      mutt_file_fclose(&fp);
    }

    /* save header in context */
    e->index = m->msg_count++;
    e->read = false;
    e->old = false;
    e->deleted = false;
    e->edata = nntp_edata_new();
    e->edata_free = nntp_edata_free;
    nntp_edata_get(e)->article_num = current;
    if (restore)
    {
      e->changed = true;
    }
    else
    {
      nntp_article_status(m, e, NULL, nntp_edata_get(e)->article_num);
      if (!e->read)
        nntp_parse_xref(m, e);
    }
    if (current > mdata->last_loaded)
      mdata->last_loaded = current;
    first_over = current + 1;
  }

  if (!c_nntp_listgroup || !mdata->adata->hasLISTGROUP)
    current = first_over;

  /* fetch overview information */
  if ((current <= last) && (rc == 0) && !mdata->deleted)
  {
    char *cmd = mdata->adata->hasOVER ? "OVER" : "XOVER";
    snprintf(buf, sizeof(buf), "%s " ANUM_FMT "-" ANUM_FMT "\r\n", cmd, current, last);
    rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, parse_overview_line, &fc);
    if (rc > 0)
    {
      mutt_error("%s: %s", cmd, buf);
    }
  }

  FREE(&fc.messages);
  progress_free(&fc.progress);
  if (rc != 0)
    return -1;
  mutt_clear_error();
  return 0;
}

/**
 * nntp_group_poll - Check newsgroup for new articles
 * @param mdata NNTP Mailbox data
 * @param update_stat Update the stats?
 * @retval  1 New articles found
 * @retval  0 No change
 * @retval -1 Lost connection
 */
static int nntp_group_poll(struct NntpMboxData *mdata, bool update_stat)
{
  char buf[1024] = { 0 };
  anum_t count = 0, first = 0, last = 0;

  /* use GROUP command to poll newsgroup */
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
    return -1;
  if (sscanf(buf, "211 " ANUM_FMT " " ANUM_FMT " " ANUM_FMT, &count, &first, &last) != 3)
    return 0;
  if ((first == mdata->first_message) && (last == mdata->last_message))
    return 0;

  /* articles have been renumbered */
  if (last < mdata->last_message)
  {
    mdata->last_cached = 0;
    if (mdata->newsrc_len)
    {
      MUTT_MEM_REALLOC(&mdata->newsrc_ent, 1, struct NewsrcEntry);
      mdata->newsrc_len = 1;
      mdata->newsrc_ent[0].first = 1;
      mdata->newsrc_ent[0].last = 0;
    }
  }
  mdata->first_message = first;
  mdata->last_message = last;
  if (!update_stat)
  {
    return 1;
  }
  else if (!last || (!mdata->newsrc_ent && !mdata->last_cached))
  {
    /* update counters */
    mdata->unread = count;
  }
  else
  {
    nntp_group_unread_stat(mdata);
  }
  return 1;
}

/**
 * check_mailbox - Check current newsgroup for new articles
 * @param m Mailbox
 * @retval enum #MxStatus
 *
 * Leave newsrc locked
 */
static enum MxStatus check_mailbox(struct Mailbox *m)
{
  if (!m || !m->mdata)
    return MX_STATUS_ERROR;

  struct NntpMboxData *mdata = m->mdata;
  struct NntpAccountData *adata = mdata->adata;
  time_t now = mutt_date_now();
  enum MxStatus rc = MX_STATUS_OK;
  struct HeaderCache *hc = NULL;

  const short c_nntp_poll = cs_subset_number(NeoMutt->sub, "nntp_poll");
  if (adata->check_time + c_nntp_poll > now)
    return MX_STATUS_OK;

  mutt_message(_("Checking for new messages..."));
  if (nntp_newsrc_parse(adata) < 0)
    return MX_STATUS_ERROR;

  adata->check_time = now;
  int rc2 = nntp_group_poll(mdata, false);
  if (rc2 < 0)
  {
    nntp_newsrc_close(adata);
    return -1;
  }
  if (rc2 != 0)
    nntp_active_save_cache(adata);

  /* articles have been renumbered, remove all emails */
  if (mdata->last_message < mdata->last_loaded)
  {
    for (int i = 0; i < m->msg_count; i++)
      email_free(&m->emails[i]);
    m->msg_count = 0;
    m->msg_tagged = 0;

    mdata->last_loaded = mdata->first_message - 1;
    const long c_nntp_context = cs_subset_long(NeoMutt->sub, "nntp_context");
    if (c_nntp_context && (mdata->last_message - mdata->last_loaded > c_nntp_context))
      mdata->last_loaded = mdata->last_message - c_nntp_context;

    rc = MX_STATUS_REOPENED;
  }

  /* .newsrc has been externally modified */
  if (adata->newsrc_modified)
  {
#ifdef USE_HCACHE
    unsigned char *messages = NULL;
    char buf[16] = { 0 };
    struct Email *e = NULL;
    anum_t first = mdata->first_message;

    const long c_nntp_context = cs_subset_long(NeoMutt->sub, "nntp_context");
    if (c_nntp_context && ((mdata->last_message - first + 1) > c_nntp_context))
      first = mdata->last_message - c_nntp_context + 1;
    messages = MUTT_MEM_CALLOC(mdata->last_loaded - first + 1, unsigned char);
    hc = nntp_hcache_open(mdata);
    nntp_hcache_update(mdata, hc);
#endif

    /* update flags according to .newsrc */
    int j = 0;
    for (int i = 0; i < m->msg_count; i++)
    {
      if (!m->emails[i])
        continue;
      bool flagged = false;
      anum_t anum = nntp_edata_get(m->emails[i])->article_num;

#ifdef USE_HCACHE
      /* check hcache for flagged and deleted flags */
      if (hc)
      {
        if ((anum >= first) && (anum <= mdata->last_loaded))
          messages[anum - first] = 1;

        snprintf(buf, sizeof(buf), ANUM_FMT, anum);
        struct HCacheEntry hce = hcache_fetch_email(hc, buf, strlen(buf), 0);
        if (hce.email)
        {
          bool deleted;

          mutt_debug(LL_DEBUG2, "#1 hcache_fetch_email %s\n", buf);
          e = hce.email;
          e->edata = NULL;
          deleted = e->deleted;
          flagged = e->flagged;
          email_free(&e);

          /* header marked as deleted, removing from context */
          if (deleted)
          {
            mutt_set_flag(m, m->emails[i], MUTT_TAG, false, true);
            email_free(&m->emails[i]);
            continue;
          }
        }
      }
#endif

      if (!m->emails[i]->changed)
      {
        m->emails[i]->flagged = flagged;
        m->emails[i]->read = false;
        m->emails[i]->old = false;
        nntp_article_status(m, m->emails[i], NULL, anum);
        if (!m->emails[i]->read)
          nntp_parse_xref(m, m->emails[i]);
      }
      m->emails[j++] = m->emails[i];
    }

#ifdef USE_HCACHE
    m->msg_count = j;

    /* restore headers without "deleted" flag */
    for (anum_t anum = first; anum <= mdata->last_loaded; anum++)
    {
      if (messages[anum - first])
        continue;

      snprintf(buf, sizeof(buf), ANUM_FMT, anum);
      struct HCacheEntry hce = hcache_fetch_email(hc, buf, strlen(buf), 0);
      if (hce.email)
      {
        mutt_debug(LL_DEBUG2, "#2 hcache_fetch_email %s\n", buf);
        mx_alloc_memory(m, m->msg_count);

        e = hce.email;
        m->emails[m->msg_count] = e;
        e->edata = NULL;
        if (e->deleted)
        {
          email_free(&e);
          if (mdata->bcache)
          {
            mutt_debug(LL_DEBUG2, "mutt_bcache_del %s\n", buf);
            mutt_bcache_del(mdata->bcache, buf);
          }
          continue;
        }

        m->msg_count++;
        e->read = false;
        e->old = false;
        e->edata = nntp_edata_new();
        e->edata_free = nntp_edata_free;
        nntp_edata_get(e)->article_num = anum;
        nntp_article_status(m, e, NULL, anum);
        if (!e->read)
          nntp_parse_xref(m, e);
      }
    }
    FREE(&messages);
#endif

    adata->newsrc_modified = false;
    rc = MX_STATUS_REOPENED;
  }

  /* some emails were removed, mailboxview must be updated */
  if (rc == MX_STATUS_REOPENED)
    mailbox_changed(m, NT_MAILBOX_INVALID);

  /* fetch headers of new articles */
  if (mdata->last_message > mdata->last_loaded)
  {
    int oldmsgcount = m->msg_count;
    bool verbose = m->verbose;
    m->verbose = false;
#ifdef USE_HCACHE
    if (!hc)
    {
      hc = nntp_hcache_open(mdata);
      nntp_hcache_update(mdata, hc);
    }
#endif
    int old_msg_count = m->msg_count;
    rc2 = nntp_fetch_headers(m, hc, mdata->last_loaded + 1, mdata->last_message, false);
    m->verbose = verbose;
    if (rc2 == 0)
    {
      if (m->msg_count > old_msg_count)
        mailbox_changed(m, NT_MAILBOX_INVALID);
      mdata->last_loaded = mdata->last_message;
    }
    if ((rc == MX_STATUS_OK) && (m->msg_count > oldmsgcount))
      rc = MX_STATUS_NEW_MAIL;
  }

#ifdef USE_HCACHE
  hcache_close(&hc);
#endif
  if (rc != MX_STATUS_OK)
    nntp_newsrc_close(adata);
  mutt_clear_error();
  return rc;
}

/**
 * nntp_date - Get date and time from server
 * @param adata NNTP server
 * @param now   Server time
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_date(struct NntpAccountData *adata, time_t *now)
{
  if (adata->hasDATE)
  {
    struct NntpMboxData mdata = { 0 };
    char buf[1024] = { 0 };
    struct tm tm = { 0 };

    mdata.adata = adata;
    mdata.group = NULL;
    mutt_str_copy(buf, "DATE\r\n", sizeof(buf));
    if (nntp_query(&mdata, buf, sizeof(buf)) < 0)
      return -1;

    if (sscanf(buf, "111 %4d%2d%2d%2d%2d%2d%*s", &tm.tm_year, &tm.tm_mon,
               &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6)
    {
      tm.tm_year -= 1900;
      tm.tm_mon--;
      *now = timegm(&tm);
      if (*now >= 0)
      {
        mutt_debug(LL_DEBUG1, "server time is %llu\n", (unsigned long long) *now);
        return 0;
      }
    }
  }
  *now = mutt_date_now();
  return 0;
}

/**
 * fetch_children - Parse XPAT line
 * @param line String to parse
 * @param data ChildCtx
 * @retval 0 Always
 */
static int fetch_children(char *line, void *data)
{
  struct ChildCtx *cc = data;
  anum_t anum = 0;

  if (!line || (sscanf(line, ANUM_FMT, &anum) != 1))
    return 0;
  for (unsigned int i = 0; i < cc->mailbox->msg_count; i++)
  {
    struct Email *e = cc->mailbox->emails[i];
    if (!e)
      break;
    if (nntp_edata_get(e)->article_num == anum)
      return 0;
  }
  if (cc->num >= cc->max)
  {
    cc->max *= 2;
    MUTT_MEM_REALLOC(&cc->child, cc->max, anum_t);
  }
  cc->child[cc->num++] = anum;
  return 0;
}

/**
 * nntp_open_connection - Connect to server, authenticate and get capabilities
 * @param adata NNTP server
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_open_connection(struct NntpAccountData *adata)
{
  if (adata->status == NNTP_OK)
    return 0;
  if (adata->status == NNTP_BYE)
    return -1;
  adata->status = NNTP_NONE;

  struct Connection *conn = adata->conn;
  if (mutt_socket_open(conn) < 0)
    return -1;

  char buf[256] = { 0 };
  int cap;
  bool posting = false, auth = true;
  int rc = -1;

  if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
  {
    nntp_connect_error(adata);
    goto done;
  }

  if (mutt_str_startswith(buf, "200"))
  {
    posting = true;
  }
  else if (!mutt_str_startswith(buf, "201"))
  {
    mutt_socket_close(conn);
    mutt_str_remove_trailing_ws(buf);
    mutt_error("%s", buf);
    goto done;
  }

  /* get initial capabilities */
  cap = nntp_capabilities(adata);
  if (cap < 0)
    goto done;

  /* tell news server to switch to mode reader if it isn't so */
  if (cap > 0)
  {
    if ((mutt_socket_send(conn, "MODE READER\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      nntp_connect_error(adata);
      goto done;
    }

    if (mutt_str_startswith(buf, "200"))
    {
      posting = true;
    }
    else if (mutt_str_startswith(buf, "201"))
    {
      posting = false;
    }
    else if (adata->hasCAPABILITIES)
    {
      /* error if has capabilities, ignore result if no capabilities */
      mutt_socket_close(conn);
      mutt_error(_("Could not switch to reader mode"));
      goto done;
    }

    /* recheck capabilities after MODE READER */
    if (adata->hasCAPABILITIES)
    {
      cap = nntp_capabilities(adata);
      if (cap < 0)
        goto done;
    }
  }

  mutt_message(_("Connected to %s. %s"), conn->account.host,
               posting ? _("Posting is ok") : _("Posting is NOT ok"));
  mutt_sleep(1);

#ifdef USE_SSL
  /* Attempt STARTTLS if available and desired. */
  const bool c_ssl_force_tls = cs_subset_bool(NeoMutt->sub, "ssl_force_tls");
  if ((adata->use_tls != 1) && (adata->hasSTARTTLS || c_ssl_force_tls))
  {
    if (adata->use_tls == 0)
    {
      adata->use_tls = c_ssl_force_tls ||
                               (query_quadoption(_("Secure connection with TLS?"),
                                                 NeoMutt->sub, "ssl_starttls") == MUTT_YES) ?
                           2 :
                           1;
    }
    if (adata->use_tls == 2)
    {
      if ((mutt_socket_send(conn, "STARTTLS\r\n") < 0) ||
          (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
      {
        nntp_connect_error(adata);
        goto done;
      }
      // Clear any data after the STARTTLS acknowledgement
      mutt_socket_empty(conn);
      if (!mutt_str_startswith(buf, "382"))
      {
        adata->use_tls = 0;
        mutt_error("STARTTLS: %s", buf);
      }
      else if (mutt_ssl_starttls(conn))
      {
        adata->use_tls = 0;
        adata->status = NNTP_NONE;
        mutt_socket_close(adata->conn);
        mutt_error(_("Could not negotiate TLS connection"));
        goto done;
      }
      else
      {
        /* recheck capabilities after STARTTLS */
        cap = nntp_capabilities(adata);
        if (cap < 0)
          goto done;
      }
    }
  }
#endif

  /* authentication required? */
  if (conn->account.flags & MUTT_ACCT_USER)
  {
    if (!conn->account.user[0])
      auth = false;
  }
  else
  {
    if ((mutt_socket_send(conn, "STAT\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      nntp_connect_error(adata);
      goto done;
    }
    if (!mutt_str_startswith(buf, "480"))
      auth = false;
  }

  /* authenticate */
  if (auth && (nntp_auth(adata) < 0))
    goto done;

  /* get final capabilities after authentication */
  if (adata->hasCAPABILITIES && (auth || (cap > 0)))
  {
    cap = nntp_capabilities(adata);
    if (cap < 0)
      goto done;
    if (cap > 0)
    {
      mutt_socket_close(conn);
      mutt_error(_("Could not switch to reader mode"));
      goto done;
    }
  }

  /* attempt features */
  if (nntp_attempt_features(adata) < 0)
    goto done;

  rc = 0;
  adata->status = NNTP_OK;

done:
  return rc;
}

/**
 * nntp_post - Post article
 * @param m   Mailbox
 * @param msg Message to post
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_post(struct Mailbox *m, const char *msg)
{
  struct NntpMboxData *mdata = NULL;
  struct NntpMboxData tmp_mdata = { 0 };
  char buf[1024] = { 0 };
  int rc = -1;

  if (m && (m->type == MUTT_NNTP))
  {
    mdata = m->mdata;
  }
  else
  {
    const char *const c_news_server = cs_subset_string(NeoMutt->sub, "news_server");
    CurrentNewsSrv = nntp_select_server(m, c_news_server, false);
    if (!CurrentNewsSrv)
      goto done;

    mdata = &tmp_mdata;
    mdata->adata = CurrentNewsSrv;
    mdata->group = NULL;
  }

  FILE *fp = mutt_file_fopen(msg, "r");
  if (!fp)
  {
    mutt_perror("%s", msg);
    goto done;
  }

  mutt_str_copy(buf, "POST\r\n", sizeof(buf));
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
  {
    mutt_file_fclose(&fp);
    goto done;
  }
  if (buf[0] != '3')
  {
    mutt_error(_("Can't post article: %s"), buf);
    mutt_file_fclose(&fp);
    goto done;
  }

  buf[0] = '.';
  buf[1] = '\0';
  while (fgets(buf + 1, sizeof(buf) - 2, fp))
  {
    size_t len = strlen(buf);
    if (buf[len - 1] == '\n')
    {
      buf[len - 1] = '\r';
      buf[len] = '\n';
      len++;
      buf[len] = '\0';
    }
    if (mutt_socket_send_d(mdata->adata->conn, (buf[1] == '.') ? buf : buf + 1,
                           MUTT_SOCK_LOG_FULL) < 0)
    {
      mutt_file_fclose(&fp);
      nntp_connect_error(mdata->adata);
      goto done;
    }
  }
  mutt_file_fclose(&fp);

  if (((buf[strlen(buf) - 1] != '\n') &&
       (mutt_socket_send_d(mdata->adata->conn, "\r\n", MUTT_SOCK_LOG_FULL) < 0)) ||
      (mutt_socket_send_d(mdata->adata->conn, ".\r\n", MUTT_SOCK_LOG_FULL) < 0) ||
      (mutt_socket_readln(buf, sizeof(buf), mdata->adata->conn) < 0))
  {
    nntp_connect_error(mdata->adata);
    goto done;
  }
  if (buf[0] != '2')
  {
    mutt_error(_("Can't post article: %s"), buf);
    goto done;
  }
  rc = 0;

done:
  return rc;
}

/**
 * nntp_active_fetch - Fetch list of all newsgroups from server
 * @param adata    NNTP server
 * @param mark_new Mark the groups as new
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_active_fetch(struct NntpAccountData *adata, bool mark_new)
{
  struct NntpMboxData tmp_mdata = { 0 };
  char msg[256] = { 0 };
  char buf[1024] = { 0 };
  unsigned int i;
  int rc;

  snprintf(msg, sizeof(msg), _("Loading list of groups from server %s..."),
           adata->conn->account.host);
  mutt_message("%s", msg);
  if (nntp_date(adata, &adata->newgroups_time) < 0)
    return -1;

  tmp_mdata.adata = adata;
  tmp_mdata.group = NULL;
  i = adata->groups_num;
  mutt_str_copy(buf, "LIST\r\n", sizeof(buf));
  rc = nntp_fetch_lines(&tmp_mdata, buf, sizeof(buf), msg, nntp_add_group, adata);
  if (rc)
  {
    if (rc > 0)
    {
      mutt_error("LIST: %s", buf);
    }
    return -1;
  }

  if (mark_new)
  {
    for (; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];
      mdata->has_new_mail = true;
    }
  }

  for (i = 0; i < adata->groups_num; i++)
  {
    struct NntpMboxData *mdata = adata->groups_list[i];

    if (mdata && mdata->deleted && !mdata->newsrc_ent)
    {
      nntp_delete_group_cache(mdata);
      mutt_hash_delete(adata->groups_hash, mdata->group, NULL);
      adata->groups_list[i] = NULL;
    }
  }

  const bool c_nntp_load_description = cs_subset_bool(NeoMutt->sub, "nntp_load_description");
  if (c_nntp_load_description)
    rc = get_description(&tmp_mdata, "*", _("Loading descriptions..."));

  nntp_active_save_cache(adata);
  if (rc < 0)
    return -1;
  mutt_clear_error();
  return 0;
}

/**
 * nntp_check_new_groups - Check for new groups/articles in subscribed groups
 * @param m     Mailbox
 * @param adata NNTP server
 * @retval  1 New groups found
 * @retval  0 No new groups
 * @retval -1 Error
 */
int nntp_check_new_groups(struct Mailbox *m, struct NntpAccountData *adata)
{
  struct NntpMboxData tmp_mdata = { 0 };
  time_t now = 0;
  char buf[1024] = { 0 };
  char *msg = _("Checking for new newsgroups...");
  unsigned int i;
  int rc, update_active = false;

  if (!adata || !adata->newgroups_time)
    return -1;

  /* check subscribed newsgroups for new articles */
  const bool c_show_new_news = cs_subset_bool(NeoMutt->sub, "show_new_news");
  if (c_show_new_news)
  {
    mutt_message(_("Checking for new messages..."));
    for (i = 0; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];

      if (mdata && mdata->subscribed)
      {
        rc = nntp_group_poll(mdata, true);
        if (rc < 0)
          return -1;
        if (rc > 0)
          update_active = true;
      }
    }
  }
  else if (adata->newgroups_time)
  {
    return 0;
  }

  /* get list of new groups */
  mutt_message("%s", msg);
  if (nntp_date(adata, &now) < 0)
    return -1;
  tmp_mdata.adata = adata;
  if (m && m->mdata)
    tmp_mdata.group = ((struct NntpMboxData *) m->mdata)->group;
  else
    tmp_mdata.group = NULL;
  i = adata->groups_num;
  struct tm tm = mutt_date_gmtime(adata->newgroups_time);
  snprintf(buf, sizeof(buf), "NEWGROUPS %02d%02d%02d %02d%02d%02d GMT\r\n",
           tm.tm_year % 100, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  rc = nntp_fetch_lines(&tmp_mdata, buf, sizeof(buf), msg, nntp_add_group, adata);
  if (rc)
  {
    if (rc > 0)
    {
      mutt_error("NEWGROUPS: %s", buf);
    }
    return -1;
  }

  /* new groups found */
  rc = 0;
  if (adata->groups_num != i)
  {
    int groups_num = i;

    adata->newgroups_time = now;
    for (; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];
      mdata->has_new_mail = true;
    }

    /* loading descriptions */
    const bool c_nntp_load_description = cs_subset_bool(NeoMutt->sub, "nntp_load_description");
    if (c_nntp_load_description)
    {
      unsigned int count = 0;
      struct Progress *progress = progress_new(MUTT_PROGRESS_READ, adata->groups_num - i);
      progress_set_message(progress, _("Loading descriptions..."));

      for (i = groups_num; i < adata->groups_num; i++)
      {
        struct NntpMboxData *mdata = adata->groups_list[i];

        if (get_description(mdata, NULL, NULL) < 0)
        {
          progress_free(&progress);
          return -1;
        }
        progress_update(progress, ++count, -1);
      }
      progress_free(&progress);
    }
    update_active = true;
    rc = 1;
  }
  if (update_active)
    nntp_active_save_cache(adata);
  mutt_clear_error();
  return rc;
}

/**
 * nntp_check_msgid - Fetch article by Message-ID
 * @param m     Mailbox
 * @param msgid Message ID
 * @retval  0 Success
 * @retval  1 No such article
 * @retval -1 Error
 */
int nntp_check_msgid(struct Mailbox *m, const char *msgid)
{
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  char buf[1024] = { 0 };

  FILE *fp = mutt_file_mkstemp();
  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    return -1;
  }

  snprintf(buf, sizeof(buf), "HEAD %s\r\n", msgid);
  int rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_tempfile, fp);
  if (rc)
  {
    mutt_file_fclose(&fp);
    if (rc < 0)
      return -1;
    if (mutt_str_startswith(buf, "430"))
      return 1;
    mutt_error("HEAD: %s", buf);
    return -1;
  }

  /* parse header */
  mx_alloc_memory(m, m->msg_count);
  m->emails[m->msg_count] = email_new();
  struct Email *e = m->emails[m->msg_count];
  e->edata = nntp_edata_new();
  e->edata_free = nntp_edata_free;
  e->env = mutt_rfc822_read_header(fp, e, false, false);
  mutt_file_fclose(&fp);

  /* get article number */
  if (e->env->xref)
  {
    nntp_parse_xref(m, e);
  }
  else
  {
    snprintf(buf, sizeof(buf), "STAT %s\r\n", msgid);
    if (nntp_query(mdata, buf, sizeof(buf)) < 0)
    {
      email_free(&e);
      return -1;
    }
    sscanf(buf + 4, ANUM_FMT, &nntp_edata_get(e)->article_num);
  }

  /* reset flags */
  e->read = false;
  e->old = false;
  e->deleted = false;
  e->changed = true;
  e->received = e->date_sent;
  e->index = m->msg_count++;
  mailbox_changed(m, NT_MAILBOX_INVALID);
  return 0;
}

/**
 * nntp_check_children - Fetch children of article with the Message-ID
 * @param m     Mailbox
 * @param msgid Message ID to find
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_check_children(struct Mailbox *m, const char *msgid)
{
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  char buf[256] = { 0 };
  int rc;
  struct HeaderCache *hc = NULL;

  if (!mdata || !mdata->adata)
    return -1;
  if (mdata->first_message > mdata->last_loaded)
    return 0;

  /* init context */
  struct ChildCtx cc = { 0 };
  cc.mailbox = m;
  cc.num = 0;
  cc.max = 10;
  cc.child = MUTT_MEM_MALLOC(cc.max, anum_t);

  /* fetch numbers of child messages */
  snprintf(buf, sizeof(buf), "XPAT References " ANUM_FMT "-" ANUM_FMT " *%s*\r\n",
           mdata->first_message, mdata->last_loaded, msgid);
  rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_children, &cc);
  if (rc)
  {
    FREE(&cc.child);
    if (rc > 0)
    {
      if (!mutt_str_startswith(buf, "500"))
      {
        mutt_error("XPAT: %s", buf);
      }
      else
      {
        mutt_error(_("Unable to find child articles because server does not support XPAT command"));
      }
    }
    return -1;
  }

  /* fetch all found messages */
  bool verbose = m->verbose;
  m->verbose = false;
#ifdef USE_HCACHE
  hc = nntp_hcache_open(mdata);
#endif
  int old_msg_count = m->msg_count;
  for (int i = 0; i < cc.num; i++)
  {
    rc = nntp_fetch_headers(m, hc, cc.child[i], cc.child[i], true);
    if (rc < 0)
      break;
  }
  if (m->msg_count > old_msg_count)
    mailbox_changed(m, NT_MAILBOX_INVALID);

#ifdef USE_HCACHE
  hcache_close(&hc);
#endif
  m->verbose = verbose;
  FREE(&cc.child);
  return (rc < 0) ? -1 : 0;
}

/**
 * nntp_compare_order - Restore the 'unsorted' order of emails - Implements ::sort_mail_t - @ingroup sort_mail_api
 */
int nntp_compare_order(const struct Email *a, const struct Email *b, bool reverse)
{
  anum_t na = nntp_edata_get((struct Email *) a)->article_num;
  anum_t nb = nntp_edata_get((struct Email *) b)->article_num;
  int result = (na == nb) ? 0 : (na > nb) ? 1 : -1;
  return reverse ? -result : result;
}

/**
 * nntp_ac_owns_path - Check whether an Account owns a Mailbox path - Implements MxOps::ac_owns_path() - @ingroup mx_ac_owns_path
 */
static bool nntp_ac_owns_path(struct Account *a, const char *path)
{
  return true;
}

/**
 * nntp_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add() - @ingroup mx_ac_add
 */
static bool nntp_ac_add(struct Account *a, struct Mailbox *m)
{
  return true;
}

/**
 * nntp_mbox_open - Open a Mailbox - Implements MxOps::mbox_open() - @ingroup mx_mbox_open
 */
static enum MxOpenReturns nntp_mbox_open(struct Mailbox *m)
{
  if (!m->account)
    return MX_OPEN_ERROR;

  char buf[8192] = { 0 };
  char server[1024] = { 0 };
  char *group = NULL;
  int rc;
  struct HeaderCache *hc = NULL;
  anum_t first = 0, last = 0, count = 0;

  struct Url *url = url_parse(mailbox_path(m));
  if (!url || !url->host || !url->path ||
      !((url->scheme == U_NNTP) || (url->scheme == U_NNTPS)))
  {
    url_free(&url);
    mutt_error(_("%s is an invalid newsgroup specification"), mailbox_path(m));
    return MX_OPEN_ERROR;
  }

  group = url->path;
  if (group[0] == '/') /* Skip a leading '/' */
    group++;

  url->path = strchr(url->path, '\0');
  url_tostring(url, server, sizeof(server), U_NO_FLAGS);

  mutt_account_hook(m->realpath);
  struct NntpAccountData *adata = m->account->adata;
  if (!adata)
    adata = CurrentNewsSrv;
  if (!adata)
  {
    adata = nntp_select_server(m, server, true);
    m->account->adata = adata;
    m->account->adata_free = nntp_adata_free;
  }

  if (!adata)
  {
    url_free(&url);
    return MX_OPEN_ERROR;
  }
  CurrentNewsSrv = adata;

  m->msg_count = 0;
  m->msg_unread = 0;
  m->vcount = 0;

  if (group[0] == '/')
    group++;

  /* find news group data structure */
  struct NntpMboxData *mdata = mutt_hash_find(adata->groups_hash, group);
  if (!mdata)
  {
    nntp_newsrc_close(adata);
    mutt_error(_("Newsgroup %s not found on the server"), group);
    url_free(&url);
    return MX_OPEN_ERROR;
  }

  m->rights &= ~MUTT_ACL_INSERT; // Clear the flag
  const bool c_save_unsubscribed = cs_subset_bool(NeoMutt->sub, "save_unsubscribed");
  if (!mdata->newsrc_ent && !mdata->subscribed && !c_save_unsubscribed)
    m->readonly = true;

  /* select newsgroup */
  mutt_message(_("Selecting %s..."), group);
  url_free(&url);
  buf[0] = '\0';
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
  {
    nntp_newsrc_close(adata);
    return MX_OPEN_ERROR;
  }

  /* newsgroup not found, remove it */
  if (mutt_str_startswith(buf, "411"))
  {
    mutt_error(_("Newsgroup %s has been removed from the server"), mdata->group);
    if (!mdata->deleted)
    {
      mdata->deleted = true;
      nntp_active_save_cache(adata);
    }
    if (mdata->newsrc_ent && !mdata->subscribed && !c_save_unsubscribed)
    {
      FREE(&mdata->newsrc_ent);
      mdata->newsrc_len = 0;
      nntp_delete_group_cache(mdata);
      nntp_newsrc_update(adata);
    }
  }
  else
  {
    /* parse newsgroup info */
    if (sscanf(buf, "211 " ANUM_FMT " " ANUM_FMT " " ANUM_FMT, &count, &first, &last) != 3)
    {
      nntp_newsrc_close(adata);
      mutt_error("GROUP: %s", buf);
      return MX_OPEN_ERROR;
    }
    mdata->first_message = first;
    mdata->last_message = last;
    mdata->deleted = false;

    /* get description if empty */
    const bool c_nntp_load_description = cs_subset_bool(NeoMutt->sub, "nntp_load_description");
    if (c_nntp_load_description && !mdata->desc)
    {
      if (get_description(mdata, NULL, NULL) < 0)
      {
        nntp_newsrc_close(adata);
        return MX_OPEN_ERROR;
      }
      if (mdata->desc)
        nntp_active_save_cache(adata);
    }
  }

  adata->check_time = mutt_date_now();
  m->mdata = mdata;
  // Every known newsgroup has an mdata which is stored in adata->groups_list.
  // Currently we don't let the Mailbox free the mdata.
  // m->mdata_free = nntp_mdata_free;
  if (!mdata->bcache && (mdata->newsrc_ent || mdata->subscribed || c_save_unsubscribed))
    mdata->bcache = mutt_bcache_open(&adata->conn->account, mdata->group);

  /* strip off extra articles if adding context is greater than $nntp_context */
  first = mdata->first_message;
  const long c_nntp_context = cs_subset_long(NeoMutt->sub, "nntp_context");
  if (c_nntp_context && ((mdata->last_message - first + 1) > c_nntp_context))
    first = mdata->last_message - c_nntp_context + 1;
  mdata->last_loaded = first ? first - 1 : 0;
  count = mdata->first_message;
  mdata->first_message = first;
  nntp_bcache_update(mdata);
  mdata->first_message = count;
#ifdef USE_HCACHE
  hc = nntp_hcache_open(mdata);
  nntp_hcache_update(mdata, hc);
#endif
  if (!hc)
    m->rights &= ~(MUTT_ACL_WRITE | MUTT_ACL_DELETE); // Clear the flags

  nntp_newsrc_close(adata);
  rc = nntp_fetch_headers(m, hc, first, mdata->last_message, false);
#ifdef USE_HCACHE
  hcache_close(&hc);
#endif
  if (rc < 0)
    return MX_OPEN_ERROR;
  mdata->last_loaded = mdata->last_message;
  adata->newsrc_modified = false;
  return MX_OPEN_OK;
}

/**
 * nntp_mbox_check - Check for new mail - Implements MxOps::mbox_check() - @ingroup mx_mbox_check
 * @param m          Mailbox
 * @retval enum #MxStatus
 */
static enum MxStatus nntp_mbox_check(struct Mailbox *m)
{
  enum MxStatus rc = check_mailbox(m);
  if (rc == MX_STATUS_OK)
  {
    struct NntpMboxData *mdata = m->mdata;
    struct NntpAccountData *adata = mdata->adata;
    nntp_newsrc_close(adata);
  }
  return rc;
}

/**
 * nntp_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync() - @ingroup mx_mbox_sync
 *
 * @note May also return values from check_mailbox()
 */
static enum MxStatus nntp_mbox_sync(struct Mailbox *m)
{
  struct NntpMboxData *mdata = m->mdata;

  /* check for new articles */
  mdata->adata->check_time = 0;
  enum MxStatus check = check_mailbox(m);
  if (check != MX_STATUS_OK)
    return check;

#ifdef USE_HCACHE
  mdata->last_cached = 0;
  struct HeaderCache *hc = nntp_hcache_open(mdata);
#endif

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    char buf[16] = { 0 };

    snprintf(buf, sizeof(buf), ANUM_FMT, nntp_edata_get(e)->article_num);
    if (mdata->bcache && e->deleted)
    {
      mutt_debug(LL_DEBUG2, "mutt_bcache_del %s\n", buf);
      mutt_bcache_del(mdata->bcache, buf);
    }

#ifdef USE_HCACHE
    if (hc && (e->changed || e->deleted))
    {
      if (e->deleted && !e->read)
        mdata->unread--;
      mutt_debug(LL_DEBUG2, "hcache_store_email %s\n", buf);
      hcache_store_email(hc, buf, strlen(buf), e, 0);
    }
#endif
  }

#ifdef USE_HCACHE
  if (hc)
  {
    hcache_close(&hc);
    mdata->last_cached = mdata->last_loaded;
  }
#endif

  /* save .newsrc entries */
  nntp_newsrc_gen_entries(m);
  nntp_newsrc_update(mdata->adata);
  nntp_newsrc_close(mdata->adata);
  return MX_STATUS_OK;
}

/**
 * nntp_mbox_close - Close a Mailbox - Implements MxOps::mbox_close() - @ingroup mx_mbox_close
 * @retval 0 Always
 */
static enum MxStatus nntp_mbox_close(struct Mailbox *m)
{
  struct NntpMboxData *mdata = m->mdata;
  struct NntpMboxData *tmp_mdata = NULL;
  if (!mdata)
    return MX_STATUS_OK;

  mdata->unread = m->msg_unread;

  nntp_acache_free(mdata);
  if (!mdata->adata || !mdata->adata->groups_hash || !mdata->group)
    return MX_STATUS_OK;

  tmp_mdata = mutt_hash_find(mdata->adata->groups_hash, mdata->group);
  if (!tmp_mdata || (tmp_mdata != mdata))
    nntp_mdata_free((void **) &mdata);
  return MX_STATUS_OK;
}

/**
 * nntp_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open() - @ingroup mx_msg_open
 */
static bool nntp_msg_open(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  struct NntpMboxData *mdata = m->mdata;
  char article[16] = { 0 };

  /* try to get article from cache */
  struct NntpAcache *acache = &mdata->acache[e->index % NNTP_ACACHE_LEN];
  if (acache->path)
  {
    if (acache->index == e->index)
    {
      msg->fp = mutt_file_fopen(acache->path, "r");
      if (msg->fp)
        return true;
    }
    else
    {
      /* clear previous entry */
      unlink(acache->path);
      FREE(&acache->path);
    }
  }
  snprintf(article, sizeof(article), ANUM_FMT, nntp_edata_get(e)->article_num);
  msg->fp = mutt_bcache_get(mdata->bcache, article);
  if (msg->fp)
  {
    if (nntp_edata_get(e)->parsed)
      return true;
  }
  else
  {
    /* don't try to fetch article from removed newsgroup */
    if (mdata->deleted)
      return false;

    /* create new cache file */
    const char *fetch_msg = _("Fetching message...");
    mutt_message("%s", fetch_msg);
    msg->fp = mutt_bcache_put(mdata->bcache, article);
    if (!msg->fp)
    {
      struct Buffer *tempfile = buf_pool_get();
      buf_mktemp(tempfile);
      acache->path = buf_strdup(tempfile);
      buf_pool_release(&tempfile);
      acache->index = e->index;
      msg->fp = mutt_file_fopen(acache->path, "w+");
      if (!msg->fp)
      {
        mutt_perror("%s", acache->path);
        unlink(acache->path);
        FREE(&acache->path);
        return false;
      }
    }

    /* fetch message to cache file */
    char buf[2048] = { 0 };
    snprintf(buf, sizeof(buf), "ARTICLE %s\r\n",
             nntp_edata_get(e)->article_num ? article : e->env->message_id);
    const int rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_tempfile, msg->fp);
    if (rc)
    {
      mutt_file_fclose(&msg->fp);
      if (acache->path)
      {
        unlink(acache->path);
        FREE(&acache->path);
      }
      if (rc > 0)
      {
        if (mutt_str_startswith(buf, nntp_edata_get(e)->article_num ? "423" : "430"))
        {
          mutt_error(_("Article %s not found on the server"),
                     nntp_edata_get(e)->article_num ? article : e->env->message_id);
        }
        else
        {
          mutt_error("ARTICLE: %s", buf);
        }
      }
      return false;
    }

    if (!acache->path)
      mutt_bcache_commit(mdata->bcache, article);
  }

  /* replace envelope with new one
   * hash elements must be updated because pointers will be changed */
  if (m->id_hash && e->env->message_id)
    mutt_hash_delete(m->id_hash, e->env->message_id, e);
  if (m->subj_hash && e->env->real_subj)
    mutt_hash_delete(m->subj_hash, e->env->real_subj, e);

  mutt_env_free(&e->env);
  e->env = mutt_rfc822_read_header(msg->fp, e, false, false);

  if (m->id_hash && e->env->message_id)
    mutt_hash_insert(m->id_hash, e->env->message_id, e);
  if (m->subj_hash && e->env->real_subj)
    mutt_hash_insert(m->subj_hash, e->env->real_subj, e);

  /* fix content length */
  if (!mutt_file_seek(msg->fp, 0, SEEK_END))
  {
    return false;
  }
  e->body->length = ftell(msg->fp) - e->body->offset;

  /* this is called in neomutt before the open which fetches the message,
   * which is probably wrong, but we just call it again here to handle
   * the problem instead of fixing it */
  nntp_edata_get(e)->parsed = true;
  mutt_parse_mime_message(e, msg->fp);

  /* these would normally be updated in mview_update(), but the
   * full headers aren't parsed with overview, so the information wasn't
   * available then */
  if (WithCrypto)
    e->security = crypt_query(e->body);

  rewind(msg->fp);
  mutt_clear_error();
  return true;
}

/**
 * nntp_msg_close - Close an email - Implements MxOps::msg_close() - @ingroup mx_msg_close
 *
 * @note May also return EOF Failure, see errno
 */
static int nntp_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * nntp_path_probe - Is this an NNTP Mailbox? - Implements MxOps::path_probe() - @ingroup mx_path_probe
 */
enum MailboxType nntp_path_probe(const char *path, const struct stat *st)
{
  if (mutt_istr_startswith(path, "news://"))
    return MUTT_NNTP;

  if (mutt_istr_startswith(path, "snews://"))
    return MUTT_NNTP;

  return MUTT_UNKNOWN;
}

/**
 * nntp_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon() - @ingroup mx_path_canon
 */
static int nntp_path_canon(struct Buffer *path)
{
  return 0;
}

/**
 * MxNntpOps - NNTP Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxNntpOps = {
  // clang-format off
  .type            = MUTT_NNTP,
  .name             = "nntp",
  .is_local         = false,
  .ac_owns_path     = nntp_ac_owns_path,
  .ac_add           = nntp_ac_add,
  .mbox_open        = nntp_mbox_open,
  .mbox_open_append = NULL,
  .mbox_check       = nntp_mbox_check,
  .mbox_check_stats = NULL,
  .mbox_sync        = nntp_mbox_sync,
  .mbox_close       = nntp_mbox_close,
  .msg_open         = nntp_msg_open,
  .msg_open_new     = NULL,
  .msg_commit       = NULL,
  .msg_close        = nntp_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = NULL,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = nntp_path_probe,
  .path_canon       = nntp_path_canon,
  // clang-format on
};
