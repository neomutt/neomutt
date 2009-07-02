/* mutt - text oriented MIME mail user agent
 * Copyright (C) 2002 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2005-9 Brendan Cully <brendan@kublai.com>
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

/* This file contains code for direct SMTP delivery of email messages. */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_socket.h"
#ifdef USE_SSL
# include "mutt_ssl.h"
#endif
#ifdef USE_SASL
#include "mutt_sasl.h"

#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/stat.h>

#define smtp_success(x) ((x)/100 == 2)
#define smtp_ready 334
#define smtp_continue 354

#define smtp_err_read -2
#define smtp_err_write -3
#define smtp_err_code -4

#define SMTP_PORT 25
#define SMTPS_PORT 465

#define SMTP_AUTH_SUCCESS 0
#define SMTP_AUTH_UNAVAIL 1
#define SMTP_AUTH_FAIL    -1

enum {
  STARTTLS,
  AUTH,
  DSN,
  EIGHTBITMIME,

  CAPMAX
};

#ifdef USE_SASL
static int smtp_auth (CONNECTION* conn);
static int smtp_auth_sasl (CONNECTION* conn, const char* mechanisms);
#endif

static int smtp_fill_account (ACCOUNT* account);
static int smtp_open (CONNECTION* conn);

static int Esmtp = 0;
static char* AuthMechs = NULL;
static unsigned char Capabilities[(CAPMAX + 7)/ 8];

static int smtp_code (char *buf, size_t len, int *n)
{
  char code[4];

  if (len < 4)
    return -1;
  code[0] = buf[0];
  code[1] = buf[1];
  code[2] = buf[2];
  code[3] = 0;
  if (mutt_atoi (code, n) < 0)
    return -1;
  return 0;
}

/* Reads a command response from the SMTP server.
 * Returns:
 * 0	on success (2xx code) or continue (354 code)
 * -1	write error, or any other response code
 */
static int
smtp_get_resp (CONNECTION * conn)
{
  int n;
  char buf[1024];

  do {
    n = mutt_socket_readln (buf, sizeof (buf), conn);
    if (n < 4) {
      /* read error, or no response code */
      return smtp_err_read;
    }

    if (!ascii_strncasecmp ("8BITMIME", buf + 4, 8))
      mutt_bit_set (Capabilities, EIGHTBITMIME);
    else if (!ascii_strncasecmp ("AUTH ", buf + 4, 5))
    {
      mutt_bit_set (Capabilities, AUTH);
      FREE (&AuthMechs);
      AuthMechs = safe_strdup (buf + 9);
    }
    else if (!ascii_strncasecmp ("DSN", buf + 4, 3))
      mutt_bit_set (Capabilities, DSN);
    else if (!ascii_strncasecmp ("STARTTLS", buf + 4, 8))
      mutt_bit_set (Capabilities, STARTTLS);

    if (smtp_code (buf, n, &n) < 0)
      return smtp_err_code;

  } while (buf[3] == '-');

  if (smtp_success (n) || n == smtp_continue)
    return 0;

  mutt_error (_("SMTP session failed: %s"), buf);
    return -1;
}

static int
smtp_rcpt_to (CONNECTION * conn, const ADDRESS * a)
{
  char buf[1024];
  int r;

  while (a)
  {
    /* weed out group mailboxes, since those are for display only */
    if (!a->mailbox || a->group)
    {
      a = a->next;
      continue;
    }
    if (mutt_bit_isset (Capabilities, DSN) && DsnNotify)
      snprintf (buf, sizeof (buf), "RCPT TO:<%s> NOTIFY=%s\r\n",
                a->mailbox, DsnNotify);
    else
      snprintf (buf, sizeof (buf), "RCPT TO:<%s>\r\n", a->mailbox);
    if (mutt_socket_write (conn, buf) == -1)
      return smtp_err_write;
    if ((r = smtp_get_resp (conn)))
      return r;
    a = a->next;
  }

  return 0;
}

