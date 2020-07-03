/**
 * @file
 * Usenet network mailbox type; talk to an NNTP server
 *
 * @authors
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Andrej Gritsenko <andrej@lucky.net>
 * Copyright (C) 2000-2017 Vsevolod Volkov <vvv@mutt.org.ua>
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
 * @page nntp_nntp Usenet network mailbox type; talk to an NNTP server
 *
 * Usenet network mailbox type; talk to an NNTP server
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
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
#include "gui/lib.h"
#include "lib.h"
#include "hook.h"
#include "init.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_parse.h"
#include "mutt_socket.h"
#include "muttlib.h"
#include "mx.h"
#include "progress.h"
#include "sort.h"
#include "bcache/lib.h"
#include "hcache/lib.h"
#include "ncrypt/lib.h"
#ifdef USE_HCACHE
#include "protos.h"
#endif
#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif
#if defined(USE_SSL) || defined(USE_HCACHE)
#include "mutt.h"
#endif

struct stat;

/* These Config Variables are only used in nntp/nntp.c */
char *C_NntpAuthenticators; ///< Config: (nntp) Allowed authentication methods
short C_NntpContext; ///< Config: (nntp) Maximum number of articles to list (0 for all articles)
bool C_NntpListgroup; ///< Config: (nntp) Check all articles when opening a newsgroup
bool C_NntpLoadDescription; ///< Config: (nntp) Load descriptions for newsgroups when adding to the list
short C_NntpPoll; ///< Config: (nntp) Interval between checks for new posts
bool C_ShowNewNews; ///< Config: (nntp) Check for new newsgroups when entering the browser

struct NntpAccountData *CurrentNewsSrv;

const char *OverviewFmt = "Subject:\0"
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
  struct Progress progress;
  header_cache_t *hc;
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
 * nntp_adata_free - Free data attached to the Mailbox
 * @param[out] ptr NNTP data
 *
 * The NntpAccountData struct stores global NNTP data, such as the connection to
 * the database.  This function will close the database, free the resources and
 * the struct itself.
 */
static void nntp_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NntpAccountData *adata = *ptr;

  mutt_file_fclose(&adata->fp_newsrc);
  FREE(&adata->newsrc_file);
  FREE(&adata->authenticators);
  FREE(&adata->overview_fmt);
  FREE(&adata->conn);
  FREE(&adata->groups_list);
  mutt_hash_free(&adata->groups_hash);
  FREE(ptr);
}

/**
 * nntp_hashelem_free - Free our hash table data - Implements ::hash_hdata_free_t
 */
static void nntp_hashelem_free(int type, void *obj, intptr_t data)
{
  nntp_mdata_free(&obj);
}

/**
 * nntp_adata_new - Allocate and initialise a new NntpAccountData structure
 * @param conn Network connection
 * @retval ptr New NntpAccountData
 */
struct NntpAccountData *nntp_adata_new(struct Connection *conn)
{
  struct NntpAccountData *adata = mutt_mem_calloc(1, sizeof(struct NntpAccountData));
  adata->conn = conn;
  adata->groups_hash = mutt_hash_new(1009, MUTT_HASH_NO_FLAGS);
  mutt_hash_set_destructor(adata->groups_hash, nntp_hashelem_free, 0);
  adata->groups_max = 16;
  adata->groups_list =
      mutt_mem_malloc(adata->groups_max * sizeof(struct NntpMboxData *));
  return adata;
}

#if 0
/**
 * nntp_adata_get - Get the Account data for this mailbox
 * @retval ptr Private Account data
 */
struct NntpAccountData *nntp_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_NNTP))
    return NULL;
  struct Account *a = m->account;
  if (!a)
    return NULL;
  return a->adata;
}
#endif

/**
 * nntp_mdata_free - Free NntpMboxData, used to destroy hash elements
 * @param[out] ptr NNTP data
 */
void nntp_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NntpMboxData *mdata = *ptr;

  nntp_acache_free(mdata);
  mutt_bcache_close(&mdata->bcache);
  FREE(&mdata->newsrc_ent);
  FREE(&mdata->desc);
  FREE(ptr);
}

/**
 * nntp_edata_free - Free data attached to an Email
 * @param[out] ptr Email data
 */
static void nntp_edata_free(void **ptr)
{
  // struct NntpEmailData *edata = *ptr;
  FREE(ptr);
}

/**
 * nntp_edata_new - Create a new NntpEmailData for an Email
 * @retval ptr New NntpEmailData struct
 */
static struct NntpEmailData *nntp_edata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct NntpEmailData));
}

/**
 * nntp_edata_get - Get the private data for this Email
 * @param e Email
 * @retval ptr Private Email data
 */
