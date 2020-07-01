/**
 * @file
 * POP helper routines
 *
 * @authors
 * Copyright (C) 2000-2003 Vsevolod Volkov <vvv@mutt.org.ua>
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
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "progress.h"
#ifdef USE_SSL
#include "mutt_globals.h"
#endif

/* These Config Variables are only used in pop/pop_lib.c */
char *C_PopOauthRefreshCommand; ///< Config: (pop) External command to generate OAUTH refresh token
char *C_PopPass; ///< Config: (pop) Password of the POP server
unsigned char C_PopReconnect; ///< Config: (pop) Reconnect to the server is the connection is lost
char *C_PopUser; ///< Config: (pop) Username of the POP server

/**
 * pop_get_field - Get connection login credentials - Implements ConnAccount::get_field()
 */
const char *pop_get_field(enum ConnAccountField field)
{
  switch (field)
  {
    case MUTT_CA_LOGIN:
    case MUTT_CA_USER:
      return C_PopUser;
    case MUTT_CA_PASS:
      return C_PopPass;
    case MUTT_CA_OAUTH_CMD:
      return C_PopOauthRefreshCommand;
    case MUTT_CA_HOST:
    default:
      return NULL;
  }
}

/**
 * pop_parse_path - Parse a POP mailbox name
 * @param path Path to parse
 * @param cac  Account to store details
 * @retval 0 success
 * @retval -1 error
 *
 * Split a POP path into host, port, username and password
 */
int pop_parse_path(const char *path, struct ConnAccount *cac)
{
  /* Defaults */
  cac->flags = 0;
  cac->type = MUTT_ACCT_TYPE_POP;
  cac->port = 0;
  cac->service = "pop";
  cac->get_field = pop_get_field;

  struct Url *url = url_parse(path);

  if (!url || ((url->scheme != U_POP) && (url->scheme != U_POPS)) ||
      !url->host || (mutt_account_fromurl(cac, url) < 0))
  {
    url_free(&url);
    mutt_error(_("Invalid POP URL: %s"), path);
    return -1;
  }

  if (url->scheme == U_POPS)
    cac->flags |= MUTT_ACCT_SSL;

  struct servent *service =
      getservbyname((url->scheme == U_POP) ? "pop3" : "pop3s", "tcp");
  if (cac->port == 0)
  {
    if (service)
      cac->port = ntohs(service->s_port);
    else
      cac->port = (url->scheme == U_POP) ? POP_PORT : POP_SSL_PORT;
  }

  url_free(&url);
  return 0;
}

/**
 * pop_error - Copy error message to err_msg buffer
 * @param adata POP Account data
 * @param msg   Error message to save
 */
static void pop_error(struct PopAccountData *adata, char *msg)
{
  char *t = strchr(adata->err_msg, '\0');
  char *c = msg;

  size_t plen = mutt_str_startswith(msg, "-ERR ");
  if (plen != 0)
  {
    char *c2 = mutt_str_skip_email_wsp(msg + plen);

    if (*c2)
      c = c2;
  }

  mutt_str_copy(t, c, sizeof(adata->err_msg) - strlen(adata->err_msg));
  mutt_str_remove_trailing_ws(adata->err_msg);
}

/**
 * fetch_capa - Parse CAPA output - Implements ::pop_fetch_t
 * @param line List of capabilities
 * @param data POP data
 * @retval 0 (always)
 */
static int fetch_capa(const char *line, void *data)
{
  struct PopAccountData *adata = data;

  if (mutt_istr_startswith(line, "SASL"))
  {
    const char *c = mutt_str_skip_email_wsp(line + 4);
    mutt_buffer_strcpy(&adata->auth_list, c);
  }
  else if (mutt_istr_startswith(line, "STLS"))
    adata->cmd_stls = true;
  else if (mutt_istr_startswith(line, "USER"))
    adata->cmd_user = 1;
  else if (mutt_istr_startswith(line, "UIDL"))
    adata->cmd_uidl = 1;
  else if (mutt_istr_startswith(line, "TOP"))
    adata->cmd_top = 1;

  return 0;
}