static int
smtp_data (CONNECTION * conn, const char *msgfile)
{
  char buf[1024];
  FILE *fp = 0;
  progress_t progress;
  struct stat st;
  int r, term = 0;
  size_t buflen = 0;

  fp = fopen (msgfile, "r");
  if (!fp)
  {
    mutt_error (_("SMTP session failed: unable to open %s"), msgfile);
    return -1;
  }
  stat (msgfile, &st);
  unlink (msgfile);
  mutt_progress_init (&progress, _("Sending message..."), M_PROGRESS_SIZE,
                      NetInc, st.st_size);

  snprintf (buf, sizeof (buf), "DATA\r\n");
  if (mutt_socket_write (conn, buf) == -1)
  {
    safe_fclose (&fp);
    return smtp_err_write;
  }
  if ((r = smtp_get_resp (conn)))
  {
    safe_fclose (&fp);
    return r;
  }

  while (fgets (buf, sizeof (buf) - 1, fp))
  {
    buflen = mutt_strlen (buf);
    term = buf[buflen-1] == '\n';
    if (buflen && buf[buflen-1] == '\n'
	&& (buflen == 1 || buf[buflen - 2] != '\r'))
      snprintf (buf + buflen - 1, sizeof (buf) - buflen + 1, "\r\n");
    if (buf[0] == '.')
    {
      if (mutt_socket_write_d (conn, ".", -1, M_SOCK_LOG_FULL) == -1)
      {
        safe_fclose (&fp);
        return smtp_err_write;
      }
    }
    if (mutt_socket_write_d (conn, buf, -1, M_SOCK_LOG_FULL) == -1)
    {
      safe_fclose (&fp);
      return smtp_err_write;
    }
    mutt_progress_update (&progress, ftell (fp), -1);
  }
  if (!term && buflen &&
      mutt_socket_write_d (conn, "\r\n", -1, M_SOCK_LOG_FULL) == -1)
  {
    safe_fclose (&fp);
    return smtp_err_write;
  }
  safe_fclose (&fp);

  /* terminate the message body */
  if (mutt_socket_write (conn, ".\r\n") == -1)
    return smtp_err_write;

  if ((r = smtp_get_resp (conn)))
    return r;

  return 0;
}

int
mutt_smtp_send (const ADDRESS* from, const ADDRESS* to, const ADDRESS* cc,
                const ADDRESS* bcc, const char *msgfile, int eightbit)
{
  CONNECTION *conn;
  ACCOUNT account;
  const char* envfrom;
  char buf[1024];
  int ret = -1;

  /* it might be better to synthesize an envelope from from user and host
   * but this condition is most likely arrived at accidentally */
  if (EnvFrom)
    envfrom = EnvFrom->mailbox;
  else if (from)
    envfrom = from->mailbox;
  else
  {
    mutt_error (_("No from address given"));
    return -1;
  }

  if (smtp_fill_account (&account) < 0)
    return ret;

  if (!(conn = mutt_conn_find (NULL, &account)))
    return -1;

  Esmtp = eightbit;

  do
  {
    /* send our greeting */
    if (( ret = smtp_open (conn)))
      break;
    FREE (&AuthMechs);

    /* send the sender's address */
    ret = snprintf (buf, sizeof (buf), "MAIL FROM:<%s>", envfrom);
    if (eightbit && mutt_bit_isset (Capabilities, EIGHTBITMIME))
    {
      safe_strncat (buf, sizeof (buf), " BODY=8BITMIME", 15);
      ret += 14;
    }
    if (DsnReturn && mutt_bit_isset (Capabilities, DSN))
      ret += snprintf (buf + ret, sizeof (buf) - ret, " RET=%s", DsnReturn);
    safe_strncat (buf, sizeof (buf), "\r\n", 3);
    if (mutt_socket_write (conn, buf) == -1)
    {
      ret = smtp_err_write;
      break;
    }
    if ((ret = smtp_get_resp (conn)))
      break;

    /* send the recipient list */
    if ((ret = smtp_rcpt_to (conn, to)) || (ret = smtp_rcpt_to (conn, cc))
        || (ret = smtp_rcpt_to (conn, bcc)))
      break;

    /* send the message data */
    if ((ret = smtp_data (conn, msgfile)))
      break;

    mutt_socket_write (conn, "QUIT\r\n");

    ret = 0;
  }
  while (0);

  if (conn)
    mutt_socket_close (conn);

  if (ret == smtp_err_read)
    mutt_error (_("SMTP session failed: read error"));
  else if (ret == smtp_err_write)
    mutt_error (_("SMTP session failed: write error"));
  else if (ret == smtp_err_code)
    mutt_error (_("Invalid server response"));

  return ret;
}

