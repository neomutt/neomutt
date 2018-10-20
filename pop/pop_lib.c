/**
 * @file
 * POP helper routines
 *
 * @authors
 * Copyright (C) 2000-2003 Vsevolod Volkov <vvv@mutt.org.ua>
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
 * @page pop_lib POP helper routines
 *
 * POP helper routines
 */

#include "config.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pop_private.h"
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "context.h"
#include "globals.h"
#include "mailbox.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "muttlib.h"
#include "progress.h"

/* These Config Variables are only used in pop/pop_lib.c */
unsigned char PopReconnect; ///< Config: (pop) Reconnect to the server is the connection is lost

/**
 * pop_parse_path - Parse a POP mailbox name
 * @param path Path to parse
 * @param acct Account to store details
 * @retval 0 success
 * @retval -1 error
 *
 * Split a POP path into host, port, username and password
 */
int pop_parse_path(const char *path, struct ConnAccount *acct)
{
  struct Url url;

  /* Defaults */
  acct->flags = 0;
  acct->type = MUTT_ACCT_TYPE_POP;
  acct->port = 0;

  char *c = mutt_str_strdup(path);
  url_parse(&url, c);

  if ((url.scheme != U_POP && url.scheme != U_POPS) || !url.host ||
      mutt_account_fromurl(acct, &url) < 0)
  {
    url_free(&url);
    FREE(&c);
    mutt_error(_("Invalid POP URL: %s"), path);
    return -1;
  }

  if (url.scheme == U_POPS)
    acct->flags |= MUTT_ACCT_SSL;

  struct servent *service = getservbyname(url.scheme == U_POP ? "pop3" : "pop3s", "tcp");
  if (!acct->port)
  {
    if (service)
      acct->port = ntohs(service->s_port);
    else
      acct->port = url.scheme == U_POP ? POP_PORT : POP_SSL_PORT;
    ;
  }

  url_free(&url);
  FREE(&c);
  return 0;
}

/**
 * pop_error - Copy error message to err_msg buffer
 * @param mdata POP Mailbox data
 * @param msg      Error message to save
 */
static void pop_error(struct PopMboxData *mdata, char *msg)
{
  char *t = strchr(mdata->err_msg, '\0');
  char *c = msg;

  if (mutt_str_strncmp(msg, "-ERR ", 5) == 0)
  {
    char *c2 = mutt_str_skip_email_wsp(msg + 5);

    if (*c2)
      c = c2;
  }

  mutt_str_strfcpy(t, c, sizeof(mdata->err_msg) - strlen(mdata->err_msg));
  mutt_str_remove_trailing_ws(mdata->err_msg);
}

/**
 * fetch_capa - Parse CAPA output
 * @param line List of capabilities
 * @param data POP data
 * @retval 0 (always)
 */
static int fetch_capa(char *line, void *data)
{
  struct PopMboxData *mdata = data;
  char *c = NULL;

  if (mutt_str_strncasecmp(line, "SASL", 4) == 0)
  {
    FREE(&mdata->auth_list);
    c = mutt_str_skip_email_wsp(line + 4);
    mdata->auth_list = mutt_str_strdup(c);
  }

  else if (mutt_str_strncasecmp(line, "STLS", 4) == 0)
    mdata->cmd_stls = true;

  else if (mutt_str_strncasecmp(line, "USER", 4) == 0)
    mdata->cmd_user = 1;

  else if (mutt_str_strncasecmp(line, "UIDL", 4) == 0)
    mdata->cmd_uidl = 1;

  else if (mutt_str_strncasecmp(line, "TOP", 3) == 0)
    mdata->cmd_top = 1;

  return 0;
}

/**
 * fetch_auth - Fetch list of the authentication mechanisms
 * @param line List of authentication methods
 * @param data POP data
 * @retval 0 (always)
 */
static int fetch_auth(char *line, void *data)
{
  struct PopMboxData *mdata = data;

  if (!mdata->auth_list)
  {
    mdata->auth_list = mutt_mem_malloc(strlen(line) + 1);
    *mdata->auth_list = '\0';
  }
  else
  {
    mutt_mem_realloc(&mdata->auth_list, strlen(mdata->auth_list) + strlen(line) + 2);
    strcat(mdata->auth_list, " ");
  }
  strcat(mdata->auth_list, line);

  return 0;
}

