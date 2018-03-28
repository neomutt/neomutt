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

#include "config.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "context.h"
#include "globals.h"
#include "header.h"
#include "mutt_account.h"
#include "mutt_curses.h"
#include "mutt_socket.h"
#include "options.h"
#include "pop.h"
#include "progress.h"
#include "protos.h"
#include "url.h"

/**
 * pop_parse_path - Parse a POP mailbox name
 * @param path Path to parse
 * @param acct Account to store details
 * @retval 0 success
 * @retval -1 error
 *
 * Split a POP path into host, port, username and password
 */
int pop_parse_path(const char *path, struct Account *acct)
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
    mutt_error(_("Invalid POP URL: %s\n"), path);
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
 * @param pop_data POP data
 * @param msg      Error message to save
 */
static void pop_error(struct PopData *pop_data, char *msg)
{
  char *t = strchr(pop_data->err_msg, '\0');
  char *c = msg;

  if (mutt_str_strncmp(msg, "-ERR ", 5) == 0)
  {
    char *c2 = mutt_str_skip_email_wsp(msg + 5);

    if (*c2)
      c = c2;
  }

  mutt_str_strfcpy(t, c, sizeof(pop_data->err_msg) - strlen(pop_data->err_msg));
  mutt_str_remove_trailing_ws(pop_data->err_msg);
}

/**
 * fetch_capa - Parse CAPA output
 * @param line List of capabilities
 * @param data POP data
 * @retval 0 (always)
 */
static int fetch_capa(char *line, void *data)
{
  struct PopData *pop_data = (struct PopData *) data;
  char *c = NULL;

  if (mutt_str_strncasecmp(line, "SASL", 4) == 0)
  {
    FREE(&pop_data->auth_list);
    c = mutt_str_skip_email_wsp(line + 4);
    pop_data->auth_list = mutt_str_strdup(c);
  }

  else if (mutt_str_strncasecmp(line, "STLS", 4) == 0)
    pop_data->cmd_stls = true;

  else if (mutt_str_strncasecmp(line, "USER", 4) == 0)
    pop_data->cmd_user = 1;

  else if (mutt_str_strncasecmp(line, "UIDL", 4) == 0)
    pop_data->cmd_uidl = 1;

  else if (mutt_str_strncasecmp(line, "TOP", 3) == 0)
    pop_data->cmd_top = 1;

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
  struct PopData *pop_data = (struct PopData *) data;

  if (!pop_data->auth_list)
  {
    pop_data->auth_list = mutt_mem_malloc(strlen(line) + 1);
    *pop_data->auth_list = '\0';
  }
  else
  {
    mutt_mem_realloc(&pop_data->auth_list, strlen(pop_data->auth_list) + strlen(line) + 2);
    strcat(pop_data->auth_list, " ");
  }
  strcat(pop_data->auth_list, line);

  return 0;
}

/**
 * pop_capabilities - Get capabilities from a POP server
 * @param pop_data POP data
 * @param mode     Initial capabilities
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Execution error
*/
static int pop_capabilities(struct PopData *pop_data, int mode)
{
  char buf[LONG_STRING];

  /* don't check capabilities on reconnect */
  if (pop_data->capabilities)
    return 0;

  /* init capabilities */
  if (mode == 0)
  {
    pop_data->cmd_capa = false;
    pop_data->cmd_stls = false;
    pop_data->cmd_user = 0;
    pop_data->cmd_uidl = 0;
    pop_data->cmd_top = 0;
    pop_data->resp_codes = false;
    pop_data->expire = true;
    pop_data->login_delay = 0;
    FREE(&pop_data->auth_list);
  }

  /* Execute CAPA command */
  if (mode == 0 || pop_data->cmd_capa)
  {
    mutt_str_strfcpy(buf, "CAPA\r\n", sizeof(buf));
    switch (pop_fetch_data(pop_data, buf, NULL, fetch_capa, pop_data))
    {
      case 0:
      {
        pop_data->cmd_capa = true;
        break;
      }
      case -1:
        return -1;
    }
  }

  /* CAPA not supported, use defaults */
  if (mode == 0 && !pop_data->cmd_capa)
  {
    pop_data->cmd_user = 2;
    pop_data->cmd_uidl = 2;
    pop_data->cmd_top = 2;

    mutt_str_strfcpy(buf, "AUTH\r\n", sizeof(buf));
    if (pop_fetch_data(pop_data, buf, NULL, fetch_auth, pop_data) == -1)
      return -1;
  }

  /* Check capabilities */
  if (mode == 2)
  {
    char *msg = NULL;

    if (!pop_data->expire)
      msg = _("Unable to leave messages on server.");
    if (!pop_data->cmd_top)
      msg = _("Command TOP is not supported by server.");
    if (!pop_data->cmd_uidl)
      msg = _("Command UIDL is not supported by server.");
    if (msg && pop_data->cmd_capa)
    {
      mutt_error(msg);
      return -2;
    }
    pop_data->capabilities = true;
  }

  return 0;
}