static int smtp_fill_account (ACCOUNT* account)
{
  static unsigned short SmtpPort = 0;

  struct servent* service;
  ciss_url_t url;
  char* urlstr;

  account->flags = 0;
  account->port = 0;
  account->type = M_ACCT_TYPE_SMTP;

  urlstr = safe_strdup (SmtpUrl);
  url_parse_ciss (&url, urlstr);
  if ((url.scheme != U_SMTP && url.scheme != U_SMTPS)
      || mutt_account_fromurl (account, &url) < 0)
  {
    FREE (&urlstr);
    mutt_error (_("Invalid SMTP URL: %s"), SmtpUrl);
    mutt_sleep (1);
    return -1;
  }
  FREE (&urlstr);

  if (url.scheme == U_SMTPS)
    account->flags |= M_ACCT_SSL;

  if (!account->port)
  {
    if (account->flags & M_ACCT_SSL)
      account->port = SMTPS_PORT;
    else
    {
      if (!SmtpPort)
      {
        service = getservbyname ("smtp", "tcp");
        if (service)
          SmtpPort = ntohs (service->s_port);
        else
          SmtpPort = SMTP_PORT;
        dprint (3, (debugfile, "Using default SMTP port %d\n", SmtpPort));
      }
      account->port = SmtpPort;
    }
  }

  return 0;
}

static int smtp_helo (CONNECTION* conn)
{
  char buf[LONG_STRING];
  const char* fqdn;

  memset (Capabilities, 0, sizeof (Capabilities));

  if (!Esmtp)
  {
    /* if TLS or AUTH are requested, use EHLO */
    if (conn->account.flags & M_ACCT_USER)
      Esmtp = 1;
#ifdef USE_SSL
    if (option (OPTSSLFORCETLS) || quadoption (OPT_SSLSTARTTLS) != M_NO)
      Esmtp = 1;
#endif
  }

  if(!(fqdn = mutt_fqdn (0)))
    fqdn = NONULL (Hostname);

  snprintf (buf, sizeof (buf), "%s %s\r\n", Esmtp ? "EHLO" : "HELO", fqdn);
  /* XXX there should probably be a wrapper in mutt_socket.c that
    * repeatedly calls conn->write until all data is sent.  This
    * currently doesn't check for a short write.
    */
  if (mutt_socket_write (conn, buf) == -1)
    return smtp_err_write;
  return smtp_get_resp (conn);
}

static int smtp_open (CONNECTION* conn)
{
  int rc;

  if (mutt_socket_open (conn))
    return -1;

  /* get greeting string */
  if ((rc = smtp_get_resp (conn)))
    return rc;

  if ((rc = smtp_helo (conn)))
    return rc;

#ifdef USE_SSL
  if (conn->ssf)
    rc = M_NO;
  else if (option (OPTSSLFORCETLS))
    rc = M_YES;
  else if (mutt_bit_isset (Capabilities, STARTTLS) &&
           (rc = query_quadoption (OPT_SSLSTARTTLS,
                                   _("Secure connection with TLS?"))) == -1)
    return rc;

  if (rc == M_YES)
  {
    if (mutt_socket_write (conn, "STARTTLS\r\n") < 0)
      return smtp_err_write;
    if ((rc = smtp_get_resp (conn)))
      return rc;

    if (mutt_ssl_starttls (conn))
    {
      mutt_error (_("Could not negotiate TLS connection"));
      mutt_sleep (1);
      return -1;
    }

    /* re-EHLO to get authentication mechanisms */
    if ((rc = smtp_helo (conn)))
      return rc;
  }
#endif

  if (conn->account.flags & M_ACCT_USER)
  {
    if (!mutt_bit_isset (Capabilities, AUTH))
    {
      mutt_error (_("SMTP server does not support authentication"));
      mutt_sleep (1);
      return -1;
    }

#ifdef USE_SASL
    return smtp_auth (conn);
#else
    mutt_error (_("SMTP authentication requires SASL"));
    mutt_sleep (1);
    return -1;
#endif /* USE_SASL */
  }

  return 0;
}