/**
 * fetch_auth - Fetch list of the authentication mechanisms - Implements ::pop_fetch_t
 * @param line List of authentication methods
 * @param data POP data
 * @retval 0 (always)
 */
static int fetch_auth(const char *line, void *data)
{
  struct PopAccountData *adata = data;

  if (!mutt_buffer_is_empty(&adata->auth_list))
  {
    mutt_buffer_addstr(&adata->auth_list, " ");
  }
  mutt_buffer_addstr(&adata->auth_list, line);

  return 0;
}

/**
 * pop_capabilities - Get capabilities from a POP server
 * @param adata POP Account data
 * @param mode  Initial capabilities
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Execution error
 */
static int pop_capabilities(struct PopAccountData *adata, int mode)
{
  char buf[1024];

  /* don't check capabilities on reconnect */
  if (adata->capabilities)
    return 0;

  /* init capabilities */
  if (mode == 0)
  {
    adata->cmd_capa = false;
    adata->cmd_stls = false;
    adata->cmd_user = 0;
    adata->cmd_uidl = 0;
    adata->cmd_top = 0;
    adata->resp_codes = false;
    adata->expire = true;
    adata->login_delay = 0;
    mutt_buffer_init(&adata->auth_list);
  }

  /* Execute CAPA command */
  if ((mode == 0) || adata->cmd_capa)
  {
    mutt_str_copy(buf, "CAPA\r\n", sizeof(buf));
    switch (pop_fetch_data(adata, buf, NULL, fetch_capa, adata))
    {
      case 0:
      {
        adata->cmd_capa = true;
        break;
      }
      case -1:
        return -1;
    }
  }

  /* CAPA not supported, use defaults */
  if ((mode == 0) && !adata->cmd_capa)
  {
    adata->cmd_user = 2;
    adata->cmd_uidl = 2;
    adata->cmd_top = 2;

    mutt_str_copy(buf, "AUTH\r\n", sizeof(buf));
    if (pop_fetch_data(adata, buf, NULL, fetch_auth, adata) == -1)
      return -1;
  }

  /* Check capabilities */
  if (mode == 2)
  {
    char *msg = NULL;

    if (!adata->expire)
      msg = _("Unable to leave messages on server");
    if (adata->cmd_top == 0)
      msg = _("Command TOP is not supported by server");
    if (adata->cmd_uidl == 0)
      msg = _("Command UIDL is not supported by server");
    if (msg && adata->cmd_capa)
    {
      mutt_error(msg);
      return -2;
    }
    adata->capabilities = true;
  }

  return 0;
}

/**
 * pop_edata_get - Get the private data for this Email
 * @param e Email
 * @retval ptr Private Email data
 */
struct PopEmailData *pop_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}

/**
 * pop_connect - Open connection
 * @param adata POP Account data
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid response
 */
int pop_connect(struct PopAccountData *adata)
{
  char buf[1024];

  adata->status = POP_NONE;
  if ((mutt_socket_open(adata->conn) < 0) ||
      (mutt_socket_readln(buf, sizeof(buf), adata->conn) < 0))
  {
    mutt_error(_("Error connecting to server: %s"), adata->conn->account.host);
    return -1;
  }

  adata->status = POP_CONNECTED;

  if (!mutt_str_startswith(buf, "+OK"))
  {
    *adata->err_msg = '\0';
    pop_error(adata, buf);
    mutt_error("%s", adata->err_msg);
    return -2;
  }

  pop_apop_timestamp(adata, buf);

  return 0;
}

/**
 * pop_open_connection - Open connection and authenticate
 * @param adata POP Account data
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Authentication cancelled
 */
