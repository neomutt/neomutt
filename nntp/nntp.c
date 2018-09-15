/**
 * @file
 * Usenet network mailbox type; talk to an NNTP server
 *
 * @authors
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Andrej Gritsenko <andrej@lucky.net>
 * Copyright (C) 2000-2017 Vsevolod Volkov <vvv@mutt.org.ua>
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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "nntp_private.h"
#include "mutt/mutt.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "nntp.h"
#include "bcache.h"
#include "context.h"
#include "curs_lib.h"
#include "globals.h"
#include "mailbox.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_parse.h"
#include "mutt_socket.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif
#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

/* These Config Variables are only used in nntp/nntp.c */
char *NntpAuthenticators; ///< Config: (nntp) Allowed authentication methods
short NntpContext; ///< Config: (nntp) Maximum number of articles to list (0 for all articles)
bool NntpListgroup; ///< Config: (nntp) Check all articles when opening a newsgroup
bool NntpLoadDescription; ///< Config: (nntp) Load descriptions for newsgroups when adding to the list
short NntpPoll; ///< Config: (nntp) Interval between checks for new posts
bool ShowNewNews; ///< Config: (nntp) Check for new newsgroups when entering the browser

struct NntpServer *CurrentNewsSrv;

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
  struct Context *ctx;
  anum_t first;
  anum_t last;
  int restore;
  unsigned char *messages;
  struct Progress progress;
#ifdef USE_HCACHE
  header_cache_t *hc;
#endif
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
 * free_emaildata - Free data attached to an Email
 * @param data Email data
 */
static void free_emaildata(void **data)
{
  FREE(data);
}

/**
 * new_emaildata - Create a new NntpEmailData for an Email
 * @retval ptr New NntpEmailData struct
 */
static struct NntpEmailData *new_emaildata(void)
{
  return mutt_mem_calloc(1, sizeof(struct NntpEmailData));
}

/**
 * nntp_connect_error - Signal a failed connection
 * @param nserv NNTP server
 * @retval -1 Always
 */
static int nntp_connect_error(struct NntpServer *nserv)
{
  nserv->status = NNTP_NONE;
  mutt_error(_("Server closed connection"));
  return -1;
}

/**
 * nntp_capabilities - Get capabilities
 * @param nserv NNTP server
 * @retval -1 Error, connection is closed
 * @retval  0 Mode is reader, capabilities set up
 * @retval  1 Need to switch to reader mode
 */
static int nntp_capabilities(struct NntpServer *nserv)
{
  struct Connection *conn = nserv->conn;
  bool mode_reader = false;
  char buf[LONG_STRING];
  char authinfo[LONG_STRING] = "";

  nserv->hasCAPABILITIES = false;
  nserv->hasSTARTTLS = false;
  nserv->hasDATE = false;
  nserv->hasLIST_NEWSGROUPS = false;
  nserv->hasLISTGROUP = false;
  nserv->hasLISTGROUPrange = false;
  nserv->hasOVER = false;
  FREE(&nserv->authenticators);

  if (mutt_socket_send(conn, "CAPABILITIES\r\n") < 0 ||
      mutt_socket_readln(buf, sizeof(buf), conn) < 0)
  {
    return nntp_connect_error(nserv);
  }

  /* no capabilities */
  if (mutt_str_strncmp("101", buf, 3) != 0)
    return 1;
  nserv->hasCAPABILITIES = true;

  /* parse capabilities */
  do
  {
    if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
      return nntp_connect_error(nserv);
    if (mutt_str_strcmp("STARTTLS", buf) == 0)
      nserv->hasSTARTTLS = true;
    else if (mutt_str_strcmp("MODE-READER", buf) == 0)
      mode_reader = true;
    else if (mutt_str_strcmp("READER", buf) == 0)
    {
      nserv->hasDATE = true;
      nserv->hasLISTGROUP = true;
      nserv->hasLISTGROUPrange = true;
    }
    else if (mutt_str_strncmp("AUTHINFO ", buf, 9) == 0)
    {
      mutt_str_strcat(buf, sizeof(buf), " ");
      mutt_str_strfcpy(authinfo, buf + 8, sizeof(authinfo));
    }
#ifdef USE_SASL
    else if (mutt_str_strncmp("SASL ", buf, 5) == 0)
    {
      char *p = buf + 5;
      while (*p == ' ')
        p++;
      nserv->authenticators = mutt_str_strdup(p);
    }
#endif
    else if (mutt_str_strcmp("OVER", buf) == 0)
      nserv->hasOVER = true;
    else if (mutt_str_strncmp("LIST ", buf, 5) == 0)
    {
      char *p = strstr(buf, " NEWSGROUPS");
      if (p)
      {
        p += 11;
        if (*p == '\0' || *p == ' ')
          nserv->hasLIST_NEWSGROUPS = true;
      }
    }
  } while (mutt_str_strcmp(".", buf) != 0);
  *buf = '\0';
#ifdef USE_SASL
  if (nserv->authenticators && strcasestr(authinfo, " SASL "))
    mutt_str_strfcpy(buf, nserv->authenticators, sizeof(buf));
#endif
  if (strcasestr(authinfo, " USER "))
  {
    if (*buf)
      mutt_str_strcat(buf, sizeof(buf), " ");
    mutt_str_strcat(buf, sizeof(buf), "USER");
  }
  mutt_str_replace(&nserv->authenticators, buf);

  /* current mode is reader */
  if (nserv->hasDATE)
    return 0;

  /* server is mode-switching, need to switch to reader mode */
  if (mode_reader)
    return 1;

  mutt_socket_close(conn);
  nserv->status = NNTP_BYE;
  mutt_error(_("Server doesn't support reader mode"));
  return -1;
}