/**
 * pop_connect - Open connection
 * @param pop_data POP data
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid response
*/
int pop_connect(struct PopData *pop_data)
{
  char buf[LONG_STRING];

  pop_data->status = POP_NONE;
  if (mutt_socket_open(pop_data->conn) < 0 ||
      mutt_socket_readln(buf, sizeof(buf), pop_data->conn) < 0)
  {
    mutt_error(_("Error connecting to server: %s"), pop_data->conn->account.host);
    return -1;
  }

  pop_data->status = POP_CONNECTED;

  if (mutt_str_strncmp(buf, "+OK", 3) != 0)
  {
    *pop_data->err_msg = '\0';
    pop_error(pop_data, buf);
    mutt_error("%s", pop_data->err_msg);
    return -2;
  }

  pop_apop_timestamp(pop_data, buf);

  return 0;
}

/**
 * pop_open_connection - Open connection and authenticate
 * @param pop_data POP data
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Authentication cancelled
*/
int pop_open_connection(struct PopData *pop_data)
{
  int rc;
  unsigned int n, size;
  char buf[LONG_STRING];

  rc = pop_connect(pop_data);
  if (rc < 0)
  {
    mutt_sleep(2);
    return rc;
  }

  rc = pop_capabilities(pop_data, 0);
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_sleep(2);
    return -2;
  }