int pop_open_connection(struct PopAccountData *adata)
{
  char buf[1024];

  int rc = pop_connect(adata);
  if (rc < 0)
    return rc;

  rc = pop_capabilities(adata, 0);
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
    return -2;

#ifdef USE_SSL
  /* Attempt STLS if available and desired. */
  if ((adata->conn->ssf == 0) && (adata->cmd_stls || C_SslForceTls))
  {
    if (C_SslForceTls)
      adata->use_stls = 2;
    if (adata->use_stls == 0)
    {
      enum QuadOption ans =
          query_quadoption(C_SslStarttls, _("Secure connection with TLS?"));
      if (ans == MUTT_ABORT)
        return -2;
      adata->use_stls = 1;
      if (ans == MUTT_YES)
        adata->use_stls = 2;
    }
    if (adata->use_stls == 2)
    {
      mutt_str_copy(buf, "STLS\r\n", sizeof(buf));
      rc = pop_query(adata, buf, sizeof(buf));
      // Clear any data after the STLS acknowledgement
      mutt_socket_empty(adata->conn);
      if (rc == -1)
        goto err_conn;
      if (rc != 0)
      {
        mutt_error("%s", adata->err_msg);
      }
      else if (mutt_ssl_starttls(adata->conn))
      {
        mutt_error(_("Could not negotiate TLS connection"));
        return -2;
      }
      else
      {
        /* recheck capabilities after STLS completes */
        rc = pop_capabilities(adata, 1);
        if (rc == -1)
          goto err_conn;
        if (rc == -2)
          return -2;
      }
    }
  }

  if (C_SslForceTls && (adata->conn->ssf == 0))
  {
    mutt_error(_("Encrypted connection unavailable"));
    return -2;
  }
#endif

  rc = pop_authenticate(adata);
  if (rc == -1)
    goto err_conn;
  if (rc == -3)
    mutt_clear_error();
  if (rc != 0)
    return rc;

  /* recheck capabilities after authentication */
  rc = pop_capabilities(adata, 2);
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
    return -2;

  /* get total size of mailbox */
  mutt_str_copy(buf, "STAT\r\n", sizeof(buf));
  rc = pop_query(adata, buf, sizeof(buf));
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_error("%s", adata->err_msg);
    return rc;
  }

  unsigned int n = 0, size = 0;
  sscanf(buf, "+OK %u %u", &n, &size);
  adata->size = size;
  return 0;

err_conn:
  adata->status = POP_DISCONNECTED;
  mutt_error(_("Server closed connection"));
  return -1;
}

/**
 * pop_logout - logout from a POP server
 * @param m Mailbox
 */
void pop_logout(struct Mailbox *m)
{
  struct PopAccountData *adata = pop_adata_get(m);

  if (adata->status == POP_CONNECTED)
  {
    int ret = 0;
    char buf[1024];
    mutt_message(_("Closing connection to POP server..."));

    if (m->readonly)
    {
      mutt_str_copy(buf, "RSET\r\n", sizeof(buf));
      ret = pop_query(adata, buf, sizeof(buf));
    }

    if (ret != -1)
    {
      mutt_str_copy(buf, "QUIT\r\n", sizeof(buf));
      ret = pop_query(adata, buf, sizeof(buf));
    }

    if (ret < 0)
      mutt_debug(LL_DEBUG1, "Error closing POP connection\n");

    mutt_clear_error();
  }

  adata->status = POP_DISCONNECTED;
}

/**
 * pop_query_d - Send data from buffer and receive answer to the same buffer
 * @param adata  POP Account data
 * @param buf    Buffer to send/store data
 * @param buflen Buffer length
 * @param msg    Progress message
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 */
int pop_query_d(struct PopAccountData *adata, char *buf, size_t buflen, char *msg)
{
  if (adata->status != POP_CONNECTED)
    return -1;

  /* print msg instead of real command */
  if (msg)
  {
    mutt_debug(MUTT_SOCK_LOG_CMD, "> %s", msg);
  }

  mutt_socket_send_d(adata->conn, buf, MUTT_SOCK_LOG_FULL);

  char *c = strpbrk(buf, " \r\n");
  if (c)
    *c = '\0';
  snprintf(adata->err_msg, sizeof(adata->err_msg), "%s: ", buf);

  if (mutt_socket_readln_d(buf, buflen, adata->conn, MUTT_SOCK_LOG_FULL) < 0)
  {
    adata->status = POP_DISCONNECTED;
    return -1;
  }
  if (mutt_str_startswith(buf, "+OK"))
    return 0;

  pop_error(adata, buf);
  return -2;
}