/**
 * pop_capabilities - Get capabilities from a POP server
 * @param mdata POP Mailbox data
 * @param mode     Initial capabilities
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Execution error
*/
static int pop_capabilities(struct PopMboxData *mdata, int mode)
{
  char buf[LONG_STRING];

  /* don't check capabilities on reconnect */
  if (mdata->capabilities)
    return 0;

  /* init capabilities */
  if (mode == 0)
  {
    mdata->cmd_capa = false;
    mdata->cmd_stls = false;
    mdata->cmd_user = 0;
    mdata->cmd_uidl = 0;
    mdata->cmd_top = 0;
    mdata->resp_codes = false;
    mdata->expire = true;
    mdata->login_delay = 0;
    FREE(&mdata->auth_list);
  }

  /* Execute CAPA command */
  if (mode == 0 || mdata->cmd_capa)
  {
    mutt_str_strfcpy(buf, "CAPA\r\n", sizeof(buf));
    switch (pop_fetch_data(mdata, buf, NULL, fetch_capa, mdata))
    {
      case 0:
      {
        mdata->cmd_capa = true;
        break;
      }
      case -1:
        return -1;
    }
  }

  /* CAPA not supported, use defaults */
  if (mode == 0 && !mdata->cmd_capa)
  {
    mdata->cmd_user = 2;
    mdata->cmd_uidl = 2;
    mdata->cmd_top = 2;

    mutt_str_strfcpy(buf, "AUTH\r\n", sizeof(buf));
    if (pop_fetch_data(mdata, buf, NULL, fetch_auth, mdata) == -1)
      return -1;
  }

  /* Check capabilities */
  if (mode == 2)
  {
    char *msg = NULL;

    if (!mdata->expire)
      msg = _("Unable to leave messages on server");
    if (!mdata->cmd_top)
      msg = _("Command TOP is not supported by server");
    if (!mdata->cmd_uidl)
      msg = _("Command UIDL is not supported by server");
    if (msg && mdata->cmd_capa)
    {
      mutt_error(msg);
      return -2;
    }
    mdata->capabilities = true;
  }

  return 0;
}

/**
 * pop_connect - Open connection
 * @param mdata POP Mailbox data
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid response
*/
int pop_connect(struct PopMboxData *mdata)
{
  char buf[LONG_STRING];

  mdata->status = POP_NONE;
  if (mutt_socket_open(mdata->conn) < 0 ||
      mutt_socket_readln(buf, sizeof(buf), mdata->conn) < 0)
  {
    mutt_error(_("Error connecting to server: %s"), mdata->conn->account.host);
    return -1;
  }

  mdata->status = POP_CONNECTED;

  if (mutt_str_strncmp(buf, "+OK", 3) != 0)
  {
    *mdata->err_msg = '\0';
    pop_error(mdata, buf);
    mutt_error("%s", mdata->err_msg);
    return -2;
  }

  pop_apop_timestamp(mdata, buf);

  return 0;
}

/**
 * pop_open_connection - Open connection and authenticate
 * @param mdata POP Mailbox data
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Authentication cancelled
*/
int pop_open_connection(struct PopMboxData *mdata)
{
  char buf[LONG_STRING];

  int rc = pop_connect(mdata);
  if (rc < 0)
  {
    mutt_sleep(2);
    return rc;
  }

  rc = pop_capabilities(mdata, 0);
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_sleep(2);
    return -2;
  }

