/*
 * Copyright (C) 2000 Vsevolod Volkov <vvv@mutt.org.ua>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#include "mutt.h"
#include "mx.h"
#include "url.h"
#include "pop.h"

#include <string.h>
#include <unistd.h>
#include <ctype.h>

/* given an POP mailbox name, return host, port, username and password */
int pop_parse_path (const char* path, ACCOUNT* acct)
{
  ciss_url_t url;
  char *c;
  int ret = -1;

  /* Defaults */
  acct->flags = 0;
  acct->port = POP_PORT;
  acct->type = M_ACCT_TYPE_POP;

  c = safe_strdup (path);
  url_parse_ciss (&url, c);

  if (url.scheme == U_POP || url.scheme == U_POPS)
  {
    if (url.scheme == U_POPS)
    {
      acct->flags |= M_ACCT_SSL;
      acct->port = POP_SSL_PORT;
    }

    if ((!url.path || !*url.path) && mutt_account_fromurl (acct, &url) == 0)
      ret = 0;
  }

  FREE (&c);
  return ret;
}

/* Copy error message to err_msg buffer */
void pop_error (POP_DATA *pop_data, char *msg)
{
  char *t, *c, *c2;

  t = strchr (pop_data->err_msg, '\0');
  c = msg;

  if (!mutt_strncmp (msg, "-ERR ", 5))
  {
    c2 = msg + 5;
    SKIPWS (c2);

    if (*c2)
      c = c2;
  }

  strfcpy (t, c, sizeof (pop_data->err_msg) - strlen (pop_data->err_msg));
  mutt_remove_trailing_ws (pop_data->err_msg);
}

/* Parse CAPA output */
static int fetch_capa (char *line, void *data)
{
  POP_DATA *pop_data = (POP_DATA *)data;
  char *c;

  if (!mutt_strncmp (line, "SASL", 4))
  {
    c = line + 4;
    SKIPWS (c);
    pop_data->auth_list = safe_strdup (c);
  }

  else if (!mutt_strncmp (line, "USER", 4))
    pop_data->cmd_user = 1;

  else if (!mutt_strncmp (line, "UIDL", 4))
    pop_data->cmd_uidl = 1;

  else if (!mutt_strncmp (line, "TOP", 3))
    pop_data->cmd_top = 1;

  return 0;
}

/* Fetch list of the authentication mechanisms */
static int fetch_auth (char *line, void *data)
{
  POP_DATA *pop_data = (POP_DATA *)data;

  if (!pop_data->auth_list)
  {
    pop_data->auth_list = safe_malloc (strlen (line) + 1);
    *pop_data->auth_list = '\0';
  }
  else
  {
    safe_realloc ((void **) &pop_data->auth_list,
	    strlen (pop_data->auth_list) + strlen (line) + 2);
    strcat (pop_data->auth_list, " ");	/* __STRCAT_CHECKED__ */
  }
  strcat (pop_data->auth_list, line);	/* __STRCAT_CHECKED__ */

  return 0;
}

/*
 * Get capabilities
 *  0 - successful,
 * -1 - conection lost,
 * -2 - execution error.
*/
static int pop_capabilities (POP_DATA *pop_data)
{
  char buf[LONG_STRING];
  char *msg = NULL;

  if (pop_data->capabilities)
    return 0;

  pop_data->cmd_capa = 0;
  pop_data->cmd_user = 0;
  pop_data->cmd_uidl = 0;
  pop_data->cmd_top = 0;
  pop_data->resp_codes = 0;
  pop_data->expire = 1;
  pop_data->login_delay = 0;
  FREE (&pop_data->auth_list);

  strfcpy (buf, "CAPA\r\n", sizeof (buf));
  switch (pop_fetch_data (pop_data, buf, NULL, fetch_capa, pop_data))
  {
    case 0:
    {
      pop_data->cmd_capa = 1;

      if (!pop_data->expire)
	msg = _("Unable to leave messages on server.");
      if (!pop_data->cmd_top)
	msg = _("Command TOP is not supported by server.");
      if (!pop_data->cmd_uidl)
	msg = _("Command UIDL is not supported by server.");
      break;
    }
    case -1:
      return -1;
  }

  if (!pop_data->cmd_capa)
  {
    pop_data->cmd_user = 2;
    pop_data->cmd_uidl = 2;
    pop_data->cmd_top = 2;

    strfcpy (buf, "AUTH\r\n", sizeof (buf));
    if (pop_fetch_data (pop_data, buf, NULL, fetch_auth, pop_data) == -1)
      return -1;
  }

  if (msg)
  {
    mutt_error (msg);
    return -2;
  }

  pop_data->capabilities = 1;

  return 0;
}