/**
 * pop_fetch_data - Read Headers with callback function
 * @param adata    POP Account data
 * @param query    POP query to send to server
 * @param progress Progress bar
 * @param callback Function called for each header read
 * @param data     Data to pass to the callback
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Error in callback(*line, *data)
 *
 * This function calls  callback(*line, *data)  for each received line,
 * callback(NULL, *data)  if  rewind(*data)  needs, exits when fail or done.
 */
int pop_fetch_data(struct PopAccountData *adata, const char *query,
                   struct Progress *progress, pop_fetch_t callback, void *data)
{
  char buf[1024];
  long pos = 0;
  size_t lenbuf = 0;

  mutt_str_copy(buf, query, sizeof(buf));
  int rc = pop_query(adata, buf, sizeof(buf));
  if (rc < 0)
    return rc;

  char *inbuf = mutt_mem_malloc(sizeof(buf));

  while (true)
  {
    const int chunk =
        mutt_socket_readln_d(buf, sizeof(buf), adata->conn, MUTT_SOCK_LOG_FULL);
    if (chunk < 0)
    {
      adata->status = POP_DISCONNECTED;
      rc = -1;
      break;
    }

    char *p = buf;
    if (!lenbuf && (buf[0] == '.'))
    {
      if (buf[1] != '.')
        break;
      p++;
    }

    mutt_str_copy(inbuf + lenbuf, p, sizeof(buf));
    pos += chunk;

    /* cast is safe since we break out of the loop when chunk<=0 */
    if ((size_t) chunk >= sizeof(buf))
    {
      lenbuf += strlen(p);
    }
    else
    {
      if (progress)
        mutt_progress_update(progress, pos, -1);
      if ((rc == 0) && (callback(inbuf, data) < 0))
        rc = -3;
      lenbuf = 0;
    }

    mutt_mem_realloc(&inbuf, lenbuf + sizeof(buf));
  }

  FREE(&inbuf);
  return rc;
}

/**
 * check_uidl - find message with this UIDL and set refno - Implements ::pop_fetch_t
 * @param line String containing UIDL
 * @param data POP data
 * @retval  0 Success
 * @retval -1 Error
 */
static int check_uidl(const char *line, void *data)
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

  struct Mailbox *m = data;
  for (int i = 0; i < m->msg_count; i++)
  {
    struct PopEmailData *edata = pop_edata_get(m->emails[i]);
    if (mutt_str_equal(edata->uid, endp))
    {
      edata->refno = index;
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
  struct PopAccountData *adata = pop_adata_get(m);

  if (adata->status == POP_CONNECTED)
    return 0;

  while (true)
  {
    mutt_socket_close(adata->conn);

    int ret = pop_open_connection(adata);
    if (ret == 0)
    {
      struct Progress progress;
      mutt_progress_init(&progress, _("Verifying message indexes..."), MUTT_PROGRESS_NET, 0);

      for (int i = 0; i < m->msg_count; i++)
      {
        struct PopEmailData *edata = pop_edata_get(m->emails[i]);
        edata->refno = -1;
      }

      ret = pop_fetch_data(adata, "UIDL\r\n", &progress, check_uidl, m);
      if (ret == -2)
      {
        mutt_error("%s", adata->err_msg);
      }
    }
    if (ret == 0)
      return 0;

    pop_logout(m);

    if (ret < -1)
      return -1;

    if (query_quadoption(C_PopReconnect,
                         _("Connection lost. Reconnect to POP server?")) != MUTT_YES)
    {
      return -1;
    }
  }
}

/**
 * pop_adata_get - Get the Account data for this mailbox
 * @param m Mailbox
 * @retval ptr PopAccountData
 */
struct PopAccountData *pop_adata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_POP))
    return NULL;
  struct Account *a = m->account;
  if (!a)
    return NULL;
  return a->adata;
}