#ifdef USE_SSL
  /* Attempt STLS if available and desired. */
  if (!mdata->conn->ssf && (mdata->cmd_stls || SslForceTls))
  {
    if (SslForceTls)
      mdata->use_stls = 2;
    if (mdata->use_stls == 0)
    {
      rc = query_quadoption(SslStarttls, _("Secure connection with TLS?"));
      if (rc == MUTT_ABORT)
        return -2;
      mdata->use_stls = 1;
      if (rc == MUTT_YES)
        mdata->use_stls = 2;
    }
    if (mdata->use_stls == 2)
    {
      mutt_str_strfcpy(buf, "STLS\r\n", sizeof(buf));
      rc = pop_query(mdata, buf, sizeof(buf));
      if (rc == -1)
        goto err_conn;
      if (rc != 0)
      {
        mutt_error("%s", mdata->err_msg);
      }
      else if (mutt_ssl_starttls(mdata->conn))
      {
        mutt_error(_("Could not negotiate TLS connection"));
        return -2;
      }
      else
      {
        /* recheck capabilities after STLS completes */
        rc = pop_capabilities(mdata, 1);
        if (rc == -1)
          goto err_conn;
        if (rc == -2)
        {
          mutt_sleep(2);
          return -2;
        }
      }
    }
  }

  if (SslForceTls && !mdata->conn->ssf)
  {
    mutt_error(_("Encrypted connection unavailable"));
    return -2;
  }
#endif

  rc = pop_authenticate(mdata);
  if (rc == -1)
    goto err_conn;
  if (rc == -3)
    mutt_clear_error();
  if (rc != 0)
    return rc;

  /* recheck capabilities after authentication */
  rc = pop_capabilities(mdata, 2);
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_sleep(2);
    return -2;
  }

  /* get total size of mailbox */
  mutt_str_strfcpy(buf, "STAT\r\n", sizeof(buf));
  rc = pop_query(mdata, buf, sizeof(buf));
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_error("%s", mdata->err_msg);
    return rc;
  }

  unsigned int n = 0, size = 0;
  sscanf(buf, "+OK %u %u", &n, &size);
  mdata->size = size;
  return 0;

err_conn:
  mdata->status = POP_DISCONNECTED;
  mutt_error(_("Server closed connection"));
  return -1;
}

/**
 * pop_logout - logout from a POP server
 * @param m Mailbox
 */
void pop_logout(struct Mailbox *m)
{
  struct PopMboxData *mdata = pop_get_mdata(m);

  if (mdata->status == POP_CONNECTED)
  {
    int ret = 0;
    char buf[LONG_STRING];
    mutt_message(_("Closing connection to POP server..."));

    if (m->readonly)
    {
      mutt_str_strfcpy(buf, "RSET\r\n", sizeof(buf));
      ret = pop_query(mdata, buf, sizeof(buf));
    }

    if (ret != -1)
    {
      mutt_str_strfcpy(buf, "QUIT\r\n", sizeof(buf));
      ret = pop_query(mdata, buf, sizeof(buf));
    }

    if (ret < 0)
      mutt_debug(1, "Error closing POP connection\n");

    mutt_clear_error();
  }

  mdata->status = POP_DISCONNECTED;
}

/**
 * pop_query_d - Send data from buffer and receive answer to the same buffer
 * @param mdata POP Mailbox data
 * @param buf      Buffer to send/store data
 * @param buflen   Buffer length
 * @param msg      Progress message
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
*/
int pop_query_d(struct PopMboxData *mdata, char *buf, size_t buflen, char *msg)
{
  int dbg = MUTT_SOCK_LOG_CMD;

  if (mdata->status != POP_CONNECTED)
    return -1;

  /* print msg instead of real command */
  if (msg)
  {
    dbg = MUTT_SOCK_LOG_FULL;
    mutt_debug(MUTT_SOCK_LOG_CMD, "> %s", msg);
  }

  mutt_socket_send_d(mdata->conn, buf, dbg);

  char *c = strpbrk(buf, " \r\n");
  if (c)
    *c = '\0';
  snprintf(mdata->err_msg, sizeof(mdata->err_msg), "%s: ", buf);

  if (mutt_socket_readln(buf, buflen, mdata->conn) < 0)
  {
    mdata->status = POP_DISCONNECTED;
    return -1;
  }
  if (mutt_str_strncmp(buf, "+OK", 3) == 0)
    return 0;

  pop_error(mdata, buf);
  return -2;
}

/**
 * pop_fetch_data - Read Headers with callback function
 * @param mdata POP Mailbox data
 * @param query       POP query to send to server
 * @param progressbar Progress bar
 * @param func        Function called for each header read
 * @param data        Data to pass to the callback
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Error in func(*line, *data)
 *
 * This function calls  func(*line, *data)  for each received line,
 * func(NULL, *data)  if  rewind(*data)  needs, exits when fail or done.
 */