#ifdef USE_SSL
  /* Attempt STLS if available and desired. */
  if (!pop_data->conn->ssf && (pop_data->cmd_stls || SslForceTls))
  {
    if (SslForceTls)
      pop_data->use_stls = 2;
    if (pop_data->use_stls == 0)
    {
      rc = query_quadoption(SslStarttls, _("Secure connection with TLS?"));
      if (rc == MUTT_ABORT)
        return -2;
      pop_data->use_stls = 1;
      if (rc == MUTT_YES)
        pop_data->use_stls = 2;
    }
    if (pop_data->use_stls == 2)
    {
      mutt_str_strfcpy(buf, "STLS\r\n", sizeof(buf));
      rc = pop_query(pop_data, buf, sizeof(buf));
      if (rc == -1)
        goto err_conn;
      if (rc != 0)
      {
        mutt_error("%s", pop_data->err_msg);
      }
      else if (mutt_ssl_starttls(pop_data->conn))
      {
        mutt_error(_("Could not negotiate TLS connection"));
        return -2;
      }
      else
      {
        /* recheck capabilities after STLS completes */
        rc = pop_capabilities(pop_data, 1);
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

  if (SslForceTls && !pop_data->conn->ssf)
  {
    mutt_error(_("Encrypted connection unavailable"));
    return -2;
  }
#endif

  rc = pop_authenticate(pop_data);
  if (rc == -1)
    goto err_conn;
  if (rc == -3)
    mutt_clear_error();
  if (rc != 0)
    return rc;

  /* recheck capabilities after authentication */
  rc = pop_capabilities(pop_data, 2);
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_sleep(2);
    return -2;
  }

  /* get total size of mailbox */
  mutt_str_strfcpy(buf, "STAT\r\n", sizeof(buf));
  rc = pop_query(pop_data, buf, sizeof(buf));
  if (rc == -1)
    goto err_conn;
  if (rc == -2)
  {
    mutt_error("%s", pop_data->err_msg);
    return rc;
  }

  sscanf(buf, "+OK %u %u", &n, &size);
  pop_data->size = size;
  return 0;

err_conn:
  pop_data->status = POP_DISCONNECTED;
  mutt_error(_("Server closed connection!"));
  return -1;
}

/**
 * pop_logout - logout from a POP server
 * @param ctx Context
 */
void pop_logout(struct Context *ctx)
{
  struct PopData *pop_data = (struct PopData *) ctx->data;

  if (pop_data->status == POP_CONNECTED)
  {
    int ret = 0;
    char buf[LONG_STRING];
    mutt_message(_("Closing connection to POP server..."));

    if (ctx->readonly)
    {
      mutt_str_strfcpy(buf, "RSET\r\n", sizeof(buf));
      ret = pop_query(pop_data, buf, sizeof(buf));
    }

    if (ret != -1)
    {
      mutt_str_strfcpy(buf, "QUIT\r\n", sizeof(buf));
      ret = pop_query(pop_data, buf, sizeof(buf));
    }

    if (ret < 0)
      mutt_debug(1, "Error closing POP connection\n");

    mutt_clear_error();
  }

  pop_data->status = POP_DISCONNECTED;
  return;
}

/**
 * pop_query_d - Send data from buffer and receive answer to the same buffer
 * @param pop_data POP data
 * @param buf      Buffer to send/store data
 * @param buflen   Buffer length
 * @param msg      Progress message
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
*/
int pop_query_d(struct PopData *pop_data, char *buf, size_t buflen, char *msg)
{
  int dbg = MUTT_SOCK_LOG_CMD;
  char *c = NULL;

  if (pop_data->status != POP_CONNECTED)
    return -1;

  /* print msg instead of real command */
  if (msg)
  {
    dbg = MUTT_SOCK_LOG_FULL;
    mutt_debug(MUTT_SOCK_LOG_CMD, "> %s", msg);
  }

  mutt_socket_write_d(pop_data->conn, buf, -1, dbg);

  c = strpbrk(buf, " \r\n");
  if (c)
    *c = '\0';
  snprintf(pop_data->err_msg, sizeof(pop_data->err_msg), "%s: ", buf);

  if (mutt_socket_readln(buf, buflen, pop_data->conn) < 0)
  {
    pop_data->status = POP_DISCONNECTED;
    return -1;
  }
  if (mutt_str_strncmp(buf, "+OK", 3) == 0)
    return 0;

  pop_error(pop_data, buf);
  return -2;
}

/**
 * pop_fetch_data - Read Headers with callback function
 * @retval  0 Successful
 * @retval -1 Connection lost
 * @retval -2 Invalid command or execution error
 * @retval -3 Error in funct(*line, *data)
 *
 * This function calls  funct(*line, *data)  for each received line,
 * funct(NULL, *data)  if  rewind(*data)  needs, exits when fail or done.
 */
int pop_fetch_data(struct PopData *pop_data, char *query, struct Progress *progressbar,
                   int (*funct)(char *, void *), void *data)
{
  char buf[LONG_STRING];
  long pos = 0;
  size_t lenbuf = 0;

  mutt_str_strfcpy(buf, query, sizeof(buf));
  int ret = pop_query(pop_data, buf, sizeof(buf));
  if (ret < 0)
    return ret;

  char *inbuf = mutt_mem_malloc(sizeof(buf));

  while (true)
  {
    const int chunk =
        mutt_socket_readln_d(buf, sizeof(buf), pop_data->conn, MUTT_SOCK_LOG_HDR);
    if (chunk < 0)
    {
      pop_data->status = POP_DISCONNECTED;
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
      if (ret == 0 && funct(inbuf, data) < 0)
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
 * @retval 0 on success
 * @retval -1 on error
 */
static int check_uidl(char *line, void *data)
{
  unsigned int index;
  struct Context *ctx = (struct Context *) data;
  char *endp = NULL;

  errno = 0;
  index = strtoul(line, &endp, 10);
  if (errno)
    return -1;
  while (*endp == ' ')
    endp++;
  memmove(line, endp, strlen(endp) + 1);

  for (int i = 0; i < ctx->msgcount; i++)
  {
    if (mutt_str_strcmp(ctx->hdrs[i]->data, line) == 0)
    {
      ctx->hdrs[i]->refno = index;
      break;
    }
  }

  return 0;
}

/**
 * pop_reconnect - reconnect and verify indexes if connection was lost
 * @param ctx Context
 * @retval 0 on success
 * @retval -1 on error
 */
int pop_reconnect(struct Context *ctx)
{
  struct PopData *pop_data = (struct PopData *) ctx->data;

  if (pop_data->status == POP_CONNECTED)
    return 0;
  if (pop_data->status == POP_BYE)
    return -1;

  while (true)
  {
    mutt_socket_close(pop_data->conn);

    int ret = pop_open_connection(pop_data);
    if (ret == 0)
    {
      struct Progress progressbar;
      mutt_progress_init(&progressbar, _("Verifying message indexes..."),
                         MUTT_PROGRESS_SIZE, NetInc, 0);

      for (int i = 0; i < ctx->msgcount; i++)
        ctx->hdrs[i]->refno = -1;

      ret = pop_fetch_data(pop_data, "UIDL\r\n", &progressbar, check_uidl, ctx);
      if (ret == -2)
      {
        mutt_error("%s", pop_data->err_msg);
      }
    }
    if (ret == 0)
      return 0;

    pop_logout(ctx);

    if (ret < -1)
      return -1;

    if (query_quadoption(PopReconnect,
                         _("Connection lost. Reconnect to POP server?")) != MUTT_YES)
    {
      return -1;
    }
  }
}