/**
 * nntp_attempt_features - Detect supported commands
 * @param nserv NNTP server
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_attempt_features(struct NntpServer *nserv)
{
  struct Connection *conn = nserv->conn;
  char buf[LONG_STRING];

  /* no CAPABILITIES, trying DATE, LISTGROUP, LIST NEWSGROUPS */
  if (!nserv->hasCAPABILITIES)
  {
    if (mutt_socket_send(conn, "DATE\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("500", buf, 3) != 0)
      nserv->hasDATE = true;

    if (mutt_socket_send(conn, "LISTGROUP\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("500", buf, 3) != 0)
      nserv->hasLISTGROUP = true;

    if (mutt_socket_send(conn, "LIST NEWSGROUPS +\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("500", buf, 3) != 0)
      nserv->hasLIST_NEWSGROUPS = true;
    if (mutt_str_strncmp("215", buf, 3) == 0)
    {
      do
      {
        if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
          return nntp_connect_error(nserv);
      } while (mutt_str_strcmp(".", buf) != 0);
    }
  }

  /* no LIST NEWSGROUPS, trying XGTITLE */
  if (!nserv->hasLIST_NEWSGROUPS)
  {
    if (mutt_socket_send(conn, "XGTITLE\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("500", buf, 3) != 0)
      nserv->hasXGTITLE = true;
  }

  /* no OVER, trying XOVER */
  if (!nserv->hasOVER)
  {
    if (mutt_socket_send(conn, "XOVER\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("500", buf, 3) != 0)
      nserv->hasXOVER = true;
  }

  /* trying LIST OVERVIEW.FMT */
  if (nserv->hasOVER || nserv->hasXOVER)
  {
    if (mutt_socket_send(conn, "LIST OVERVIEW.FMT\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("215", buf, 3) != 0)
      nserv->overview_fmt = mutt_str_strdup(OverviewFmt);
    else
    {
      int cont = 0;
      size_t buflen = 2 * LONG_STRING, off = 0, b = 0;

      if (nserv->overview_fmt)
        FREE(&nserv->overview_fmt);
      nserv->overview_fmt = mutt_mem_malloc(buflen);

      while (true)
      {
        if (buflen - off < LONG_STRING)
        {
          buflen *= 2;
          mutt_mem_realloc(&nserv->overview_fmt, buflen);
        }

        const int chunk =
            mutt_socket_readln(nserv->overview_fmt + off, buflen - off, conn);
        if (chunk < 0)
        {
          FREE(&nserv->overview_fmt);
          return nntp_connect_error(nserv);
        }

        if (!cont && (mutt_str_strcmp(".", nserv->overview_fmt + off) == 0))
          break;

        cont = chunk >= buflen - off ? 1 : 0;
        off += strlen(nserv->overview_fmt + off);
        if (!cont)
        {
          char *colon = NULL;

          if (nserv->overview_fmt[b] == ':')
          {
            memmove(nserv->overview_fmt + b, nserv->overview_fmt + b + 1, off - b - 1);
            nserv->overview_fmt[off - 1] = ':';
          }
          colon = strchr(nserv->overview_fmt + b, ':');
          if (!colon)
            nserv->overview_fmt[off++] = ':';
          else if (strcmp(colon + 1, "full") != 0)
            off = colon + 1 - nserv->overview_fmt;
          if (strcasecmp(nserv->overview_fmt + b, "Bytes:") == 0)
          {
            size_t len = strlen(nserv->overview_fmt + b);
            mutt_str_strfcpy(nserv->overview_fmt + b, "Content-Length:", len + 1);
            off = b + len;
          }
          nserv->overview_fmt[off++] = '\0';
          b = off;
        }
      }
      nserv->overview_fmt[off++] = '\0';
      mutt_mem_realloc(&nserv->overview_fmt, off);
    }
  }
  return 0;
}

/**
 * nntp_auth - Get login, password and authenticate
 * @param nserv NNTP server
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_auth(struct NntpServer *nserv)
{
  struct Connection *conn = nserv->conn;
  char buf[LONG_STRING];
  char authenticators[LONG_STRING] = "USER";
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
    if (NntpAuthenticators && *NntpAuthenticators)
      mutt_str_strfcpy(authenticators, NntpAuthenticators, sizeof(authenticators));
    else if (nserv->hasCAPABILITIES)
    {
      mutt_str_strfcpy(authenticators, nserv->authenticators, sizeof(authenticators));
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

    mutt_debug(1, "available methods: %s\n", nserv->authenticators);
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
      if (nserv->hasCAPABILITIES)
      {
        char *m = NULL;

        if (!nserv->authenticators)
          continue;
        m = strcasestr(nserv->authenticators, method);
        if (!m)
          continue;
        if (m > nserv->authenticators && *(m - 1) != ' ')
          continue;
        m += strlen(method);
        if (*m != '\0' && *m != ' ')
          continue;
      }
      mutt_debug(1, "trying method %s\n", method);

      /* AUTHINFO USER authentication */
      if (strcmp(method, "USER") == 0)
      {
        mutt_message(_("Authenticating (%s)..."), method);
        snprintf(buf, sizeof(buf), "AUTHINFO USER %s\r\n", conn->account.user);
        if (mutt_socket_send(conn, buf) < 0 ||
            mutt_socket_readln(buf, sizeof(buf), conn) < 0)
        {
          break;
        }

        /* authenticated, password is not required */
        if (mutt_str_strncmp("281", buf, 3) == 0)
          return 0;

        /* username accepted, sending password */
        if (mutt_str_strncmp("381", buf, 3) == 0)
        {
          if (DebugLevel < MUTT_SOCK_LOG_FULL)
            mutt_debug(MUTT_SOCK_LOG_CMD, "%d> AUTHINFO PASS *\n", conn->fd);
          snprintf(buf, sizeof(buf), "AUTHINFO PASS %s\r\n", conn->account.pass);
          if (mutt_socket_send_d(conn, buf, MUTT_SOCK_LOG_FULL) < 0 ||
              mutt_socket_readln(buf, sizeof(buf), conn) < 0)
          {
            break;
          }

          /* authenticated */
          if (mutt_str_strncmp("281", buf, 3) == 0)
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
        char inbuf[LONG_STRING] = "";
        const char *mech = NULL;
        const char *client_out = NULL;
        unsigned int client_len, len;

        if (mutt_sasl_client_new(conn, &saslconn) < 0)
        {
          mutt_debug(1, "error allocating SASL connection.\n");
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
        if (rc != SASL_OK && rc != SASL_CONTINUE)
        {
          sasl_dispose(&saslconn);
          mutt_debug(1, "error starting SASL authentication exchange.\n");
          continue;
        }

        mutt_message(_("Authenticating (%s)..."), method);
        snprintf(buf, sizeof(buf), "AUTHINFO SASL %s", method);

        /* looping protocol */
        while (rc == SASL_CONTINUE || (rc == SASL_OK && client_len))
        {
          /* send out client response */
          if (client_len)
          {
            if (DebugLevel >= MUTT_SOCK_LOG_FULL)
            {
              char tmp[LONG_STRING];
              memcpy(tmp, client_out, client_len);
              for (p = tmp; p < tmp + client_len; p++)
              {
                if (*p == '\0')
                  *p = '.';
              }
              *p = '\0';
              mutt_debug(1, "SASL> %s\n", tmp);
            }

            if (*buf)
              mutt_str_strcat(buf, sizeof(buf), " ");
            len = strlen(buf);
            if (sasl_encode64(client_out, client_len, buf + len,
                              sizeof(buf) - len, &len) != SASL_OK)
            {
              mutt_debug(1, "error base64-encoding client response.\n");
              break;
            }
          }

          mutt_str_strcat(buf, sizeof(buf), "\r\n");
          if (DebugLevel < MUTT_SOCK_LOG_FULL)
          {
            if (strchr(buf, ' '))
            {
              mutt_debug(MUTT_SOCK_LOG_CMD, "%d> AUTHINFO SASL %s%s\n",
                         conn->fd, method, client_len ? " sasl_data" : "");
            }
            else
              mutt_debug(MUTT_SOCK_LOG_CMD, "%d> sasl_data\n", conn->fd);
          }
          client_len = 0;
          if (mutt_socket_send_d(conn, buf, MUTT_SOCK_LOG_FULL) < 0 ||
              mutt_socket_readln_d(inbuf, sizeof(inbuf), conn, MUTT_SOCK_LOG_FULL) < 0)
          {
            break;
          }
          if ((mutt_str_strncmp(inbuf, "283 ", 4) != 0) &&
              (mutt_str_strncmp(inbuf, "383 ", 4) != 0))
          {
            if (DebugLevel < MUTT_SOCK_LOG_FULL)
              mutt_debug(MUTT_SOCK_LOG_CMD, "%d< %s\n", conn->fd, inbuf);
            break;
          }
          if (DebugLevel < MUTT_SOCK_LOG_FULL)
          {
            inbuf[3] = '\0';
            mutt_debug(MUTT_SOCK_LOG_CMD, "%d< %s sasl_data\n", conn->fd, inbuf);
          }

          if (strcmp("=", inbuf + 4) == 0)
            len = 0;
          else if (sasl_decode64(inbuf + 4, strlen(inbuf + 4), buf,
                                 sizeof(buf) - 1, &len) != SASL_OK)
          {
            mutt_debug(1, "error base64-decoding server response.\n");
            break;
          }
          else if (DebugLevel >= MUTT_SOCK_LOG_FULL)
          {
            char tmp[LONG_STRING];
            memcpy(tmp, buf, len);
            for (p = tmp; p < tmp + len; p++)
            {
              if (*p == '\0')
                *p = '.';
            }
            *p = '\0';
            mutt_debug(1, "SASL< %s\n", tmp);
          }

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

        if (rc == SASL_OK && client_len == 0 && *inbuf == '2')
        {
          mutt_sasl_setup_conn(conn, saslconn);
          return 0;
        }

        /* terminate SASL session */
        sasl_dispose(&saslconn);
        if (conn->fd < 0)
          break;
        if (mutt_str_strncmp(inbuf, "383 ", 4) == 0)
        {
          if (mutt_socket_send(conn, "*\r\n") < 0 ||
              mutt_socket_readln(inbuf, sizeof(inbuf), conn) < 0)
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
  nserv->status = NNTP_BYE;
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
  struct NntpServer *nserv = mdata->nserv;
  char buf[LONG_STRING] = { 0 };

  if (nserv->status == NNTP_BYE)
    return -1;

  while (true)
  {
    if (nserv->status == NNTP_OK)
    {
      int rc = 0;

      if (*line)
        rc = mutt_socket_send(nserv->conn, line);
      else if (mdata->group)
      {
        snprintf(buf, sizeof(buf), "GROUP %s\r\n", mdata->group);
        rc = mutt_socket_send(nserv->conn, buf);
      }
      if (rc >= 0)
        rc = mutt_socket_readln(buf, sizeof(buf), nserv->conn);
      if (rc >= 0)
        break;
    }

    /* reconnect */
    while (true)
    {
      nserv->status = NNTP_NONE;
      if (nntp_open_connection(nserv) == 0)
        break;

      snprintf(buf, sizeof(buf), _("Connection to %s lost. Reconnect?"),
               nserv->conn->account.host);
      if (mutt_yesorno(buf, MUTT_YES) != MUTT_YES)
      {
        nserv->status = NNTP_BYE;
        return -1;
      }
    }

    /* select newsgroup after reconnection */
    if (mdata->group)
    {
      snprintf(buf, sizeof(buf), "GROUP %s\r\n", mdata->group);
      if (mutt_socket_send(nserv->conn, buf) < 0 ||
          mutt_socket_readln(buf, sizeof(buf), nserv->conn) < 0)
      {
        return nntp_connect_error(nserv);
      }
    }
    if (!*line)
      break;
  }

  mutt_str_strfcpy(line, buf, linelen);
  return 0;
}

/**
 * nntp_fetch_lines - Read lines, calling a callback function for each
 * @param mdata NNTP Mailbox data
 * @param query     Query to match
 * @param qlen      Length of query
 * @param msg       Progess message (OPTIONAL)
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
  int done = false;
  int rc;

  while (!done)
  {
    char buf[LONG_STRING];
    char *line = NULL;
    unsigned int lines = 0;
    size_t off = 0;
    struct Progress progress;

    if (msg)
      mutt_progress_init(&progress, msg, MUTT_PROGRESS_MSG, ReadInc, 0);

    mutt_str_strfcpy(buf, query, sizeof(buf));
    if (nntp_query(mdata, buf, sizeof(buf)) < 0)
      return -1;
    if (buf[0] != '2')
    {
      mutt_str_strfcpy(query, buf, qlen);
      return 1;
    }

    line = mutt_mem_malloc(sizeof(buf));
    rc = 0;

    while (true)
    {
      char *p = NULL;
      int chunk = mutt_socket_readln_d(buf, sizeof(buf), mdata->nserv->conn, MUTT_SOCK_LOG_HDR);
      if (chunk < 0)
      {
        mdata->nserv->status = NNTP_NONE;
        break;
      }

      p = buf;
      if (!off && buf[0] == '.')
      {
        if (buf[1] == '\0')
        {
          done = true;
          break;
        }
        if (buf[1] == '.')
          p++;
      }

      mutt_str_strfcpy(line + off, p, sizeof(buf));

      if (chunk >= sizeof(buf))
        off += strlen(p);
      else
      {
        if (msg)
          mutt_progress_update(&progress, ++lines, -1);

        if (rc == 0 && func(line, data) < 0)
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
  struct NntpServer *nserv = data;
  struct NntpMboxData *mdata = NULL;
  char *desc = NULL;

  if (!line)
    return 0;

  desc = strpbrk(line, " \t");
  if (desc)
  {
    *desc++ = '\0';
    desc += strspn(desc, " \t");
  }
  else
    desc = strchr(line, '\0');

  mdata = mutt_hash_find(nserv->groups_hash, line);
  if (mdata && (mutt_str_strcmp(desc, mdata->desc) != 0))
  {
    mutt_str_replace(&mdata->desc, desc);
    mutt_debug(2, "group: %s, desc: %s\n", line, desc);
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
  char buf[STRING];
  const char *cmd = NULL;

  /* get newsgroup description, if possible */
  struct NntpServer *nserv = mdata->nserv;
  if (!wildmat)
    wildmat = mdata->group;
  if (nserv->hasLIST_NEWSGROUPS)
    cmd = "LIST NEWSGROUPS";
  else if (nserv->hasXGTITLE)
    cmd = "XGTITLE";
  else
    return 0;

  snprintf(buf, sizeof(buf), "%s %s\r\n", cmd, wildmat);
  int rc = nntp_fetch_lines(mdata, buf, sizeof(buf), msg, fetch_description, nserv);
  if (rc > 0)
  {
    mutt_error("%s: %s", cmd, buf);
  }
  return rc;
}

/**
 * nntp_parse_xref - Parse cross-reference
 * @param mailbox Mailbox
 * @param e       Email
 *
 * Update read flag and set article number if empty
 */
static void nntp_parse_xref(struct Mailbox *mailbox, struct Email *e)
{
  struct NntpMboxData *mdata = mailbox->data;

  char *buf = mutt_str_strdup(e->env->xref);
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

    nntp_article_status(mailbox, e, grp, anum);
    if (!NNTP_EDATA(e)->article_num && (mutt_str_strcmp(mdata->group, grp) == 0))
      NNTP_EDATA(e)->article_num = anum;
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
  else if (fputs(line, fp) == EOF || fputc('\n', fp) == EOF)
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
  if (anum < fc->first || anum > fc->last)
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
  struct FetchCtx *fc = data;
  struct Context *ctx = fc->ctx;
  struct NntpMboxData *mdata = ctx->mailbox->data;
  struct Email *e = NULL;
  char *header = NULL, *field = NULL;
  bool save = true;
  anum_t anum;

  if (!line)
    return 0;

  /* parse article number */
  field = strchr(line, '\t');
  if (field)
    *field++ = '\0';
  if (sscanf(line, ANUM, &anum) != 1)
    return 0;
  mutt_debug(2, "" ANUM "\n", anum);

  /* out of bounds */
  if (anum < fc->first || anum > fc->last)
    return 0;

  /* not in LISTGROUP */
  if (!fc->messages[anum - fc->first])
  {
    /* progress */
    if (!ctx->mailbox->quiet)
      mutt_progress_update(&fc->progress, anum - fc->first + 1, -1);
    return 0;
  }

  /* convert overview line to header */
  FILE *fp = mutt_file_mkstemp();
  if (!fp)
    return -1;

  header = mdata->nserv->overview_fmt;
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
    if (fputs(b, fp) == EOF || fputc('\n', fp) == EOF)
    {
      mutt_file_fclose(&fp);
      return -1;
    }
  }
  rewind(fp);

  /* allocate memory for headers */
  if (ctx->mailbox->msg_count >= ctx->mailbox->hdrmax)
    mx_alloc_memory(ctx->mailbox);

  /* parse header */
  ctx->mailbox->hdrs[ctx->mailbox->msg_count] = mutt_email_new();
  e = ctx->mailbox->hdrs[ctx->mailbox->msg_count];
  e->env = mutt_rfc822_read_header(fp, e, false, false);
  e->env->newsgroups = mutt_str_strdup(mdata->group);
  e->received = e->date_sent;
  mutt_file_fclose(&fp);

#ifdef USE_HCACHE
  if (fc->hc)
  {
    char buf[16];

    /* try to replace with header from cache */
    snprintf(buf, sizeof(buf), "%u", anum);
    void *hdata = mutt_hcache_fetch(fc->hc, buf, strlen(buf));
    if (hdata)
    {
      mutt_debug(2, "mutt_hcache_fetch %s\n", buf);
      mutt_email_free(&e);
      e = mutt_hcache_restore(hdata);
      ctx->mailbox->hdrs[ctx->mailbox->msg_count] = e;
      mutt_hcache_free(fc->hc, &hdata);
      e->data = NULL;
      e->read = false;
      e->old = false;

      /* skip header marked as deleted in cache */
      if (e->deleted && !fc->restore)
      {
        if (mdata->bcache)
        {
          mutt_debug(2, "mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }
        save = false;
      }
    }

    /* not cached yet, store header */
    else
    {
      mutt_debug(2, "mutt_hcache_store %s\n", buf);
      mutt_hcache_store(fc->hc, buf, strlen(buf), e, 0);
    }
  }
#endif

  if (save)
  {
    e->index = ctx->mailbox->msg_count++;
    e->read = false;
    e->old = false;
    e->deleted = false;
    e->data = new_emaildata();
    e->free_data = free_emaildata;
    NNTP_EDATA(e)->article_num = anum;
    if (fc->restore)
      e->changed = true;
    else
    {
      nntp_article_status(ctx->mailbox, e, NULL, anum);
      if (!e->read)
        nntp_parse_xref(ctx->mailbox, e);
    }
    if (anum > mdata->last_loaded)
      mdata->last_loaded = anum;
  }
  else
    mutt_email_free(&e);

  /* progress */
  if (!ctx->mailbox->quiet)
    mutt_progress_update(&fc->progress, anum - fc->first + 1, -1);
  return 0;
}

/**
 * nntp_fetch_headers - Fetch headers
 * @param ctx     Mailbox
 * @param hc      Header cache
 * @param first   Number of first header to fetch
 * @param last    Number of last header to fetch
 * @param restore Restore message listed as deleted
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_fetch_headers(struct Context *ctx, void *hc, anum_t first,
                              anum_t last, int restore)
{
  struct NntpMboxData *mdata = ctx->mailbox->data;
  struct FetchCtx fc;
  struct Email *e = NULL;
  char buf[HUGE_STRING];
  int rc = 0;
  int oldmsgcount = ctx->mailbox->msg_count;
  anum_t current;
  anum_t first_over = first;
#ifdef USE_HCACHE
  void *hdata = NULL;
#endif

  /* if empty group or nothing to do */
  if (!last || first > last)
    return 0;

  /* init fetch context */
  fc.ctx = ctx;
  fc.first = first;
  fc.last = last;
  fc.restore = restore;
  fc.messages = mutt_mem_calloc(last - first + 1, sizeof(unsigned char));
  if (!fc.messages)
    return -1;
#ifdef USE_HCACHE
  fc.hc = hc;
#endif

  /* fetch list of articles */
  if (NntpListgroup && mdata->nserv->hasLISTGROUP && !mdata->deleted)
  {
    if (!ctx->mailbox->quiet)
      mutt_message(_("Fetching list of articles..."));
    if (mdata->nserv->hasLISTGROUPrange)
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
      for (current = first; current <= last && rc == 0; current++)
      {
        if (fc.messages[current - first])
          continue;

        snprintf(buf, sizeof(buf), "%u", current);
        if (mdata->bcache)
        {
          mutt_debug(2, "#1 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }

#ifdef USE_HCACHE
        if (fc.hc)
        {
          mutt_debug(2, "mutt_hcache_delete %s\n", buf);
          mutt_hcache_delete(fc.hc, buf, strlen(buf));
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
  if (!ctx->mailbox->quiet)
  {
    mutt_progress_init(&fc.progress, _("Fetching message headers..."),
                       MUTT_PROGRESS_MSG, ReadInc, last - first + 1);
  }
  for (current = first; current <= last && rc == 0; current++)
  {
    if (!ctx->mailbox->quiet)
      mutt_progress_update(&fc.progress, current - first + 1, -1);

#ifdef USE_HCACHE
    snprintf(buf, sizeof(buf), "%u", current);
#endif

    /* delete header from cache that does not exist on server */
    if (!fc.messages[current - first])
      continue;

    /* allocate memory for headers */
    if (ctx->mailbox->msg_count >= ctx->mailbox->hdrmax)
      mx_alloc_memory(ctx->mailbox);

#ifdef USE_HCACHE
    /* try to fetch header from cache */
    hdata = mutt_hcache_fetch(fc.hc, buf, strlen(buf));
    if (hdata)
    {
      mutt_debug(2, "mutt_hcache_fetch %s\n", buf);
      e = mutt_hcache_restore(hdata);
      ctx->mailbox->hdrs[ctx->mailbox->msg_count] = e;
      mutt_hcache_free(fc.hc, &hdata);
      e->data = NULL;

      /* skip header marked as deleted in cache */
      if (e->deleted && !restore)
      {
        mutt_email_free(&e);
        if (mdata->bcache)
        {
          mutt_debug(2, "#2 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }
        continue;
      }

      e->read = false;
      e->old = false;
    }
    else
#endif

        /* don't try to fetch header from removed newsgroup */
        if (mdata->deleted)
      continue;

    /* fallback to fetch overview */
    else if (mdata->nserv->hasOVER || mdata->nserv->hasXOVER)
    {
      if (NntpListgroup && mdata->nserv->hasLISTGROUP)
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
        if (mutt_str_strncmp("423", buf, 3) != 0)
        {
          mutt_error("HEAD: %s", buf);
          break;
        }

        /* no such article */
        if (mdata->bcache)
        {
          snprintf(buf, sizeof(buf), "%u", current);
          mutt_debug(2, "#3 mutt_bcache_del %s\n", buf);
          mutt_bcache_del(mdata->bcache, buf);
        }
        rc = 0;
        continue;
      }

      /* parse header */
      ctx->mailbox->hdrs[ctx->mailbox->msg_count] = mutt_email_new();
      e = ctx->mailbox->hdrs[ctx->mailbox->msg_count];
      e->env = mutt_rfc822_read_header(fp, e, false, false);
      e->received = e->date_sent;
      mutt_file_fclose(&fp);
    }

    /* save header in context */
    e->index = ctx->mailbox->msg_count++;
    e->read = false;
    e->old = false;
    e->deleted = false;
    e->data = new_emaildata();
    e->free_data = free_emaildata;
    NNTP_EDATA(e)->article_num = current;
    if (restore)
      e->changed = true;
    else
    {
      nntp_article_status(ctx->mailbox, e, NULL, NNTP_EDATA(e)->article_num);
      if (!e->read)
        nntp_parse_xref(ctx->mailbox, e);
    }
    if (current > mdata->last_loaded)
      mdata->last_loaded = current;
    first_over = current + 1;
  }

  if (!NntpListgroup || !mdata->nserv->hasLISTGROUP)
    current = first_over;

  /* fetch overview information */
  if (current <= last && rc == 0 && !mdata->deleted)
  {
    char *cmd = mdata->nserv->hasOVER ? "OVER" : "XOVER";
    snprintf(buf, sizeof(buf), "%s %u-%u\r\n", cmd, current, last);
    rc = nntp_fetch_lines(mdata, buf, sizeof(buf), NULL, parse_overview_line, &fc);
    if (rc > 0)
    {
      mutt_error("%s: %s", cmd, buf);
    }
  }

  if (ctx->mailbox->msg_count > oldmsgcount)
    mx_update_context(ctx, ctx->mailbox->msg_count - oldmsgcount);

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
static int nntp_group_poll(struct NntpMboxData *mdata, int update_stat)
{
  char buf[LONG_STRING] = "";
  anum_t count, first, last;

  /* use GROUP command to poll newsgroup */
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
    return -1;
  if (sscanf(buf, "211 " ANUM " " ANUM " " ANUM, &count, &first, &last) != 3)
    return 0;
  if (first == mdata->first_message && last == mdata->last_message)
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
 * @param ctx Mailbox
 * @retval #MUTT_REOPENED Articles have been renumbered or removed from server
 * @retval #MUTT_NEW_MAIL New articles found
 * @retval  0             No change
 * @retval -1             Lost connection
 *
 * Leave newsrc locked
 */
static int check_mailbox(struct Context *ctx)
{
  struct NntpMboxData *mdata = ctx->mailbox->data;
  struct NntpServer *nserv = mdata->nserv;
  time_t now = time(NULL);
  int rc, ret = 0;
  void *hc = NULL;

  if (nserv->check_time + NntpPoll > now)
    return 0;

  mutt_message(_("Checking for new messages..."));
  if (nntp_newsrc_parse(nserv) < 0)
    return -1;

  nserv->check_time = now;
  rc = nntp_group_poll(mdata, 0);
  if (rc < 0)
  {
    nntp_newsrc_close(nserv);
    return -1;
  }
  if (rc)
    nntp_active_save_cache(nserv);

  /* articles have been renumbered, remove all headers */
  if (mdata->last_message < mdata->last_loaded)
  {
    for (int i = 0; i < ctx->mailbox->msg_count; i++)
      mutt_email_free(&ctx->mailbox->hdrs[i]);
    ctx->mailbox->msg_count = 0;
    ctx->tagged = 0;

    if (mdata->last_message < mdata->last_loaded)
    {
      mdata->last_loaded = mdata->first_message - 1;
      if (NntpContext && mdata->last_message - mdata->last_loaded > NntpContext)
        mdata->last_loaded = mdata->last_message - NntpContext;
    }
    ret = MUTT_REOPENED;
  }

  /* .newsrc has been externally modified */
  if (nserv->newsrc_modified)
  {
#ifdef USE_HCACHE
    unsigned char *messages = NULL;
    char buf[16];
    void *hdata = NULL;
    struct Email *e = NULL;
    anum_t first = mdata->first_message;

    if (NntpContext && mdata->last_message - first + 1 > NntpContext)
      first = mdata->last_message - NntpContext + 1;
    messages = mutt_mem_calloc(mdata->last_loaded - first + 1, sizeof(unsigned char));
    hc = nntp_hcache_open(mdata);
    nntp_hcache_update(mdata, hc);
#endif

    /* update flags according to .newsrc */
    int j = 0;
    anum_t anum;
    for (int i = 0; i < ctx->mailbox->msg_count; i++)
    {
      bool flagged = false;
      anum = NNTP_EDATA(ctx->mailbox->hdrs[i])->article_num;

#ifdef USE_HCACHE
      /* check hcache for flagged and deleted flags */
      if (hc)
      {
        if (anum >= first && anum <= mdata->last_loaded)
          messages[anum - first] = 1;

        snprintf(buf, sizeof(buf), "%u", anum);
        hdata = mutt_hcache_fetch(hc, buf, strlen(buf));
        if (hdata)
        {
          bool deleted;

          mutt_debug(2, "#1 mutt_hcache_fetch %s\n", buf);
          e = mutt_hcache_restore(hdata);
          mutt_hcache_free(hc, &hdata);
          e->data = NULL;
          deleted = e->deleted;
          flagged = e->flagged;
          mutt_email_free(&e);

          /* header marked as deleted, removing from context */
          if (deleted)
          {
            mutt_set_flag(ctx, ctx->mailbox->hdrs[i], MUTT_TAG, 0);
            mutt_email_free(&ctx->mailbox->hdrs[i]);
            continue;
          }
        }
      }
#endif

      if (!ctx->mailbox->hdrs[i]->changed)
      {
        ctx->mailbox->hdrs[i]->flagged = flagged;
        ctx->mailbox->hdrs[i]->read = false;
        ctx->mailbox->hdrs[i]->old = false;
        nntp_article_status(ctx->mailbox, ctx->mailbox->hdrs[i], NULL, anum);
        if (!ctx->mailbox->hdrs[i]->read)
          nntp_parse_xref(ctx->mailbox, ctx->mailbox->hdrs[i]);
      }
      ctx->mailbox->hdrs[j++] = ctx->mailbox->hdrs[i];
    }

#ifdef USE_HCACHE
    ctx->mailbox->msg_count = j;

    /* restore headers without "deleted" flag */
    for (anum = first; anum <= mdata->last_loaded; anum++)
    {
      if (messages[anum - first])
        continue;

      snprintf(buf, sizeof(buf), "%u", anum);
      hdata = mutt_hcache_fetch(hc, buf, strlen(buf));
      if (hdata)
      {
        mutt_debug(2, "#2 mutt_hcache_fetch %s\n", buf);
        if (ctx->mailbox->msg_count >= ctx->mailbox->hdrmax)
          mx_alloc_memory(ctx->mailbox);

        e = mutt_hcache_restore(hdata);
        ctx->mailbox->hdrs[ctx->mailbox->msg_count] = e;
        mutt_hcache_free(hc, &hdata);
        e->data = NULL;
        if (e->deleted)
        {
          mutt_email_free(&e);
          if (mdata->bcache)
          {
            mutt_debug(2, "mutt_bcache_del %s\n", buf);
            mutt_bcache_del(mdata->bcache, buf);
          }
          continue;
        }

        ctx->mailbox->msg_count++;
        e->read = false;
        e->old = false;
        e->data = new_emaildata();
        e->free_data = free_emaildata;
        NNTP_EDATA(e)->article_num = anum;
        nntp_article_status(ctx->mailbox, e, NULL, anum);
        if (!e->read)
          nntp_parse_xref(ctx->mailbox, e);
      }
    }
    FREE(&messages);
#endif

    nserv->newsrc_modified = false;
    ret = MUTT_REOPENED;
  }

  /* some headers were removed, context must be updated */
  if (ret == MUTT_REOPENED)
  {
    if (ctx->mailbox->subj_hash)
      mutt_hash_destroy(&ctx->mailbox->subj_hash);
    if (ctx->mailbox->id_hash)
      mutt_hash_destroy(&ctx->mailbox->id_hash);
    mutt_clear_threads(ctx);

    ctx->mailbox->vcount = 0;
    ctx->deleted = 0;
    ctx->new = 0;
    ctx->mailbox->msg_unread = 0;
    ctx->mailbox->msg_flagged = 0;
    ctx->mailbox->changed = false;
    ctx->mailbox->id_hash = NULL;
    ctx->mailbox->subj_hash = NULL;
    mx_update_context(ctx, ctx->mailbox->msg_count);
  }

  /* fetch headers of new articles */
  if (mdata->last_message > mdata->last_loaded)
  {
    int oldmsgcount = ctx->mailbox->msg_count;
    bool quiet = ctx->mailbox->quiet;
    ctx->mailbox->quiet = true;
#ifdef USE_HCACHE
    if (!hc)
    {
      hc = nntp_hcache_open(mdata);
      nntp_hcache_update(mdata, hc);
    }
#endif
    rc = nntp_fetch_headers(ctx, hc, mdata->last_loaded + 1, mdata->last_message, 0);
    ctx->mailbox->quiet = quiet;
    if (rc >= 0)
      mdata->last_loaded = mdata->last_message;
    if (ret == 0 && ctx->mailbox->msg_count > oldmsgcount)
      ret = MUTT_NEW_MAIL;
  }

#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif
  if (ret)
    nntp_newsrc_close(nserv);
  mutt_clear_error();
  return ret;
}

/**
 * nntp_date - Get date and time from server
 * @param nserv NNTP server
 * @param now   Server time
 * @retval  0 Success
 * @retval -1 Failure
 */
static int nntp_date(struct NntpServer *nserv, time_t *now)
{
  if (nserv->hasDATE)
  {
    struct NntpMboxData mdata;
    char buf[LONG_STRING];
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    mdata.nserv = nserv;
    mdata.group = NULL;
    mutt_str_strfcpy(buf, "DATE\r\n", sizeof(buf));
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
        mutt_debug(1, "server time is %lu\n", *now);
        return 0;
      }
    }
  }
  time(now);
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

  if (!line || sscanf(line, ANUM, &anum) != 1)
    return 0;
  for (unsigned int i = 0; i < cc->mailbox->msg_count; i++)
    if (NNTP_EDATA(cc->mailbox->hdrs[i])->article_num == anum)
      return 0;
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
 * @param nserv NNTP server
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_open_connection(struct NntpServer *nserv)
{
  struct Connection *conn = nserv->conn;
  char buf[STRING];
  int cap;
  bool posting = false, auth = true;

  if (nserv->status == NNTP_OK)
    return 0;
  if (nserv->status == NNTP_BYE)
    return -1;
  nserv->status = NNTP_NONE;

  if (mutt_socket_open(conn) < 0)
    return -1;

  if (mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    return nntp_connect_error(nserv);

  if (mutt_str_strncmp("200", buf, 3) == 0)
    posting = true;
  else if (mutt_str_strncmp("201", buf, 3) != 0)
  {
    mutt_socket_close(conn);
    mutt_str_remove_trailing_ws(buf);
    mutt_error("%s", buf);
    return -1;
  }

  /* get initial capabilities */
  cap = nntp_capabilities(nserv);
  if (cap < 0)
    return -1;

  /* tell news server to switch to mode reader if it isn't so */
  if (cap > 0)
  {
    if (mutt_socket_send(conn, "MODE READER\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }

    if (mutt_str_strncmp("200", buf, 3) == 0)
      posting = true;
    else if (mutt_str_strncmp("201", buf, 3) == 0)
      posting = false;
    /* error if has capabilities, ignore result if no capabilities */
    else if (nserv->hasCAPABILITIES)
    {
      mutt_socket_close(conn);
      mutt_error(_("Could not switch to reader mode"));
      return -1;
    }

    /* recheck capabilities after MODE READER */
    if (nserv->hasCAPABILITIES)
    {
      cap = nntp_capabilities(nserv);
      if (cap < 0)
        return -1;
    }
  }

  mutt_message(_("Connected to %s. %s"), conn->account.host,
               posting ? _("Posting is ok") : _("Posting is NOT ok"));
  mutt_sleep(1);

#ifdef USE_SSL
  /* Attempt STARTTLS if available and desired. */
  if (nserv->use_tls != 1 && (nserv->hasSTARTTLS || SslForceTls))
  {
    if (nserv->use_tls == 0)
    {
      nserv->use_tls =
          SslForceTls || query_quadoption(SslStarttls,
                                          _("Secure connection with TLS?")) == MUTT_YES ?
              2 :
              1;
    }
    if (nserv->use_tls == 2)
    {
      if (mutt_socket_send(conn, "STARTTLS\r\n") < 0 ||
          mutt_socket_readln(buf, sizeof(buf), conn) < 0)
      {
        return nntp_connect_error(nserv);
      }
      if (mutt_str_strncmp("382", buf, 3) != 0)
      {
        nserv->use_tls = 0;
        mutt_error("STARTTLS: %s", buf);
      }
      else if (mutt_ssl_starttls(conn))
      {
        nserv->use_tls = 0;
        nserv->status = NNTP_NONE;
        mutt_socket_close(nserv->conn);
        mutt_error(_("Could not negotiate TLS connection"));
        return -1;
      }
      else
      {
        /* recheck capabilities after STARTTLS */
        cap = nntp_capabilities(nserv);
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
    if (mutt_socket_send(conn, "STAT\r\n") < 0 ||
        mutt_socket_readln(buf, sizeof(buf), conn) < 0)
    {
      return nntp_connect_error(nserv);
    }
    if (mutt_str_strncmp("480", buf, 3) != 0)
      auth = false;
  }

  /* authenticate */
  if (auth && nntp_auth(nserv) < 0)
    return -1;

  /* get final capabilities after authentication */
  if (nserv->hasCAPABILITIES && (auth || cap > 0))
  {
    cap = nntp_capabilities(nserv);
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
  if (nntp_attempt_features(nserv) < 0)
    return -1;

  nserv->status = NNTP_OK;
  return 0;
}

/**
 * nntp_post - Post article
 * @param mailbox Mailbox
 * @param msg     Message to post
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_post(struct Mailbox *mailbox, const char *msg)
{
  struct NntpMboxData *mdata = NULL;
  struct NntpMboxData nntp_tmp = { 0 };
  char buf[LONG_STRING];

  if (mailbox && (mailbox->magic == MUTT_NNTP))
    mdata = mailbox->data;
  else
  {
    CurrentNewsSrv = nntp_select_server(mailbox, NewsServer, false);
    if (!CurrentNewsSrv)
      return -1;

    mdata = &nntp_tmp;
    mdata->nserv = CurrentNewsSrv;
    mdata->group = NULL;
  }

  FILE *fp = mutt_file_fopen(msg, "r");
  if (!fp)
  {
    mutt_perror(msg);
    return -1;
  }

  mutt_str_strfcpy(buf, "POST\r\n", sizeof(buf));
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
    if (mutt_socket_send_d(mdata->nserv->conn, buf[1] == '.' ? buf : buf + 1,
                           MUTT_SOCK_LOG_HDR) < 0)
    {
      mutt_file_fclose(&fp);
      return nntp_connect_error(mdata->nserv);
    }
  }
  mutt_file_fclose(&fp);

  if ((buf[strlen(buf) - 1] != '\n' &&
       mutt_socket_send_d(mdata->nserv->conn, "\r\n", MUTT_SOCK_LOG_HDR) < 0) ||
      mutt_socket_send_d(mdata->nserv->conn, ".\r\n", MUTT_SOCK_LOG_HDR) < 0 ||
      mutt_socket_readln(buf, sizeof(buf), mdata->nserv->conn) < 0)
  {
    return nntp_connect_error(mdata->nserv);
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
 * @param nserv NNTP server
 * @param new   Mark the groups as new
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_active_fetch(struct NntpServer *nserv, bool new)
{
  struct NntpMboxData mdata;
  char msg[STRING];
  char buf[LONG_STRING];
  unsigned int i;
  int rc;

  snprintf(msg, sizeof(msg), _("Loading list of groups from server %s..."),
           nserv->conn->account.host);
  mutt_message(msg);
  if (nntp_date(nserv, &nserv->newgroups_time) < 0)
    return -1;

  mdata.nserv = nserv;
  mdata.group = NULL;
  i = nserv->groups_num;
  mutt_str_strfcpy(buf, "LIST\r\n", sizeof(buf));
  rc = nntp_fetch_lines(&mdata, buf, sizeof(buf), msg, nntp_add_group, nserv);
  if (rc)
  {
    if (rc > 0)
    {
      mutt_error("LIST: %s", buf);
    }
    return -1;
  }

  if (new)
  {
    for (; i < nserv->groups_num; i++)
    {
      struct NntpMboxData *data = nserv->groups_list[i];
      data->new = true;
    }
  }

  for (i = 0; i < nserv->groups_num; i++)
  {
    struct NntpMboxData *data = nserv->groups_list[i];

    if (data && data->deleted && !data->newsrc_ent)
    {
      nntp_delete_group_cache(data);
      mutt_hash_delete(nserv->groups_hash, data->group, NULL);
      nserv->groups_list[i] = NULL;
    }
  }

  if (NntpLoadDescription)
    rc = get_description(&mdata, "*", _("Loading descriptions..."));

  nntp_active_save_cache(nserv);
  if (rc < 0)
    return -1;
  mutt_clear_error();
  return 0;
}

/**
 * nntp_check_new_groups - Check for new groups/articles in subscribed groups
 * @param mailbox Mailbox
 * @param nserv   NNTP server
 * @retval  1 New groups found
 * @retval  0 No new groups
 * @retval -1 Error
 */
int nntp_check_new_groups(struct Mailbox *mailbox, struct NntpServer *nserv)
{
  struct NntpMboxData mdata;
  time_t now;
  struct tm *tm = NULL;
  char buf[LONG_STRING];
  char *msg = _("Checking for new newsgroups...");
  unsigned int i;
  int rc, update_active = false;

  if (!nserv || !nserv->newgroups_time)
    return -1;

  /* check subscribed newsgroups for new articles */
  if (ShowNewNews)
  {
    mutt_message(_("Checking for new messages..."));
    for (i = 0; i < nserv->groups_num; i++)
    {
      struct NntpMboxData *data = nserv->groups_list[i];

      if (data && data->subscribed)
      {
        rc = nntp_group_poll(data, 1);
        if (rc < 0)
          return -1;
        if (rc > 0)
          update_active = true;
      }
    }
    /* select current newsgroup */
    if (mailbox && (mailbox->magic == MUTT_NNTP))
    {
      buf[0] = '\0';
      if (nntp_query(mailbox->data, buf, sizeof(buf)) < 0)
        return -1;
    }
  }
  else if (nserv->newgroups_time)
    return 0;

  /* get list of new groups */
  mutt_message(msg);
  if (nntp_date(nserv, &now) < 0)
    return -1;
  mdata.nserv = nserv;
  if (mailbox && (mailbox->magic == MUTT_NNTP))
    mdata.group = ((struct NntpMboxData *) mailbox->data)->group;
  else
    mdata.group = NULL;
  i = nserv->groups_num;
  tm = gmtime(&nserv->newgroups_time);
  snprintf(buf, sizeof(buf), "NEWGROUPS %02d%02d%02d %02d%02d%02d GMT\r\n",
           tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
           tm->tm_min, tm->tm_sec);
  rc = nntp_fetch_lines(&mdata, buf, sizeof(buf), msg, nntp_add_group, nserv);
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
  if (nserv->groups_num != i)
  {
    int groups_num = i;

    nserv->newgroups_time = now;
    for (; i < nserv->groups_num; i++)
    {
      struct NntpMboxData *data = nserv->groups_list[i];
      data->new = true;
    }

    /* loading descriptions */
    if (NntpLoadDescription)
    {
      unsigned int count = 0;
      struct Progress progress;

      mutt_progress_init(&progress, _("Loading descriptions..."),
                         MUTT_PROGRESS_MSG, ReadInc, nserv->groups_num - i);
      for (i = groups_num; i < nserv->groups_num; i++)
      {
        struct NntpMboxData *data = nserv->groups_list[i];

        if (get_description(data, NULL, NULL) < 0)
          return -1;
        mutt_progress_update(&progress, ++count, -1);
      }
    }
    update_active = true;
    rc = 1;
  }
  if (update_active)
    nntp_active_save_cache(nserv);
  mutt_clear_error();
  return rc;
}

/**
 * nntp_check_msgid - Fetch article by Message-ID
 * @param ctx   Mailbox
 * @param msgid Message ID
 * @retval  0 Success
 * @retval  1 No such article
 * @retval -1 Error
 */
int nntp_check_msgid(struct Context *ctx, const char *msgid)
{
  struct NntpMboxData *mdata = ctx->mailbox->data;
  char buf[LONG_STRING];

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
    if (mutt_str_strncmp("430", buf, 3) == 0)
      return 1;
    mutt_error("HEAD: %s", buf);
    return -1;
  }

  /* parse header */
  if (ctx->mailbox->msg_count == ctx->mailbox->hdrmax)
    mx_alloc_memory(ctx->mailbox);
  ctx->mailbox->hdrs[ctx->mailbox->msg_count] = mutt_email_new();
  struct Email *e = ctx->mailbox->hdrs[ctx->mailbox->msg_count];
  e->data = new_emaildata();
  e->free_data = free_emaildata;
  e->env = mutt_rfc822_read_header(fp, e, false, false);
  mutt_file_fclose(&fp);

  /* get article number */
  if (e->env->xref)
    nntp_parse_xref(ctx->mailbox, e);
  else
  {
    snprintf(buf, sizeof(buf), "STAT %s\r\n", msgid);
    if (nntp_query(mdata, buf, sizeof(buf)) < 0)
    {
      mutt_email_free(&e);
      return -1;
    }
    sscanf(buf + 4, ANUM, &NNTP_EDATA(e)->article_num);
  }

  /* reset flags */
  e->read = false;
  e->old = false;
  e->deleted = false;
  e->changed = true;
  e->received = e->date_sent;
  e->index = ctx->mailbox->msg_count++;
  mx_update_context(ctx, 1);
  return 0;
}

/**
 * nntp_check_children - Fetch children of article with the Message-ID
 * @param ctx   Mailbox
 * @param msgid Message ID to find
 * @retval  0 Success
 * @retval -1 Failure
 */
int nntp_check_children(struct Context *ctx, const char *msgid)
{
  struct NntpMboxData *mdata = ctx->mailbox->data;
  struct ChildCtx cc;
  char buf[STRING];
  int rc;
  bool quiet;
  void *hc = NULL;

  if (!mdata || !mdata->nserv)
    return -1;
  if (mdata->first_message > mdata->last_loaded)
    return 0;

  /* init context */
  cc.mailbox = ctx->mailbox;
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
      if (mutt_str_strncmp("500", buf, 3) != 0)
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
  quiet = ctx->mailbox->quiet;
  ctx->mailbox->quiet = true;
#ifdef USE_HCACHE
  hc = nntp_hcache_open(mdata);
#endif
  for (int i = 0; i < cc.num; i++)
  {
    rc = nntp_fetch_headers(ctx, hc, cc.child[i], cc.child[i], 1);
    if (rc < 0)
      break;
  }
#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif
  ctx->mailbox->quiet = quiet;
  FREE(&cc.child);
  return (rc < 0) ? -1 : 0;
}

/**
 * nntp_compare_order - Sort to mailbox order - Implements ::sort_t
 */
int nntp_compare_order(const void *a, const void *b)
{
  struct Email **ea = (struct Email **) a;
  struct Email **eb = (struct Email **) b;

  anum_t na = NNTP_EDATA(*ea)->article_num;
  anum_t nb = NNTP_EDATA(*eb)->article_num;
  int result = (na == nb) ? 0 : (na > nb) ? 1 : -1;
  result = perform_auxsort(result, a, b);
  return SORTCODE(result);
}

/**
 * nntp_mbox_open - Implements MxOps::mbox_open()
 */
static int nntp_mbox_open(struct Context *ctx)
{
  struct NntpServer *nserv = NULL;
  struct NntpMboxData *mdata = NULL;
  char buf[HUGE_STRING];
  char server[LONG_STRING];
  char *group = NULL;
  int rc;
  void *hc = NULL;
  anum_t first, last, count = 0;
  struct Url url;

  mutt_str_strfcpy(buf, ctx->mailbox->path, sizeof(buf));
  if (url_parse(&url, buf) < 0 || !url.host || !url.path ||
      !(url.scheme == U_NNTP || url.scheme == U_NNTPS))
  {
    url_free(&url);
    mutt_error(_("%s is an invalid newsgroup specification"), ctx->mailbox->path);
    return -1;
  }

  group = url.path;
  url.path = strchr(url.path, '\0');
  url_tostring(&url, server, sizeof(server), 0);
  nserv = nntp_select_server(ctx->mailbox, server, true);
  url_free(&url);
  if (!nserv)
    return -1;
  CurrentNewsSrv = nserv;

  /* find news group data structure */
  mdata = mutt_hash_find(nserv->groups_hash, group);
  if (!mdata)
  {
    nntp_newsrc_close(nserv);
    mutt_error(_("Newsgroup %s not found on the server"), group);
    return -1;
  }

  mutt_bit_unset(ctx->mailbox->rights, MUTT_ACL_INSERT);
  if (!mdata->newsrc_ent && !mdata->subscribed && !SaveUnsubscribed)
    ctx->mailbox->readonly = true;

  /* select newsgroup */
  mutt_message(_("Selecting %s..."), group);
  buf[0] = '\0';
  if (nntp_query(mdata, buf, sizeof(buf)) < 0)
  {
    nntp_newsrc_close(nserv);
    return -1;
  }

  /* newsgroup not found, remove it */
  if (mutt_str_strncmp("411", buf, 3) == 0)
  {
    mutt_error(_("Newsgroup %s has been removed from the server"), mdata->group);
    if (!mdata->deleted)
    {
      mdata->deleted = true;
      nntp_active_save_cache(nserv);
    }
    if (mdata->newsrc_ent && !mdata->subscribed && !SaveUnsubscribed)
    {
      FREE(&mdata->newsrc_ent);
      mdata->newsrc_len = 0;
      nntp_delete_group_cache(mdata);
      nntp_newsrc_update(nserv);
    }
  }

  /* parse newsgroup info */
  else
  {
    if (sscanf(buf, "211 " ANUM " " ANUM " " ANUM, &count, &first, &last) != 3)
    {
      nntp_newsrc_close(nserv);
      mutt_error("GROUP: %s", buf);
      return -1;
    }
    mdata->first_message = first;
    mdata->last_message = last;
    mdata->deleted = false;

    /* get description if empty */
    if (NntpLoadDescription && !mdata->desc)
    {
      if (get_description(mdata, NULL, NULL) < 0)
      {
        nntp_newsrc_close(nserv);
        return -1;
      }
      if (mdata->desc)
        nntp_active_save_cache(nserv);
    }
  }

  time(&nserv->check_time);
  ctx->mailbox->data = mdata;
  if (!mdata->bcache && (mdata->newsrc_ent || mdata->subscribed || SaveUnsubscribed))
    mdata->bcache = mutt_bcache_open(&nserv->conn->account, mdata->group);

  /* strip off extra articles if adding context is greater than $nntp_context */
  first = mdata->first_message;
  if (NntpContext && mdata->last_message - first + 1 > NntpContext)
    first = mdata->last_message - NntpContext + 1;
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
  {
    mutt_bit_unset(ctx->mailbox->rights, MUTT_ACL_WRITE);
    mutt_bit_unset(ctx->mailbox->rights, MUTT_ACL_DELETE);
  }
  nntp_newsrc_close(nserv);
  rc = nntp_fetch_headers(ctx, hc, first, mdata->last_message, 0);
#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif
  if (rc < 0)
    return -1;
  mdata->last_loaded = mdata->last_message;
  nserv->newsrc_modified = false;
  return 0;
}

/**
 * nntp_mbox_check - Implements MxOps::mbox_check()
 * @param ctx        Mailbox
 * @param index_hint Current message (UNUSED)
 * @retval #MUTT_REOPENED Articles have been renumbered or removed from server
 * @retval #MUTT_NEW_MAIL New articles found
 * @retval  0             No change
 * @retval -1             Lost connection
 */
static int nntp_mbox_check(struct Context *ctx, int *index_hint)
{
  int ret = check_mailbox(ctx);
  if (ret == 0)
  {
    struct NntpMboxData *mdata = ctx->mailbox->data;
    struct NntpServer *nserv = mdata->nserv;
    nntp_newsrc_close(nserv);
  }
  return ret;
}

/**
 * nntp_mbox_sync - Implements MxOps::mbox_sync()
 *
 * @note May also return values from check_mailbox()
 */
static int nntp_mbox_sync(struct Context *ctx, int *index_hint)
{
  struct NntpMboxData *mdata = ctx->mailbox->data;
  int rc;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
#endif

  /* check for new articles */
  mdata->nserv->check_time = 0;
  rc = check_mailbox(ctx);
  if (rc)
    return rc;

#ifdef USE_HCACHE
  mdata->last_cached = 0;
  hc = nntp_hcache_open(mdata);
#endif

  for (int i = 0; i < ctx->mailbox->msg_count; i++)
  {
    struct Email *e = ctx->mailbox->hdrs[i];
    char buf[16];

    snprintf(buf, sizeof(buf), "%d", NNTP_EDATA(e)->article_num);
    if (mdata->bcache && e->deleted)
    {
      mutt_debug(2, "mutt_bcache_del %s\n", buf);
      mutt_bcache_del(mdata->bcache, buf);
    }

#ifdef USE_HCACHE
    if (hc && (e->changed || e->deleted))
    {
      if (e->deleted && !e->read)
        mdata->unread--;
      mutt_debug(2, "mutt_hcache_store %s\n", buf);
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
  nntp_newsrc_gen_entries(ctx);
  nntp_newsrc_update(mdata->nserv);
  nntp_newsrc_close(mdata->nserv);
  return 0;
}

/**
 * nntp_mbox_close - Implements MxOps::mbox_close()
 * @retval 0 Always
 */
static int nntp_mbox_close(struct Context *ctx)
{
  struct NntpMboxData *mdata = ctx->mailbox->data, *nntp_tmp = NULL;
  if (!mdata)
    return 0;

  mdata->unread = ctx->mailbox->msg_unread;

  nntp_acache_free(mdata);
  if (!mdata->nserv || !mdata->nserv->groups_hash || !mdata->group)
    return 0;

  nntp_tmp = mutt_hash_find(mdata->nserv->groups_hash, mdata->group);
  if (!nntp_tmp || nntp_tmp != mdata)
    nntp_data_free(mdata);
  return 0;
}

/**
 * nntp_msg_open - Implements MxOps::msg_open()
 */
static int nntp_msg_open(struct Context *ctx, struct Message *msg, int msgno)
{
  struct NntpMboxData *mdata = ctx->mailbox->data;
  struct Email *e = ctx->mailbox->hdrs[msgno];
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
  snprintf(article, sizeof(article), "%d", NNTP_EDATA(e)->article_num);
  msg->fp = mutt_bcache_get(mdata->bcache, article);
  if (msg->fp)
  {
    if (NNTP_EDATA(e)->parsed)
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
      acache->path = mutt_str_strdup(buf);
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
             NNTP_EDATA(e)->article_num ? article : e->env->message_id);
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
        if (mutt_str_strncmp(NNTP_EDATA(e)->article_num ? "423" : "430", buf, 3) == 0)
        {
          mutt_error(_("Article %d not found on the server"),
                     NNTP_EDATA(e)->article_num ? article : e->env->message_id);
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
  if (ctx->mailbox->id_hash && e->env->message_id)
    mutt_hash_delete(ctx->mailbox->id_hash, e->env->message_id, e);
  if (ctx->mailbox->subj_hash && e->env->real_subj)
    mutt_hash_delete(ctx->mailbox->subj_hash, e->env->real_subj, e);

  mutt_env_free(&e->env);
  e->env = mutt_rfc822_read_header(msg->fp, e, false, false);

  if (ctx->mailbox->id_hash && e->env->message_id)
    mutt_hash_insert(ctx->mailbox->id_hash, e->env->message_id, e);
  if (ctx->mailbox->subj_hash && e->env->real_subj)
    mutt_hash_insert(ctx->mailbox->subj_hash, e->env->real_subj, e);

  /* fix content length */
  fseek(msg->fp, 0, SEEK_END);
  e->content->length = ftell(msg->fp) - e->content->offset;

  /* this is called in neomutt before the open which fetches the message,
   * which is probably wrong, but we just call it again here to handle
   * the problem instead of fixing it */
  NNTP_EDATA(e)->parsed = true;
  mutt_parse_mime_message(ctx, e);

  /* these would normally be updated in mx_update_context(), but the
   * full headers aren't parsed with overview, so the information wasn't
   * available then */
  if (WithCrypto)
    e->security = crypt_query(e->content);

  rewind(msg->fp);
  mutt_clear_error();
  return 0;
}

/**
 * nntp_msg_close - Implements MxOps::msg_close()
 *
 * @note May also return EOF Failure, see errno
 */
static int nntp_msg_close(struct Context *ctx, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * nntp_path_probe - Is this an NNTP mailbox? - Implements MxOps::path_probe()
 */
int nntp_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (mutt_str_strncasecmp(path, "news://", 7) == 0)
    return MUTT_NNTP;

  if (mutt_str_strncasecmp(path, "snews://", 8) == 0)
    return MUTT_NNTP;

  return MUTT_UNKNOWN;
}

/**
 * nntp_path_canon - Canonicalise a mailbox path - Implements MxOps::path_canon()
 */
int nntp_path_canon(char *buf, size_t buflen, const char *folder)
{
  if (!buf)
    return -1;

  if ((buf[0] == '+') || (buf[0] == '='))
  {
    if (!folder)
      return -1;

    size_t flen = mutt_str_strlen(folder);
    if ((flen > 0) && (folder[flen - 1] != '/'))
    {
      buf[0] = '/';
      mutt_str_inline_replace(buf, buflen, 0, folder);
    }
    else
    {
      mutt_str_inline_replace(buf, buflen, 1, folder);
    }
  }

  return 0;
}

/**
 * nntp_path_pretty - Implements MxOps::path_pretty()
 */
int nntp_path_pretty(char *buf, size_t buflen, const char *folder)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

/**
 * nntp_path_parent - Implements MxOps::path_parent()
 */
int nntp_path_parent(char *buf, size_t buflen)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

// clang-format off
/**
 * struct mx_nntp_ops - NNTP mailbox - Implements ::MxOps
 */
struct MxOps mx_nntp_ops = {
  .magic            = MUTT_NNTP,
  .name             = "nntp",
  .mbox_open        = nntp_mbox_open,
  .mbox_open_append = NULL,
  .mbox_check       = nntp_mbox_check,
  .mbox_sync        = nntp_mbox_sync,
  .mbox_close       = nntp_mbox_close,
  .msg_open         = nntp_msg_open,
  .msg_open_new     = NULL,
  .msg_commit       = NULL,
  .msg_close        = nntp_msg_close,
  .msg_padding_size = NULL,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = nntp_path_probe,
  .path_canon       = nntp_path_canon,
  .path_pretty      = nntp_path_pretty,
  .path_parent      = nntp_path_parent,
};
// clang-format on