int pop_fetch_data(struct PopMboxData *mdata, const char *query,
                   struct Progress *progressbar, int (*func)(char *, void *), void *data)
{
  char buf[LONG_STRING];
  long pos = 0;
  size_t lenbuf = 0;

  mutt_str_strfcpy(buf, query, sizeof(buf));
  int ret = pop_query(mdata, buf, sizeof(buf));
  if (ret < 0)
    return ret;

  char *inbuf = mutt_mem_malloc(sizeof(buf));

  while (true)
  {
    const int chunk = mutt_socket_readln_d(buf, sizeof(buf), mdata->conn, MUTT_SOCK_LOG_HDR);
    if (chunk < 0)
    {
      mdata->status = POP_DISCONNECTED;
      ret = -1;
      break;
    }

    char *p = buf;
    if (!lenbuf && buf[0] == '.')
    {
      if (buf[1] != '.')
        break;
      p++;
    }

    mutt_str_strfcpy(inbuf + lenbuf, p, sizeof(buf));
    pos += chunk;

    /* cast is safe since we break out of the loop when chunk<=0 */
    if ((size_t) chunk >= sizeof(buf))
    {
      lenbuf += strlen(p);
    }
    else
    {
      if (progressbar)
        mutt_progress_update(progressbar, pos, -1);
      if (ret == 0 && func(inbuf, data) < 0)
        ret = -3;
      lenbuf = 0;
    }

    mutt_mem_realloc(&inbuf, lenbuf + sizeof(buf));
  }

  FREE(&inbuf);
  return ret;
}

/**
 * check_uidl - find message with this UIDL and set refno
 * @param line String containing UIDL
 * @param data POP data
 * @retval  0 Success
 * @retval -1 Error
 */
static int check_uidl(char *line, void *data)
{
  if (!line || !data)
    return -1;

  char *endp = NULL;

  errno = 0;
  unsigned int index = strtoul(line, &endp, 10);
  if (errno != 0)
    return -1;
  while (*endp == ' ')
    endp++;
  memmove(line, endp, strlen(endp) + 1);

  struct Mailbox *m = data;
  for (int i = 0; i < m->msg_count; i++)
  {
    struct PopEmailData *edata = m->hdrs[i]->edata;
    if (mutt_str_strcmp(edata->uid, line) == 0)
    {
      m->hdrs[i]->refno = index;
      break;
    }
  }

  return 0;
}

/**
 * pop_reconnect - reconnect and verify indexes if connection was lost
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Error
 */
int pop_reconnect(struct Mailbox *m)
{
  struct PopMboxData *mdata = pop_get_mdata(m);

  if (mdata->status == POP_CONNECTED)
    return 0;
  if (mdata->status == POP_BYE)
    return -1;

  while (true)
  {
    mutt_socket_close(mdata->conn);

    int ret = pop_open_connection(mdata);
    if (ret == 0)
    {
      struct Progress progressbar;
      mutt_progress_init(&progressbar, _("Verifying message indexes..."),
                         MUTT_PROGRESS_SIZE, NetInc, 0);

      for (int i = 0; i < m->msg_count; i++)
        m->hdrs[i]->refno = -1;

      ret = pop_fetch_data(mdata, "UIDL\r\n", &progressbar, check_uidl, m);
      if (ret == -2)
      {
        mutt_error("%s", mdata->err_msg);
      }
    }
    if (ret == 0)
      return 0;

    pop_logout(m);

    if (ret < -1)
      return -1;

    if (query_quadoption(PopReconnect,
                         _("Connection lost. Reconnect to POP server?")) != MUTT_YES)
    {
      return -1;
    }
  }
}

/**
 * pop_get_mdata - Get the private data for this Mailbox
 * @param m Mailbox
 * @retval ptr PopMboxData
 */
struct PopMboxData *pop_get_mdata(struct Mailbox *m)
{
  if (!m || (m->magic != MUTT_POP))
    return NULL;
  return m->mdata;
}