/*
 * Open connection
 *  0 - successful,
 * -1 - conection lost,
 * -2 - invalid response.
*/
int pop_connect (POP_DATA *pop_data)
{
  char buf[LONG_STRING];

  pop_data->status = POP_NONE;
  if (mutt_socket_open (pop_data->conn) < 0 ||
      mutt_socket_readln (buf, sizeof (buf), pop_data->conn) < 0)
  {
    mutt_error (_("Error connecting to server: %s"), pop_data->conn->account.host);
    return -1;
  }

  pop_data->status = POP_CONNECTED;

  if (mutt_strncmp (buf, "+OK", 3))
  {
    *pop_data->err_msg = '\0';
    pop_error (pop_data, buf);
    mutt_error ("%s", pop_data->err_msg);
    return -2;
  }

  pop_apop_timestamp (pop_data, buf);

  return 0;
}

/*
 * Open connection and authenticate
 *  0 - successful,
 * -1 - conection lost,
 * -2 - invalid command or execution error,
 * -3 - authentication canceled.
*/
int pop_open_connection (POP_DATA *pop_data)
{
  int ret;
  char buf[LONG_STRING];

  ret = pop_connect (pop_data);
  if (ret < 0)
  {
    sleep (2);
    return ret;
  }

  ret = pop_capabilities (pop_data);
  if (ret == -1)
    goto err_conn;
  if (ret == -2)
  {
    sleep (2);
    return -2;
  }

  ret = pop_authenticate (pop_data);
  if (ret == -1)
    goto err_conn;
  if (ret != 0)
    return ret;

  /* get total size of mailbox */
  strfcpy (buf, "STAT\r\n", sizeof (buf));
  ret = pop_query (pop_data, buf, sizeof (buf));
  if (ret == -1)
    goto err_conn;
  if (ret == -2)
  {
    mutt_error ("%s", pop_data->err_msg);
    sleep (2);
    return ret;
  }

  sscanf (buf, "+OK %d %d", &ret, &pop_data->size);
  return 0;

err_conn:
  pop_data->status = POP_DISCONNECTED;
  mutt_error _("Server closed connection!");
  sleep (2);
  return -1;
}

/* logout from POP server */
void pop_logout (CONTEXT *ctx)
{
  int ret = 0;
  char buf[LONG_STRING];
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  if (pop_data->status == POP_CONNECTED)
  {
    mutt_message _("Closing connection to POP server...");

    if (ctx->readonly)
    {
      strfcpy (buf, "RSET\r\n", sizeof (buf));
      ret = pop_query (pop_data, buf, sizeof (buf));
    }

    if (ret != -1)
    {
      strfcpy (buf, "QUIT\r\n", sizeof (buf));
      pop_query (pop_data, buf, sizeof (buf));
    }

    mutt_clear_error ();
  }

  pop_data->status = POP_DISCONNECTED;
  return;
}