#ifdef USE_SASL
static int smtp_auth (CONNECTION* conn)
{
  int r = SMTP_AUTH_UNAVAIL;

  if (SmtpAuthenticators && *SmtpAuthenticators)
  {
    char* methods = safe_strdup (SmtpAuthenticators);
    char* method;
    char* delim;

    for (method = methods; method; method = delim)
    {
      delim = strchr (method, ':');
      if (delim)
	*delim++ = '\0';
      if (! method[0])
	continue;

      dprint (2, (debugfile, "smtp_authenticate: Trying method %s\n", method));

      r = smtp_auth_sasl (conn, method);
      
      if (r == SMTP_AUTH_FAIL && delim)
      {
        mutt_error (_("%s authentication failed, trying next method"), method);
        mutt_sleep (1);
      }
      else if (r != SMTP_AUTH_UNAVAIL)
        break;
    }

    FREE (&methods);
  }
  else
    r = smtp_auth_sasl (conn, AuthMechs);

  if (r != SMTP_AUTH_SUCCESS)
    mutt_account_unsetpass (&conn->account);

  if (r == SMTP_AUTH_FAIL)
  {
    mutt_error (_("SASL authentication failed"));
    mutt_sleep (1);
  }
  else if (r == SMTP_AUTH_UNAVAIL)
  {
    mutt_error (_("No authenticators available"));
    mutt_sleep (1);
  }

  return r == SMTP_AUTH_SUCCESS ? 0 : -1;
}

static int smtp_auth_sasl (CONNECTION* conn, const char* mechlist)
{
  sasl_conn_t* saslconn;
  sasl_interact_t* interaction = NULL;
  const char* mech;
  const char* data = NULL;
  unsigned int len;
  char buf[HUGE_STRING];
  int rc, saslrc;

  if (mutt_sasl_client_new (conn, &saslconn) < 0)
    return SMTP_AUTH_FAIL;

  do
  {
    rc = sasl_client_start (saslconn, mechlist, &interaction, &data, &len, &mech);
    if (rc == SASL_INTERACT)
      mutt_sasl_interact (interaction);
  }
  while (rc == SASL_INTERACT);

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    dprint (2, (debugfile, "smtp_auth_sasl: %s unavailable\n", mech));
    sasl_dispose (&saslconn);
    return SMTP_AUTH_UNAVAIL;
  }

  if (!option(OPTNOCURSES))
    mutt_message (_("Authenticating (%s)..."), mech);

  snprintf (buf, sizeof (buf), "AUTH %s", mech);
  if (len)
  {
    safe_strcat (buf, sizeof (buf), " ");
    if (sasl_encode64 (data, len, buf + mutt_strlen (buf),
                       sizeof (buf) - mutt_strlen (buf), &len) != SASL_OK)
    {
      dprint (1, (debugfile, "smtp_auth_sasl: error base64-encoding client response.\n"));
      goto fail;
    }
  }
  safe_strcat (buf, sizeof (buf), "\r\n");

  do {
    if (mutt_socket_write (conn, buf) < 0)
      goto fail;
    if ((rc = mutt_socket_readln (buf, sizeof (buf), conn)) < 0)
      goto fail;
    if (smtp_code (buf, rc, &rc) < 0)
      goto fail;

    if (rc != smtp_ready)
      break;

    if (sasl_decode64 (buf+4, strlen (buf+4), buf, sizeof (buf), &len) != SASL_OK)
    {
      dprint (1, (debugfile, "smtp_auth_sasl: error base64-decoding server response.\n"));
      goto fail;
    }

    do
    {
      saslrc = sasl_client_step (saslconn, buf, len, &interaction, &data, &len);
      if (saslrc == SASL_INTERACT)
        mutt_sasl_interact (interaction);
    }
    while (saslrc == SASL_INTERACT);

    if (len)
    {
      if (sasl_encode64 (data, len, buf, sizeof (buf), &len) != SASL_OK)
      {
        dprint (1, (debugfile, "smtp_auth_sasl: error base64-encoding client response.\n"));
        goto fail;
      }
    }
    strfcpy (buf + len, "\r\n", sizeof (buf) - len);
  } while (rc == smtp_ready && saslrc != SASL_FAIL);

  if (smtp_success (rc))
  {
    mutt_sasl_setup_conn (conn, saslconn);
    return SMTP_AUTH_SUCCESS;
  }

fail:
  sasl_dispose (&saslconn);
  return SMTP_AUTH_FAIL;
}
#endif /* USE_SASL */