struct NntpEmailData *nntp_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
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
  char buf[1024];
  char authinfo[1024] = { 0 };

  adata->hasCAPABILITIES = false;
  adata->hasSTARTTLS = false;
  adata->hasDATE = false;
  adata->hasLIST_NEWSGROUPS = false;
  adata->hasLISTGROUP = false;
  adata->hasLISTGROUPrange = false;
  adata->hasOVER = false;
  FREE(&adata->authenticators);

  if ((mutt_socket_send(conn, "CAPABILITIES\r\n") < 0) ||
      (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
  {
    return nntp_connect_error(adata);
  }

  /* no capabilities */
  if (!mutt_str_startswith(buf, "101"))
    return 1;
  adata->hasCAPABILITIES = true;

  /* parse capabilities */
  do
  {
    size_t plen = 0;
    if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
      return nntp_connect_error(adata);
    if (mutt_str_equal("STARTTLS", buf))
      adata->hasSTARTTLS = true;
    else if (mutt_str_equal("MODE-READER", buf))
      mode_reader = true;
    else if (mutt_str_equal("READER", buf))
    {
      adata->hasDATE = true;
      adata->hasLISTGROUP = true;
      adata->hasLISTGROUPrange = true;
    }
    else if ((plen = mutt_str_startswith(buf, "AUTHINFO ")))
    {
      mutt_str_cat(buf, sizeof(buf), " ");
      mutt_str_copy(authinfo, buf + plen - 1, sizeof(authinfo));
    }
#ifdef USE_SASL
    else if ((plen = mutt_str_startswith(buf, "SASL ")))
    {
      char *p = buf + plen;
      while (*p == ' ')
        p++;
      adata->authenticators = mutt_str_dup(p);
    }
#endif
    else if (mutt_str_equal("OVER", buf))
      adata->hasOVER = true;
    else if (mutt_str_startswith(buf, "LIST "))
    {
      char *p = strstr(buf, " NEWSGROUPS");
      if (p)
      {
        p += 11;
        if ((*p == '\0') || (*p == ' '))
          adata->hasLIST_NEWSGROUPS = true;
      }
    }
  } while (!mutt_str_equal(".", buf));
  *buf = '\0';
#ifdef USE_SASL
  if (adata->authenticators && strcasestr(authinfo, " SASL "))
    mutt_str_copy(buf, adata->authenticators, sizeof(buf));
#endif
  if (strcasestr(authinfo, " USER "))
  {
    if (*buf != '\0')
      mutt_str_cat(buf, sizeof(buf), " ");
    mutt_str_cat(buf, sizeof(buf), "USER");
  }
  mutt_str_replace(&adata->authenticators, buf);

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
  char buf[1024];

  /* no CAPABILITIES, trying DATE, LISTGROUP, LIST NEWSGROUPS */
  if (!adata->hasCAPABILITIES)
  {
    if ((mutt_socket_send(conn, "DATE\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      return nntp_connect_error(adata);
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasDATE = true;

    if ((mutt_socket_send(conn, "LISTGROUP\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      return nntp_connect_error(adata);
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasLISTGROUP = true;

    if ((mutt_socket_send(conn, "LIST NEWSGROUPS +\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      return nntp_connect_error(adata);
    }
    if (!mutt_str_startswith(buf, "500"))
      adata->hasLIST_NEWSGROUPS = true;
    if (mutt_str_startswith(buf, "215"))
    {
      do
      {
        if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
          return nntp_connect_error(adata);
      } while (!mutt_str_equal(".", buf));
    }
  }

  /* no LIST NEWSGROUPS, trying XGTITLE */
  if (!adata->hasLIST_NEWSGROUPS)
  {
    if ((mutt_socket_send(conn, "XGTITLE\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      return nntp_connect_error(adata);
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
      return nntp_connect_error(adata);
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
      return nntp_connect_error(adata);
    }
    if (!mutt_str_startswith(buf, "215"))
      adata->overview_fmt = mutt_str_dup(OverviewFmt);
    else
    {
      bool cont = false;
      size_t buflen = 2048, off = 0, b = 0;

      FREE(&adata->overview_fmt);
      adata->overview_fmt = mutt_mem_malloc(buflen);

      while (true)
      {
        if ((buflen - off) < 1024)
        {
          buflen *= 2;
          mutt_mem_realloc(&adata->overview_fmt, buflen);
        }

        const int chunk = mutt_socket_readln_d(adata->overview_fmt + off,
                                               buflen - off, conn, MUTT_SOCK_LOG_HDR);
        if (chunk < 0)
        {
          FREE(&adata->overview_fmt);
          return nntp_connect_error(adata);
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
          else if (strcmp(colon + 1, "full") != 0)
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
      mutt_mem_realloc(&adata->overview_fmt, off);
    }
  }
  return 0;
}

#ifdef USE_SASL
/**
 * nntp_memchr - look for a char in a binary buf, conveniently
 * @param haystack [in/out] input: start here, output: store address of hit
 * @param sentinel points just beyond (1 byte after) search area
 * @param needle the character to search for
 * @retval true found and updated haystack
 * @retval false not found
 */
static bool nntp_memchr(char **haystack, char *sentinel, int needle)
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
 * nntp_log_binbuf - log a buffer possibly containing NUL bytes
 * @param buf source buffer
 * @param len how many bytes from buf
 * @param pfx logging prefix (protocol etc.)
 * @param dbg which loglevel does message belong
 */
static void nntp_log_binbuf(const char *buf, size_t len, const char *pfx, int dbg)
{
  char tmp[1024];
  char *p = tmp;
  char *sentinel = tmp + len;

  if (C_DebugLevel < dbg)
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
  char buf[1024];
  char authenticators[1024] = "USER";
  char *method = NULL, *a = NULL, *p = NULL;
  unsigned char flags = conn->account.flags;

  while (true)
  {
    /* get login and password */
    if ((mutt_account_getuser(&conn->account) < 0) || (conn->account.user[0] == '\0') ||
        (mutt_account_getpass(&conn->account) < 0) || (conn->account.pass[0] == '\0'))
    {
      break;
    }

    /* get list of authenticators */
    if (C_NntpAuthenticators)
      mutt_str_copy(authenticators, C_NntpAuthenticators, sizeof(authenticators));
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
        char *m = NULL;

        if (!adata->authenticators)
          continue;
        m = strcasestr(adata->authenticators, method);
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
      if (strcmp(method, "USER") == 0)
      {
        mutt_message(_("Authenticating (%s)..."), method);
        snprintf(buf, sizeof(buf), "AUTHINFO USER %s\r\n", conn->account.user);
        if ((mutt_socket_send(conn, buf) < 0) ||
            (mutt_socket_readln_d(buf, sizeof(buf), conn, MUTT_SOCK_LOG_FULL) < 0))
        {
          break;
        }

        /* authenticated, password is not required */
        if (mutt_str_startswith(buf, "281"))
          return 0;

        /* username accepted, sending password */
        if (mutt_str_startswith(buf, "381"))
        {
          mutt_debug(MUTT_SOCK_LOG_FULL, "%d> AUTHINFO PASS *\n", conn->fd);
          snprintf(buf, sizeof(buf), "AUTHINFO PASS %s\r\n", conn->account.pass);
          if ((mutt_socket_send_d(conn, buf, MUTT_SOCK_LOG_FULL) < 0) ||
              (mutt_socket_readln_d(buf, sizeof(buf), conn, MUTT_SOCK_LOG_FULL) < 0))
          {
            break;
          }

          /* authenticated */
          if (mutt_str_startswith(buf, "281"))
            return 0;
        }

        /* server doesn't support AUTHINFO USER, trying next method */
        if (*buf == '5')
          continue;
      }
      else
      {
#ifdef USE_SASL
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
          mutt_debug(LL_DEBUG1,
                     "error starting SASL authentication exchange\n");
          continue;
        }

        mutt_message(_("Authenticating (%s)..."), method);
        snprintf(buf, sizeof(buf), "AUTHINFO SASL %s", method);

        /* looping protocol */
        while ((rc == SASL_CONTINUE) || ((rc == SASL_OK) && client_len))
        {
          /* send out client response */
          if (client_len)
          {
            nntp_log_binbuf(client_out, client_len, "SASL", MUTT_SOCK_LOG_FULL);
            if (*buf != '\0')
              mutt_str_cat(buf, sizeof(buf), " ");
            len = strlen(buf);
            if (sasl_encode64(client_out, client_len, buf + len,
                              sizeof(buf) - len, &len) != SASL_OK)
            {
              mutt_debug(LL_DEBUG1, "error base64-encoding client response\n");
              break;
            }
          }

          mutt_str_cat(buf, sizeof(buf), "\r\n");
          if (strchr(buf, ' '))
          {
            mutt_debug(MUTT_SOCK_LOG_CMD, "%d> AUTHINFO SASL %s%s\n", conn->fd,
                       method, client_len ? " sasl_data" : "");
          }
          else
            mutt_debug(MUTT_SOCK_LOG_CMD, "%d> sasl_data\n", conn->fd);
          client_len = 0;
          if ((mutt_socket_send_d(conn, buf, MUTT_SOCK_LOG_FULL) < 0) ||
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

          if (strcmp("=", inbuf + 4) == 0)
            len = 0;
          else if (sasl_decode64(inbuf + 4, strlen(inbuf + 4), buf,
                                 sizeof(buf) - 1, &len) != SASL_OK)
          {
            mutt_debug(LL_DEBUG1, "error base64-decoding server response\n");
            break;
          }
          else
            nntp_log_binbuf(buf, len, "SASL", MUTT_SOCK_LOG_FULL);

          while (true)
          {
            rc = sasl_client_step(saslconn, buf, len, &interaction, &client_out, &client_len);
            if (rc != SASL_INTERACT)
              break;
            mutt_sasl_interact(interaction);
          }
          if (*inbuf != '3')
            break;

          *buf = '\0';
        } /* looping protocol */

        if ((rc == SASL_OK) && (client_len == 0) && (*inbuf == '2'))
        {
          mutt_sasl_setup_conn(conn, saslconn);
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
#endif /* USE_SASL */
      }

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
    mutt_socket_close(conn);
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
  char buf[1024] = { 0 };

  if (adata->status == NNTP_BYE)
    return -1;

  while (true)
  {
    if (adata->status == NNTP_OK)
    {
      int rc = 0;

      if (*line)
        rc = mutt_socket_send(adata->conn, line);
      else if (mdata->group)
      {
        snprintf(buf, sizeof(buf), "GROUP %s\r\n", mdata->group);
        rc = mutt_socket_send(adata->conn, buf);
      }
      if (rc >= 0)
        rc = mutt_socket_readln(buf, sizeof(buf), adata->conn);
      if (rc >= 0)
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
      if (mutt_yesorno(buf, MUTT_YES) != MUTT_YES)
      {
        adata->status = NNTP_BYE;
        return -1;
      }
    }

    /* select newsgroup after reconnection */
    if (mdata->group)
    {
      snprintf(buf, sizeof(buf), "GROUP %s\r\n", mdata->group);
      if ((mutt_socket_send(adata->conn, buf) < 0) ||
          (mutt_socket_readln(buf, sizeof(buf), adata->conn) < 0))
      {
        return nntp_connect_error(adata);
      }
    }
    if (*line == '\0')
      break;
  }

  mutt_str_copy(line, buf, linelen);
  return 0;
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
    char buf[1024];
    char *line = NULL;
    unsigned int lines = 0;
    size_t off = 0;
    struct Progress progress;

    if (msg)
      mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, 0);

    mutt_str_copy(buf, query, sizeof(buf));
    if (nntp_query(mdata, buf, sizeof(buf)) < 0)
      return -1;
    if (buf[0] != '2')
    {
      mutt_str_copy(query, buf, qlen);
      return 1;
    }

    line = mutt_mem_malloc(sizeof(buf));
    rc = 0;

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
        off += strlen(p);
      else
      {
        if (msg)
          mutt_progress_update(&progress, ++lines, -1);

        if ((rc == 0) && (func(line, data) < 0))
          rc = -2;
        off = 0;
      }

      mutt_mem_realloc(&line, off + sizeof(buf));
    }
    FREE(&line);
    func(NULL, data);
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
    desc = strchr(line, '\0');

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
  char buf[256];
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
    anum_t anum;

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
    if (sscanf(colon, ANUM, &anum) != 1)
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
  anum_t anum;

  if (!line)
    return 0;
  if (sscanf(line, ANUM, &anum) != 1)
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
  anum_t anum;

  /* parse article number */
  field = strchr(line, '\t');
  if (field)
    *field++ = '\0';
  if (sscanf(line, ANUM, &anum) != 1)
    return 0;
  mutt_debug(LL_DEBUG2, "" ANUM "\n", anum);

  /* out of bounds */
  if ((anum < fc->first) || (anum > fc->last))
    return 0;

  /* not in LISTGROUP */
  if (!fc->messages[anum - fc->first])
  {
    /* progress */
    if (m->verbose)
      mutt_progress_update(&fc->progress, anum - fc->first + 1, -1);
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
  if (m->msg_count >= m->email_max)
    mx_alloc_memory(m);

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
    char buf[16];

    /* try to replace with header from cache */
    snprintf(buf, sizeof(buf), "%u", anum);
    struct HCacheEntry hce = mutt_hcache_fetch(fc->hc, buf, strlen(buf), 0);
    if (hce.email)
    {
      mutt_debug(LL_DEBUG2, "mutt_hcache_fetch %s\n", buf);
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

    /* not cached yet, store header */
    else
    {
      mutt_debug(LL_DEBUG2, "mutt_hcache_store %s\n", buf);
      mutt_hcache_store(fc->hc, buf, strlen(buf), e, 0);
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
      e->changed = true;
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
    email_free(&e);

  /* progress */
  if (m->verbose)
    mutt_progress_update(&fc->progress, anum - fc->first + 1, -1);
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
  struct FetchCtx fc;
  struct Email *e = NULL;
  char buf[8192];
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
  fc.messages = mutt_mem_calloc(last - first + 1, sizeof(unsigned char));
  if (!fc.messages)
    return -1;
  fc.hc = hc;

  /* fetch list of articles */
  if (C_NntpListgroup && mdata->adata->hasLISTGROUP && !mdata->deleted)
  {
    if (m->verbose)
      mutt_message(_("Fetching list of articles..."));
    if (mdata->adata->hasLISTGROUPrange)
      snprintf(buf, sizeof(buf), "LISTGROUP %s %u-%u\r\n", mdata->group, first, last);
    else
      snprintf(buf, sizeof(buf), "LISTGROUP %s\r\n", mdata->group);
    rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_numbers, &fc);
    if (rc > 0)
    {
      mutt_error("LISTGROUP: %s", buf);
    }
    if (rc == 0)
    {
      for (current = first; (current <= last) && (rc == 0); current++)
      {
        if (fc.messages[current - first])
          continue;

        snprintf(buf, sizeof(buf), "%u", current);
        if (mdata->bcache)
        {
          mutt_debug(LL_DEBUG2, "#1 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }

#ifdef USE_HCACHE
        if (fc.hc)
        {
          mutt_debug(LL_DEBUG2, "mutt_hcache_delete_header %s\n", buf);
          mutt_hcache_delete_header(fc.hc, buf, strlen(buf));
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
    mutt_progress_init(&fc.progress, _("Fetching message headers..."),
                       MUTT_PROGRESS_READ, last - first + 1);
  }
  for (current = first; (current <= last) && (rc == 0); current++)
  {
    if (m->verbose)
      mutt_progress_update(&fc.progress, current - first + 1, -1);

#ifdef USE_HCACHE
    snprintf(buf, sizeof(buf), "%u", current);
#endif

    /* delete header from cache that does not exist on server */
    if (!fc.messages[current - first])
      continue;

    /* allocate memory for headers */
    if (m->msg_count >= m->email_max)
      mx_alloc_memory(m);

#ifdef USE_HCACHE
    /* try to fetch header from cache */
    struct HCacheEntry hce = mutt_hcache_fetch(fc.hc, buf, strlen(buf), 0);
    if (hce.email)
    {
      mutt_debug(LL_DEBUG2, "mutt_hcache_fetch %s\n", buf);
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

    /* fallback to fetch overview */
    else if (mdata->adata->hasOVER || mdata->adata->hasXOVER)
    {
      if (C_NntpListgroup && mdata->adata->hasLISTGROUP)
        break;
      else
        continue;
    }

    /* fetch header from server */
    else
    {
      FILE *fp = mutt_file_mkstemp();
      if (!fp)
      {
        mutt_perror(_("Can't create temporary file"));
        rc = -1;
        break;
      }

      snprintf(buf, sizeof(buf), "HEAD %u\r\n", current);
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
          snprintf(buf, sizeof(buf), "%u", current);
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
      e->changed = true;
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

  if (!C_NntpListgroup || !mdata->adata->hasLISTGROUP)
    current = first_over;

  /* fetch overview information */
  if ((current <= last) && (rc == 0) && !mdata->deleted)
  {
    char *cmd = mdata->adata->hasOVER ? "OVER" : "XOVER";
    snprintf(buf, sizeof(buf), "%s %u-%u\r\n", cmd, current, last);
    rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, parse_overview_line, &fc);
    if (rc > 0)
    {
      mutt_error("%s: %s", cmd, buf);
    }
  }

  FREE(&fc.messages);
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
  anum_t count, first, last;

  /* use GROUP command to poll newsgroup */
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
    return -1;
  if (sscanf(buf, "211 " ANUM " " ANUM " " ANUM, &count, &first, &last) != 3)
    return 0;
  if ((first == mdata->first_message) && (last == mdata->last_message))
    return 0;

  /* articles have been renumbered */
  if (last < mdata->last_message)
  {
    mdata->last_cached = 0;
    if (mdata->newsrc_len)
    {
      mutt_mem_realloc(&mdata->newsrc_ent, sizeof(struct NewsrcEntry));
      mdata->newsrc_len = 1;
      mdata->newsrc_ent[0].first = 1;
      mdata->newsrc_ent[0].last = 0;
    }
  }
  mdata->first_message = first;
  mdata->last_message = last;
  if (!update_stat)
    return 1;

  /* update counters */
  else if (!last || (!mdata->newsrc_ent && !mdata->last_cached))
    mdata->unread = count;
  else
    nntp_group_unread_stat(mdata);
  return 1;
}

/**
 * check_mailbox - Check current newsgroup for new articles
 * @param m Mailbox
 * @retval #MUTT_REOPENED Articles have been renumbered or removed from server
 * @retval #MUTT_NEW_MAIL New articles found
 * @retval  0             No change
 * @retval -1             Lost connection
 *
 * Leave newsrc locked
 */
static int check_mailbox(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  struct NntpAccountData *adata = mdata->adata;
  time_t now = mutt_date_epoch();
  int rc = 0;
  void *hc = NULL;

  if (adata->check_time + C_NntpPoll > now)
    return 0;

  mutt_message(_("Checking for new messages..."));
  if (nntp_newsrc_parse(adata) < 0)
    return -1;

  adata->check_time = now;
  int rc2 = nntp_group_poll(mdata, false);
  if (rc2 < 0)
  {
    nntp_newsrc_close(adata);
    return -1;
  }
  if (rc2 != 0)
    nntp_active_save_cache(adata);

  /* articles have been renumbered, remove all headers */
  if (mdata->last_message < mdata->last_loaded)
  {
    for (int i = 0; i < m->msg_count; i++)
      email_free(&m->emails[i]);
    m->msg_count = 0;
    m->msg_tagged = 0;

    if (mdata->last_message < mdata->last_loaded)
    {
      mdata->last_loaded = mdata->first_message - 1;
      if (C_NntpContext && (mdata->last_message - mdata->last_loaded > C_NntpContext))
        mdata->last_loaded = mdata->last_message - C_NntpContext;
    }
    rc = MUTT_REOPENED;
  }

  /* .newsrc has been externally modified */
  if (adata->newsrc_modified)
  {
#ifdef USE_HCACHE
    unsigned char *messages = NULL;
    char buf[16];
    struct Email *e = NULL;
    anum_t first = mdata->first_message;

    if (C_NntpContext && (mdata->last_message - first + 1 > C_NntpContext))
      first = mdata->last_message - C_NntpContext + 1;
    messages = mutt_mem_calloc(mdata->last_loaded - first + 1, sizeof(unsigned char));
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

        snprintf(buf, sizeof(buf), "%u", anum);
        struct HCacheEntry hce = mutt_hcache_fetch(hc, buf, strlen(buf), 0);
        if (hce.email)
        {
          bool deleted;

          mutt_debug(LL_DEBUG2, "#1 mutt_hcache_fetch %s\n", buf);
          e = hce.email;
          e->edata = NULL;
          deleted = e->deleted;
          flagged = e->flagged;
          email_free(&e);

          /* header marked as deleted, removing from context */
          if (deleted)
          {
            mutt_set_flag(m, m->emails[i], MUTT_TAG, false);
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

      snprintf(buf, sizeof(buf), "%u", anum);
      struct HCacheEntry hce = mutt_hcache_fetch(hc, buf, strlen(buf), 0);
      if (hce.email)
      {
        mutt_debug(LL_DEBUG2, "#2 mutt_hcache_fetch %s\n", buf);
        if (m->msg_count >= m->email_max)
          mx_alloc_memory(m);

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
    rc = MUTT_REOPENED;
  }

  /* some headers were removed, context must be updated */
  if (rc == MUTT_REOPENED)
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
    if ((rc == 0) && (m->msg_count > oldmsgcount))
      rc = MUTT_NEW_MAIL;
  }

#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif
  if (rc)
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
    char buf[1024];
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
        mutt_debug(LL_DEBUG1, "server time is %lu\n", *now);
        return 0;
      }
    }
  }
  *now = mutt_date_epoch();
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
  anum_t anum;

  if (!line || (sscanf(line, ANUM, &anum) != 1))
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
    mutt_mem_realloc(&cc->child, sizeof(anum_t) * cc->max);
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
  struct Connection *conn = adata->conn;
  char buf[256];
  int cap;
  bool posting = false, auth = true;

  if (adata->status == NNTP_OK)
    return 0;
  if (adata->status == NNTP_BYE)
    return -1;
  adata->status = NNTP_NONE;

  if (mutt_socket_open(conn) < 0)
    return -1;

  if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    return nntp_connect_error(adata);

  if (mutt_str_startswith(buf, "200"))
    posting = true;
  else if (!mutt_str_startswith(buf, "201"))
  {
    mutt_socket_close(conn);
    mutt_str_remove_trailing_ws(buf);
    mutt_error("%s", buf);
    return -1;
  }

  /* get initial capabilities */
  cap = nntp_capabilities(adata);
  if (cap < 0)
    return -1;

  /* tell news server to switch to mode reader if it isn't so */
  if (cap > 0)
  {
    if ((mutt_socket_send(conn, "MODE READER\r\n") < 0) ||
        (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
    {
      return nntp_connect_error(adata);
    }

    if (mutt_str_startswith(buf, "200"))
      posting = true;
    else if (mutt_str_startswith(buf, "201"))
      posting = false;
    /* error if has capabilities, ignore result if no capabilities */
    else if (adata->hasCAPABILITIES)
    {
      mutt_socket_close(conn);
      mutt_error(_("Could not switch to reader mode"));
      return -1;
    }

    /* recheck capabilities after MODE READER */
    if (adata->hasCAPABILITIES)
    {
      cap = nntp_capabilities(adata);
      if (cap < 0)
        return -1;
    }
  }

  mutt_message(_("Connected to %s. %s"), conn->account.host,
               posting ? _("Posting is ok") : _("Posting is NOT ok"));
  mutt_sleep(1);

#ifdef USE_SSL
  /* Attempt STARTTLS if available and desired. */
  if ((adata->use_tls != 1) && (adata->hasSTARTTLS || C_SslForceTls))
  {
    if (adata->use_tls == 0)
    {
      adata->use_tls =
          C_SslForceTls ||
                  (query_quadoption(C_SslStarttls,
                                    _("Secure connection with TLS?")) == MUTT_YES) ?
              2 :
              1;
    }
    if (adata->use_tls == 2)
    {
      if ((mutt_socket_send(conn, "STARTTLS\r\n") < 0) ||
          (mutt_socket_readln(buf, sizeof(buf), conn) < 0))
      {
        return nntp_connect_error(adata);
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
        return -1;
      }
      else
      {
        /* recheck capabilities after STARTTLS */
        cap = nntp_capabilities(adata);
        if (cap < 0)
          return -1;
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
      return nntp_connect_error(adata);
    }
    if (!mutt_str_startswith(buf, "480"))
      auth = false;
  }

  /* authenticate */
  if (auth && (nntp_auth(adata) < 0))
    return -1;

  /* get final capabilities after authentication */
  if (adata->hasCAPABILITIES && (auth || (cap > 0)))
  {
    cap = nntp_capabilities(adata);
    if (cap < 0)
      return -1;
    if (cap > 0)
    {
      mutt_socket_close(conn);
      mutt_error(_("Could not switch to reader mode"));
      return -1;
    }
  }

  /* attempt features */
  if (nntp_attempt_features(adata) < 0)
    return -1;

  adata->status = NNTP_OK;
  return 0;
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
  char buf[1024];

  if (m && (m->type == MUTT_NNTP))
    mdata = m->mdata;
  else
  {
    CurrentNewsSrv = nntp_select_server(m, C_NewsServer, false);
    if (!CurrentNewsSrv)
      return -1;

    mdata = &tmp_mdata;
    mdata->adata = CurrentNewsSrv;
    mdata->group = NULL;
  }

  FILE *fp = mutt_file_fopen(msg, "r");
  if (!fp)
  {
    mutt_perror(msg);
    return -1;
  }

  mutt_str_copy(buf, "POST\r\n", sizeof(buf));
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
  {
    mutt_file_fclose(&fp);
    return -1;
  }
  if (buf[0] != '3')
  {
    mutt_error(_("Can't post article: %s"), buf);
    mutt_file_fclose(&fp);
    return -1;
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
      return nntp_connect_error(mdata->adata);
    }
  }
  mutt_file_fclose(&fp);

  if (((buf[strlen(buf) - 1] != '\n') &&
       (mutt_socket_send_d(mdata->adata->conn, "\r\n", MUTT_SOCK_LOG_FULL) < 0)) ||
      (mutt_socket_send_d(mdata->adata->conn, ".\r\n", MUTT_SOCK_LOG_FULL) < 0) ||
      (mutt_socket_readln(buf, sizeof(buf), mdata->adata->conn) < 0))
  {
    return nntp_connect_error(mdata->adata);
  }
  if (buf[0] != '2')
  {
    mutt_error(_("Can't post article: %s"), buf);
    return -1;
  }
  return 0;
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
  char msg[256];
  char buf[1024];
  unsigned int i;
  int rc;

  snprintf(msg, sizeof(msg), _("Loading list of groups from server %s..."),
           adata->conn->account.host);
  mutt_message(msg);
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

  if (C_NntpLoadDescription)
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
  time_t now;
  char buf[1024];
  char *msg = _("Checking for new newsgroups...");
  unsigned int i;
  int rc, update_active = false;

  if (!adata || !adata->newgroups_time)
    return -1;

  /* check subscribed newsgroups for new articles */
  if (C_ShowNewNews)
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
    return 0;

  /* get list of new groups */
  mutt_message(msg);
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
    if (C_NntpLoadDescription)
    {
      unsigned int count = 0;
      struct Progress progress;

      mutt_progress_init(&progress, _("Loading descriptions..."),
                         MUTT_PROGRESS_READ, adata->groups_num - i);
      for (i = groups_num; i < adata->groups_num; i++)
      {
        struct NntpMboxData *mdata = adata->groups_list[i];

        if (get_description(mdata, NULL, NULL) < 0)
          return -1;
        mutt_progress_update(&progress, ++count, -1);
      }
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
  char buf[1024];

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
  if (m->msg_count == m->email_max)
    mx_alloc_memory(m);
  m->emails[m->msg_count] = email_new();
  struct Email *e = m->emails[m->msg_count];
  e->edata = nntp_edata_new();
  e->edata_free = nntp_edata_free;
  e->env = mutt_rfc822_read_header(fp, e, false, false);
  mutt_file_fclose(&fp);

  /* get article number */
  if (e->env->xref)
    nntp_parse_xref(m, e);
  else
  {
    snprintf(buf, sizeof(buf), "STAT %s\r\n", msgid);
    if (nntp_query(mdata, buf, sizeof(buf)) < 0)
    {
      email_free(&e);
      return -1;
    }
    sscanf(buf + 4, ANUM, &nntp_edata_get(e)->article_num);
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
  struct ChildCtx cc;
  char buf[256];
  int rc;
  void *hc = NULL;

  if (!mdata || !mdata->adata)
    return -1;
  if (mdata->first_message > mdata->last_loaded)
    return 0;

  /* init context */
  cc.mailbox = m;
  cc.num = 0;
  cc.max = 10;
  cc.child = mutt_mem_malloc(sizeof(anum_t) * cc.max);

  /* fetch numbers of child messages */
  snprintf(buf, sizeof(buf), "XPAT References %u-%u *%s*\r\n",
           mdata->first_message, mdata->last_loaded, msgid);
  rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, fetch_children, &cc);
  if (rc)
  {
    FREE(&cc.child);
    if (rc > 0)
    {
      if (!mutt_str_startswith(buf, "500"))
        mutt_error("XPAT: %s", buf);
      else
      {
        mutt_error(_("Unable to find child articles because server does not "
                     "support XPAT command"));
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
  mutt_hcache_close(hc);
#endif
  m->verbose = verbose;
  FREE(&cc.child);
  return (rc < 0) ? -1 : 0;
}

/**
 * nntp_compare_order - Sort to mailbox order - Implements ::sort_t
 */
int nntp_compare_order(const void *a, const void *b)
{
  const struct Email *ea = *(struct Email const *const *) a;
  const struct Email *eb = *(struct Email const *const *) b;

  anum_t na = nntp_edata_get((struct Email *) ea)->article_num;
  anum_t nb = nntp_edata_get((struct Email *) eb)->article_num;
  int result = (na == nb) ? 0 : (na > nb) ? 1 : -1;
  result = perform_auxsort(result, a, b);
  return SORT_CODE(result);
}

/**
 * nntp_ac_find - Find an Account that matches a Mailbox path - Implements MxOps::ac_find()
 */
static struct Account *nntp_ac_find(struct Account *a, const char *path)
{
#if 0
  if (!a || (a->type != MUTT_NNTP) || !path)
    return NULL;

  struct Url url = { 0 };
  char tmp[PATH_MAX];
  mutt_str_copy(tmp, path, sizeof(tmp));
  url_parse(&url, tmp);

  struct ImapAccountData *adata = a->data;
  struct ConnAccount *cac = &adata->conn_account;

  if (!mutt_istr_equal(url.host, cac->host))
    return NULL;

  if (!mutt_istr_equal(url.user, cac->user))
    return NULL;

  // if (mutt_str_equal(path, a->mailbox->realpath))
  //   return a;
#endif
  return a;
}

/**
 * nntp_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add()
 */
static int nntp_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || (m->type != MUTT_NNTP))
    return -1;
  return 0;
}

/**
 * nntp_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 */
static int nntp_mbox_open(struct Mailbox *m)
{
  if (!m || !m->account)
    return -1;

  char buf[8192];
  char server[1024];
  char *group = NULL;
  int rc;
  void *hc = NULL;
  anum_t first, last, count = 0;

  struct Url *url = url_parse(mailbox_path(m));
  if (!url || !url->host || !url->path ||
      !((url->scheme == U_NNTP) || (url->scheme == U_NNTPS)))
  {
    url_free(&url);
    mutt_error(_("%s is an invalid newsgroup specification"), mailbox_path(m));
    return -1;
  }

  group = url->path;
  if (group[0] == '/') /* Skip a leading '/' */
    group++;

  url->path = strchr(url->path, '\0');
  url_tostring(url, server, sizeof(server), 0);

  mutt_account_hook(m->realpath);
  struct NntpAccountData *adata = m->account->adata;
  if (!adata)
  {
    adata = nntp_select_server(m, server, true);
    m->account->adata = adata;
    m->account->adata_free = nntp_adata_free;
  }

  if (!adata)
  {
    url_free(&url);
    return -1;
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
    return -1;
  }

  m->rights &= ~MUTT_ACL_INSERT; // Clear the flag
  if (!mdata->newsrc_ent && !mdata->subscribed && !C_SaveUnsubscribed)
    m->readonly = true;

  /* select newsgroup */
  mutt_message(_("Selecting %s..."), group);
  url_free(&url);
  buf[0] = '\0';
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
  {
    nntp_newsrc_close(adata);
    return -1;
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
    if (mdata->newsrc_ent && !mdata->subscribed && !C_SaveUnsubscribed)
    {
      FREE(&mdata->newsrc_ent);
      mdata->newsrc_len = 0;
      nntp_delete_group_cache(mdata);
      nntp_newsrc_update(adata);
    }
  }

  /* parse newsgroup info */
  else
  {
    if (sscanf(buf, "211 " ANUM " " ANUM " " ANUM, &count, &first, &last) != 3)
    {
      nntp_newsrc_close(adata);
      mutt_error("GROUP: %s", buf);
      return -1;
    }
    mdata->first_message = first;
    mdata->last_message = last;
    mdata->deleted = false;

    /* get description if empty */
    if (C_NntpLoadDescription && !mdata->desc)
    {
      if (get_description(mdata, NULL, NULL) < 0)
      {
        nntp_newsrc_close(adata);
        return -1;
      }
      if (mdata->desc)
        nntp_active_save_cache(adata);
    }
  }

  adata->check_time = mutt_date_epoch();
  m->mdata = mdata;
  // Every known newsgroup has an mdata which is stored in adata->groups_list.
  // Currently we don't let the Mailbox free the mdata.
  // m->mdata_free = nntp_mdata_free;
  if (!mdata->bcache && (mdata->newsrc_ent || mdata->subscribed || C_SaveUnsubscribed))
    mdata->bcache = mutt_bcache_open(&adata->conn->account, mdata->group);

  /* strip off extra articles if adding context is greater than $nntp_context */
  first = mdata->first_message;
  if (C_NntpContext && (mdata->last_message - first + 1 > C_NntpContext))
    first = mdata->last_message - C_NntpContext + 1;
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
  mutt_hcache_close(hc);
#endif
  if (rc < 0)
    return -1;
  mdata->last_loaded = mdata->last_message;
  adata->newsrc_modified = false;
  return 0;
}

/**
 * nntp_mbox_check - Check for new mail - Implements MxOps::mbox_check()
 * @param m          Mailbox
 * @retval #MUTT_REOPENED Articles have been renumbered or removed from server
 * @retval #MUTT_NEW_MAIL New articles found
 * @retval  0             No change
 * @retval -1             Lost connection
 */
static int nntp_mbox_check(struct Mailbox *m)
{
  if (!m)
    return -1;

  int rc = check_mailbox(m);
  if (rc == 0)
  {
    struct NntpMboxData *mdata = m->mdata;
    struct NntpAccountData *adata = mdata->adata;
    nntp_newsrc_close(adata);
  }
  return rc;
}

/**
 * nntp_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 *
 * @note May also return values from check_mailbox()
 */
static int nntp_mbox_sync(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  int rc;

  /* check for new articles */
  mdata->adata->check_time = 0;
  rc = check_mailbox(m);
  if (rc)
    return rc;

#ifdef USE_HCACHE
  mdata->last_cached = 0;
  header_cache_t *hc = nntp_hcache_open(mdata);
#endif

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    char buf[16];

    snprintf(buf, sizeof(buf), ANUM, nntp_edata_get(e)->article_num);
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
      mutt_debug(LL_DEBUG2, "mutt_hcache_store %s\n", buf);
      mutt_hcache_store(hc, buf, strlen(buf), e, 0);
    }
#endif
  }

#ifdef USE_HCACHE
  if (hc)
  {
    mutt_hcache_close(hc);
    mdata->last_cached = mdata->last_loaded;
  }
#endif

  /* save .newsrc entries */
  nntp_newsrc_gen_entries(m);
  nntp_newsrc_update(mdata->adata);
  nntp_newsrc_close(mdata->adata);
  return 0;
}

/**
 * nntp_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 * @retval 0 Always
 */
static int nntp_mbox_close(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  struct NntpMboxData *tmp_mdata = NULL;
  if (!mdata)
    return 0;

  mdata->unread = m->msg_unread;

  nntp_acache_free(mdata);
  if (!mdata->adata || !mdata->adata->groups_hash || !mdata->group)
    return 0;

  tmp_mdata = mutt_hash_find(mdata->adata->groups_hash, mdata->group);
  if (!tmp_mdata || (tmp_mdata != mdata))
    nntp_mdata_free((void **) &mdata);
  return 0;
}

/**
 * nntp_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static int nntp_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m || !m->emails || (msgno >= m->msg_count) || !msg)
    return -1;

  struct NntpMboxData *mdata = m->mdata;
  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  char article[16];

  /* try to get article from cache */
  struct NntpAcache *acache = &mdata->acache[e->index % NNTP_ACACHE_LEN];
  if (acache->path)
  {
    if (acache->index == e->index)
    {
      msg->fp = mutt_file_fopen(acache->path, "r");
      if (msg->fp)
        return 0;
    }
    /* clear previous entry */
    else
    {
      unlink(acache->path);
      FREE(&acache->path);
    }
  }
  snprintf(article, sizeof(article), ANUM, nntp_edata_get(e)->article_num);
  msg->fp = mutt_bcache_get(mdata->bcache, article);
  if (msg->fp)
  {
    if (nntp_edata_get(e)->parsed)
      return 0;
  }
  else
  {
    char buf[PATH_MAX];
    /* don't try to fetch article from removed newsgroup */
    if (mdata->deleted)
      return -1;

    /* create new cache file */
    const char *fetch_msg = _("Fetching message...");
    mutt_message(fetch_msg);
    msg->fp = mutt_bcache_put(mdata->bcache, article);
    if (!msg->fp)
    {
      mutt_mktemp(buf, sizeof(buf));
      acache->path = mutt_str_dup(buf);
      acache->index = e->index;
      msg->fp = mutt_file_fopen(acache->path, "w+");
      if (!msg->fp)
      {
        mutt_perror(acache->path);
        unlink(acache->path);
        FREE(&acache->path);
        return -1;
      }
    }

    /* fetch message to cache file */
    snprintf(buf, sizeof(buf), "ARTICLE %s\r\n",
             nntp_edata_get(e)->article_num ? article : e->env->message_id);
    const int rc =
        nntp_fetch_lines(mdata, buf, sizeof(buf), fetch_msg, fetch_tempfile, msg->fp);
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
          mutt_error("ARTICLE: %s", buf);
      }
      return -1;
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
  fseek(msg->fp, 0, SEEK_END);
  e->content->length = ftell(msg->fp) - e->content->offset;

  /* this is called in neomutt before the open which fetches the message,
   * which is probably wrong, but we just call it again here to handle
   * the problem instead of fixing it */
  nntp_edata_get(e)->parsed = true;
  mutt_parse_mime_message(m, e);

  /* these would normally be updated in ctx_update(), but the
   * full headers aren't parsed with overview, so the information wasn't
   * available then */
  if (WithCrypto)
    e->security = crypt_query(e->content);

  rewind(msg->fp);
  mutt_clear_error();
  return 0;
}

/**
 * nntp_msg_close - Close an email - Implements MxOps::msg_close()
 *
 * @note May also return EOF Failure, see errno
 */
static int nntp_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * nntp_path_probe - Is this an NNTP Mailbox? - Implements MxOps::path_probe()
 */
enum MailboxType nntp_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (mutt_istr_startswith(path, "news://"))
    return MUTT_NNTP;

  if (mutt_istr_startswith(path, "snews://"))
    return MUTT_NNTP;

  return MUTT_UNKNOWN;
}

/**
 * nntp_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon()
 */
static int nntp_path_canon(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  return 0;
}

/**
 * nntp_path_pretty - Abbreviate a Mailbox path - Implements MxOps::path_pretty()
 */
static int nntp_path_pretty(char *buf, size_t buflen, const char *folder)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

/**
 * nntp_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent()
 */
static int nntp_path_parent(char *buf, size_t buflen)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

// clang-format off
/**
 * MxNntpOps - NNTP Mailbox - Implements ::MxOps
 */
struct MxOps MxNntpOps = {
  .type            = MUTT_NNTP,
  .name             = "nntp",
  .is_local         = false,
  .ac_find          = nntp_ac_find,
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
  .path_pretty      = nntp_path_pretty,
  .path_parent      = nntp_path_parent,
};
// clang-format on