/*
 * Send data from buffer and receive answer to the same buffer
 *  0 - successful,
 * -1 - conection lost,
 * -2 - invalid command or execution error.
*/
int pop_query_d (POP_DATA *pop_data, char *buf, size_t buflen, char *msg)
{
  int dbg = M_SOCK_LOG_CMD;
  char *c;

  if (pop_data->status != POP_CONNECTED)
    return -1;

#ifdef DEBUG
    /* print msg instaed of real command */
    if (msg)
    {
      dbg = M_SOCK_LOG_FULL;
      dprint (M_SOCK_LOG_CMD, (debugfile, "> %s", msg));
    }
#endif

  mutt_socket_write_d (pop_data->conn, buf, dbg);

  c = strpbrk (buf, " \r\n");
  *c = '\0';
  snprintf (pop_data->err_msg, sizeof (pop_data->err_msg), "%s: ", buf);

  if (mutt_socket_readln (buf, buflen, pop_data->conn) < 0)
  {
    pop_data->status = POP_DISCONNECTED;
    return -1;
  }
  if (!mutt_strncmp (buf, "+OK", 3))
    return 0;

  pop_error (pop_data, buf);
  return -2;
}

/*
 * This function calls  funct(*line, *data)  for each received line,
 * funct(NULL, *data)  if  rewind(*data)  needs, exits when fail or done.
 * Returned codes:
 *  0 - successful,
 * -1 - conection lost,
 * -2 - invalid command or execution error,
 * -3 - error in funct(*line, *data)
 */
int pop_fetch_data (POP_DATA *pop_data, char *query, char *msg,
		    int (*funct) (char *, void *), void *data)
{
  char buf[LONG_STRING];
  char *inbuf;
  char *p;
  int ret, chunk, line = 0;
  size_t lenbuf = 0;

  strfcpy (buf, query, sizeof (buf));
  ret = pop_query (pop_data, buf, sizeof (buf));
  if (ret < 0)
    return ret;

  inbuf = safe_malloc (sizeof (buf));

  FOREVER
  {
    chunk = mutt_socket_readln_d (buf, sizeof (buf), pop_data->conn, M_SOCK_LOG_HDR);
    if (chunk < 0)
    {
      pop_data->status = POP_DISCONNECTED;
      ret = -1;
      break;
    }

    p = buf;
    if (!lenbuf && buf[0] == '.')
    {
      if (buf[1] != '.')
	break;
      p++;
    }

    strfcpy (inbuf + lenbuf, p, sizeof (buf));

    if (chunk >= sizeof (buf))
    {
      lenbuf += strlen (p);
    }
    else
    {
      line++;
      if (msg && ReadInc && (line % ReadInc == 0))
	mutt_message ("%s %d", msg, line);
      if (ret == 0 && funct (inbuf, data) < 0)
	ret = -3;
      lenbuf = 0;
    }

    safe_realloc ((void **) &inbuf, lenbuf + sizeof (buf));
  }

  FREE (&inbuf);
  return ret;
}

/* find message with this UIDL and set refno */
static int check_uidl (char *line, void *data)
{
  int i, index;
  CONTEXT *ctx = (CONTEXT *)data;

  sscanf (line, "%d %s", &index, line);
  for (i = 0; i < ctx->msgcount; i++)
  {
    if (!mutt_strcmp (ctx->hdrs[i]->data, line))
    {
      ctx->hdrs[i]->refno = index;
      break;
    }
  }

  return 0;
}

/* reconnect and verify idnexes if connection was lost */
int pop_reconnect (CONTEXT *ctx)
{
  int ret;
  POP_DATA *pop_data = (POP_DATA *)ctx->data;

  if (pop_data->status == POP_CONNECTED)
    return 0;
  if (pop_data->status == POP_BYE)
    return -1;

  FOREVER
  {
    mutt_socket_close (pop_data->conn);

    ret = pop_open_connection (pop_data);
    if (ret == 0)
    {
      char *msg = _("Verifying message indexes...");
      int i;

      for (i = 0; i < ctx->msgcount; i++)
	ctx->hdrs[i]->refno = -1;

      mutt_message (msg);

      ret = pop_fetch_data (pop_data, "UIDL\r\n", msg, check_uidl, ctx);
      if (ret == -2)
      {
        mutt_error ("%s", pop_data->err_msg);
        sleep (2);
      }
    }
    if (ret == 0)
      return 0;

    pop_logout (ctx);

    if (ret < -1)
      return -1;

    if (query_quadoption (OPT_POPRECONNECT,
		_("Connection lost. Reconnect to POP server?")) != M_YES)
      return -1;
  }
}
