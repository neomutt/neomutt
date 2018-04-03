/**
 * @file
 * Send email to an SMTP server
 *
 * @authors
 * Copyright (C) 2002 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2005-2009 Brendan Cully <brendan@kublai.com>
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

/* This file contains code for direct SMTP delivery of email messages. */

#include "config.h"
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "globals.h"
#include "mutt_account.h"
#include "mutt_curses.h"
#include "mutt_socket.h"
#include "options.h"
#include "progress.h"
#include "protos.h"
#include "url.h"
#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#define smtp_success(x) ((x) / 100 == 2)
#define SMTP_READY 334
#define SMTP_CONTINUE 354

#define SMTP_ERR_READ -2
#define SMTP_ERR_WRITE -3
#define SMTP_ERR_CODE -4

#define SMTP_PORT 25
#define SMTPS_PORT 465

#define SMTP_AUTH_SUCCESS 0
#define SMTP_AUTH_UNAVAIL 1
#define SMTP_AUTH_FAIL -1

/**
 * enum SmtpCapability - SMTP server capabilities
 */
enum SmtpCapability
{
  STARTTLS,
  AUTH,
  DSN,
  EIGHTBITMIME,
  SMTPUTF8,

  CAPMAX
};

static int Esmtp = 0;
static char *AuthMechs = NULL;
static unsigned char Capabilities[(CAPMAX + 7) / 8];

static bool valid_smtp_code(char *buf, size_t len, int *n)
{
  char code[4];

  if (len < 4)
    return false;
  code[0] = buf[0];
  code[1] = buf[1];
  code[2] = buf[2];
  code[3] = 0;
  if (mutt_str_atoi(code, n) < 0)
    return false;
  return true;
}

/**
 * smtp_get_resp - Read a command response from the SMTP server
 * @param conn SMTP connection
 * @retval  0 Success (2xx code) or continue (354 code)
 * @retval -1 Write error, or any other response code
 */
static int smtp_get_resp(struct Connection *conn)
{
  int n;
  char buf[1024];

  do
  {
    n = mutt_socket_readln(buf, sizeof(buf), conn);
    if (n < 4)
    {
      /* read error, or no response code */
      return SMTP_ERR_READ;
    }

    if (mutt_str_strncasecmp("8BITMIME", buf + 4, 8) == 0)
      mutt_bit_set(Capabilities, EIGHTBITMIME);
    else if (mutt_str_strncasecmp("AUTH ", buf + 4, 5) == 0)
    {
      mutt_bit_set(Capabilities, AUTH);
      FREE(&AuthMechs);
      AuthMechs = mutt_str_strdup(buf + 9);
    }
    else if (mutt_str_strncasecmp("DSN", buf + 4, 3) == 0)
      mutt_bit_set(Capabilities, DSN);
    else if (mutt_str_strncasecmp("STARTTLS", buf + 4, 8) == 0)
      mutt_bit_set(Capabilities, STARTTLS);
    else if (mutt_str_strncasecmp("SMTPUTF8", buf + 4, 8) == 0)
      mutt_bit_set(Capabilities, SMTPUTF8);

    if (!valid_smtp_code(buf, n, &n))
      return SMTP_ERR_CODE;

  } while (buf[3] == '-');

  if (smtp_success(n) || n == SMTP_CONTINUE)
    return 0;

  mutt_error(_("SMTP session failed: %s"), buf);
  return -1;
}

static int smtp_rcpt_to(struct Connection *conn, const struct Address *a)
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
    if (mutt_bit_isset(Capabilities, DSN) && DsnNotify)
      snprintf(buf, sizeof(buf), "RCPT TO:<%s> NOTIFY=%s\r\n", a->mailbox, DsnNotify);
    else
      snprintf(buf, sizeof(buf), "RCPT TO:<%s>\r\n", a->mailbox);
    if (mutt_socket_write(conn, buf) == -1)
      return SMTP_ERR_WRITE;
    r = smtp_get_resp(conn);
    if (r != 0)
      return r;
    a = a->next;
  }

  return 0;
}

static int smtp_data(struct Connection *conn, const char *msgfile)
{
  char buf[1024];
  struct Progress progress;
  struct stat st;
  int r, term = 0;
  size_t buflen = 0;

  FILE *fp = fopen(msgfile, "r");
  if (!fp)
  {
    mutt_error(_("SMTP session failed: unable to open %s"), msgfile);
    return -1;
  }
  stat(msgfile, &st);
  unlink(msgfile);
  mutt_progress_init(&progress, _("Sending message..."), MUTT_PROGRESS_SIZE,
                     NetInc, st.st_size);

  snprintf(buf, sizeof(buf), "DATA\r\n");
  if (mutt_socket_write(conn, buf) == -1)
  {
    mutt_file_fclose(&fp);
    return SMTP_ERR_WRITE;
  }
  r = smtp_get_resp(conn);
  if (r != 0)
  {
    mutt_file_fclose(&fp);
    return r;
  }

  while (fgets(buf, sizeof(buf) - 1, fp))
  {
    buflen = mutt_str_strlen(buf);
    term = buflen && buf[buflen - 1] == '\n';
    if (term && (buflen == 1 || buf[buflen - 2] != '\r'))
      snprintf(buf + buflen - 1, sizeof(buf) - buflen + 1, "\r\n");
    if (buf[0] == '.')
    {
      if (mutt_socket_write_d(conn, ".", -1, MUTT_SOCK_LOG_FULL) == -1)
      {
        mutt_file_fclose(&fp);
        return SMTP_ERR_WRITE;
      }
    }
    if (mutt_socket_write_d(conn, buf, -1, MUTT_SOCK_LOG_FULL) == -1)
    {
      mutt_file_fclose(&fp);
      return SMTP_ERR_WRITE;
    }
    mutt_progress_update(&progress, ftell(fp), -1);
  }
  if (!term && buflen && mutt_socket_write_d(conn, "\r\n", -1, MUTT_SOCK_LOG_FULL) == -1)
  {
    mutt_file_fclose(&fp);
    return SMTP_ERR_WRITE;
  }
  mutt_file_fclose(&fp);

  /* terminate the message body */
  if (mutt_socket_write(conn, ".\r\n") == -1)
    return SMTP_ERR_WRITE;

  r = smtp_get_resp(conn);
  if (r != 0)
    return r;

  return 0;
}

/**
 * address_uses_unicode - Do any addresses use Unicode
 * @retval true if any of the string of addresses use 8-bit characters
 */
static bool address_uses_unicode(const char *a)
{
  if (!a)
    return false;

  while (*a)
  {
    if ((unsigned char) *a & (1 << 7))
      return true;
    a++;
  }

  return false;
}

/**
 * addresses_use_unicode - Do any of a list of addresses use Unicode
 * @retval true if any use 8-bit characters
 */
static bool addresses_use_unicode(const struct Address *a)
{
  while (a)
  {
    if (a->mailbox && !a->group && address_uses_unicode(a->mailbox))
      return true;
    a = a->next;
  }
  return false;
}

static int smtp_fill_account(struct Account *account)
{
  struct Url url;

  account->flags = 0;
  account->port = 0;
  account->type = MUTT_ACCT_TYPE_SMTP;

  char *urlstr = mutt_str_strdup(SmtpUrl);
  url_parse(&url, urlstr);
  if ((url.scheme != U_SMTP && url.scheme != U_SMTPS) || !url.host ||
      mutt_account_fromurl(account, &url) < 0)
  {
    url_free(&url);
    FREE(&urlstr);
    mutt_error(_("Invalid SMTP URL: %s"), SmtpUrl);
    return -1;
  }
  url_free(&url);
  FREE(&urlstr);

  if (url.scheme == U_SMTPS)
    account->flags |= MUTT_ACCT_SSL;

  if (!account->port)
  {
    if (account->flags & MUTT_ACCT_SSL)
      account->port = SMTPS_PORT;
    else
    {
      static unsigned short SmtpPort = 0;
      if (!SmtpPort)
      {
        struct servent *service = getservbyname("smtp", "tcp");
        if (service)
          SmtpPort = ntohs(service->s_port);
        else
          SmtpPort = SMTP_PORT;
        mutt_debug(3, "Using default SMTP port %d\n", SmtpPort);
      }
      account->port = SmtpPort;
    }
  }

  return 0;
}

static int smtp_helo(struct Connection *conn)
{
  char buf[LONG_STRING];
  const char *fqdn = NULL;

  memset(Capabilities, 0, sizeof(Capabilities));

  if (!Esmtp)
  {
    /* if TLS or AUTH are requested, use EHLO */
    if (conn->account.flags & MUTT_ACCT_USER)
      Esmtp = 1;
#ifdef USE_SSL
    if (SslForceTls || SslStarttls != MUTT_NO)
      Esmtp = 1;
#endif
  }

  fqdn = mutt_fqdn(0);
  if (!fqdn)
    fqdn = NONULL(ShortHostname);

  snprintf(buf, sizeof(buf), "%s %s\r\n", Esmtp ? "EHLO" : "HELO", fqdn);
  /* XXX there should probably be a wrapper in mutt_socket.c that
   * repeatedly calls conn->write until all data is sent.  This
   * currently doesn't check for a short write.
   */
  if (mutt_socket_write(conn, buf) == -1)
    return SMTP_ERR_WRITE;
  return smtp_get_resp(conn);
}

#ifdef USE_SASL
static int smtp_auth_sasl(struct Connection *conn, const char *mechlist)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  const char *mech = NULL;
  const char *data = NULL;
  unsigned int len;
  char *buf = NULL;
  size_t bufsize = 0;
  int rc, saslrc;

  if (mutt_sasl_client_new(conn, &saslconn) < 0)
    return SMTP_AUTH_FAIL;

  do
  {
    rc = sasl_client_start(saslconn, mechlist, &interaction, &data, &len, &mech);
    if (rc == SASL_INTERACT)
      mutt_sasl_interact(interaction);
  } while (rc == SASL_INTERACT);

  if (rc != SASL_OK && rc != SASL_CONTINUE)
  {
    mutt_debug(2, "%s unavailable\n", mech);
    sasl_dispose(&saslconn);
    return SMTP_AUTH_UNAVAIL;
  }

  if (!OPT_NO_CURSES)
    mutt_message(_("Authenticating (%s)..."), mech);

  bufsize = ((len * 2) > LONG_STRING) ? (len * 2) : LONG_STRING;
  buf = mutt_mem_malloc(bufsize);

  snprintf(buf, bufsize, "AUTH %s", mech);
  if (len)
  {
    mutt_str_strcat(buf, bufsize, " ");
    if (sasl_encode64(data, len, buf + mutt_str_strlen(buf),
                      bufsize - mutt_str_strlen(buf), &len) != SASL_OK)
    {
      mutt_debug(1, "#1 error base64-encoding client response.\n");
      goto fail;
    }
  }
  mutt_str_strcat(buf, bufsize, "\r\n");

  do
  {
    if (mutt_socket_write(conn, buf) < 0)
      goto fail;
    rc = mutt_socket_readln(buf, bufsize, conn);
    if (rc < 0)
      goto fail;
    if (!valid_smtp_code(buf, rc, &rc))
      goto fail;

    if (rc != SMTP_READY)
      break;

    if (sasl_decode64(buf + 4, strlen(buf + 4), buf, bufsize - 1, &len) != SASL_OK)
    {
      mutt_debug(1, "error base64-decoding server response.\n");
      goto fail;
    }

    do
    {
      saslrc = sasl_client_step(saslconn, buf, len, &interaction, &data, &len);
      if (saslrc == SASL_INTERACT)
        mutt_sasl_interact(interaction);
    } while (saslrc == SASL_INTERACT);

    if (len)
    {
      if ((len * 2) > bufsize)
      {
        bufsize = len * 2;
        mutt_mem_realloc(&buf, bufsize);
      }
      if (sasl_encode64(data, len, buf, bufsize, &len) != SASL_OK)
      {
        mutt_debug(1, "#2 error base64-encoding client response.\n");
        goto fail;
      }
    }
    mutt_str_strfcpy(buf + len, "\r\n", bufsize - len);
  } while (rc == SMTP_READY && saslrc != SASL_FAIL);

  if (smtp_success(rc))
  {
    mutt_sasl_setup_conn(conn, saslconn);
    FREE(&buf);
    return SMTP_AUTH_SUCCESS;
  }

fail:
  sasl_dispose(&saslconn);
  FREE(&buf);
  return SMTP_AUTH_FAIL;
}

static int smtp_auth(struct Connection *conn)
{
  int r = SMTP_AUTH_UNAVAIL;

  if (SmtpAuthenticators && *SmtpAuthenticators)
  {
    char *methods = mutt_str_strdup(SmtpAuthenticators);
    char *method = NULL;
    char *delim = NULL;

    for (method = methods; method; method = delim)
    {
      delim = strchr(method, ':');
      if (delim)
        *delim++ = '\0';
      if (!method[0])
        continue;

      mutt_debug(2, "Trying method %s\n", method);

      r = smtp_auth_sasl(conn, method);

      if (r == SMTP_AUTH_FAIL && delim)
      {
        mutt_error(_("%s authentication failed, trying next method"), method);
      }
      else if (r != SMTP_AUTH_UNAVAIL)
        break;
    }

    FREE(&methods);
  }
  else
    r = smtp_auth_sasl(conn, AuthMechs);

  if (r != SMTP_AUTH_SUCCESS)
    mutt_account_unsetpass(&conn->account);

  if (r == SMTP_AUTH_FAIL)
  {
    mutt_error(_("SASL authentication failed"));
  }
  else if (r == SMTP_AUTH_UNAVAIL)
  {
    mutt_error(_("No authenticators available"));
  }

  return r == SMTP_AUTH_SUCCESS ? 0 : -1;
}

#else  /* USE_SASL */

static int smtp_auth_plain(struct Connection *conn)
{
  char buf[LONG_STRING];
  size_t len;
  const char *method = NULL;
  const char *delim = NULL;
  const char *error = _("SASL authentication failed");

  if (!SmtpAuthenticators || !*SmtpAuthenticators)
  {
    goto error;
  }

  /* Check if any elements in SmtpAuthenticators is "plain" */
  for (method = delim = SmtpAuthenticators;
       *delim && (delim = mutt_str_strchrnul(method, ':')); method = delim + 1)
  {
    if (mutt_str_strncasecmp(method, "plain", 5) == 0)
    {
      /* Get username and password. Bail out of any cannot be retrieved. */
      if ((mutt_account_getuser(&conn->account) < 0) ||
          (mutt_account_getpass(&conn->account) < 0))
      {
        goto error;
      }

      /* Build the initial client response. */
      len = mutt_sasl_plain_msg(buf, sizeof(buf), "AUTH PLAIN", conn->account.user,
                                conn->account.user, conn->account.pass);

      /* Terminate as per SMTP protocol. Bail out if there's no room left. */
      if (snprintf(buf + len, sizeof(buf) - len, "\r\n") != 2)
      {
        goto error;
      }

      /* Send request, receive response (with a check for OK code). */
      if ((mutt_socket_write(conn, buf) < 0) || smtp_get_resp(conn))
      {
        goto error;
      }

      /* If we got here, auth was successful. */
      return 0;
    }
  }

  error = _("No authenticators available");

error:
  mutt_error(error);
  return -1;
}
#endif /* USE_SASL */

static int smtp_open(struct Connection *conn)
{
  int rc;

  if (mutt_socket_open(conn))
    return -1;

  /* get greeting string */
  rc = smtp_get_resp(conn);
  if (rc != 0)
    return rc;

  rc = smtp_helo(conn);
  if (rc != 0)
    return rc;

#ifdef USE_SSL
  if (conn->ssf)
    rc = MUTT_NO;
  else if (SslForceTls)
    rc = MUTT_YES;
  else if (mutt_bit_isset(Capabilities, STARTTLS) &&
           (rc = query_quadoption(SslStarttls,
                                  _("Secure connection with TLS?"))) == MUTT_ABORT)
    return rc;

  if (rc == MUTT_YES)
  {
    if (mutt_socket_write(conn, "STARTTLS\r\n") < 0)
      return SMTP_ERR_WRITE;
    rc = smtp_get_resp(conn);
    if (rc != 0)
      return rc;

    if (mutt_ssl_starttls(conn))
    {
      mutt_error(_("Could not negotiate TLS connection"));
      return -1;
    }

    /* re-EHLO to get authentication mechanisms */
    rc = smtp_helo(conn);
    if (rc != 0)
      return rc;
  }
#endif

  if (conn->account.flags & MUTT_ACCT_USER)
  {
    if (!mutt_bit_isset(Capabilities, AUTH))
    {
      mutt_error(_("SMTP server does not support authentication"));
      return -1;
    }

#ifdef USE_SASL
    return smtp_auth(conn);
#else
    return smtp_auth_plain(conn);
#endif /* USE_SASL */
  }

  return 0;
}

int mutt_smtp_send(const struct Address *from, const struct Address *to,
                   const struct Address *cc, const struct Address *bcc,
                   const char *msgfile, int eightbit)
{
  struct Connection *conn = NULL;
  struct Account account;
  const char *envfrom = NULL;
  char buf[1024];
  int rc = -1;

  /* it might be better to synthesize an envelope from from user and host
   * but this condition is most likely arrived at accidentally */
  if (EnvelopeFromAddress)
    envfrom = EnvelopeFromAddress->mailbox;
  else if (from)
    envfrom = from->mailbox;
  else
  {
    mutt_error(_("No from address given"));
    return -1;
  }

  if (smtp_fill_account(&account) < 0)
    return rc;

  conn = mutt_conn_find(NULL, &account);
  if (!conn)
    return -1;

  Esmtp = eightbit;

  do
  {
    /* send our greeting */
    rc = smtp_open(conn);
    if (rc != 0)
      break;
    FREE(&AuthMechs);

    /* send the sender's address */
    int len = snprintf(buf, sizeof(buf), "MAIL FROM:<%s>", envfrom);
    if (eightbit && mutt_bit_isset(Capabilities, EIGHTBITMIME))
    {
      mutt_str_strncat(buf, sizeof(buf), " BODY=8BITMIME", 15);
      len += 14;
    }
    if (DsnReturn && mutt_bit_isset(Capabilities, DSN))
      len += snprintf(buf + len, sizeof(buf) - len, " RET=%s", DsnReturn);
    if (mutt_bit_isset(Capabilities, SMTPUTF8) &&
        (address_uses_unicode(envfrom) || addresses_use_unicode(to) ||
         addresses_use_unicode(cc) || addresses_use_unicode(bcc)))
    {
      snprintf(buf + len, sizeof(buf) - len, " SMTPUTF8");
    }
    mutt_str_strncat(buf, sizeof(buf), "\r\n", 3);
    if (mutt_socket_write(conn, buf) == -1)
    {
      rc = SMTP_ERR_WRITE;
      break;
    }
    rc = smtp_get_resp(conn);
    if (rc != 0)
      break;

    /* send the recipient list */
    if ((rc = smtp_rcpt_to(conn, to)) || (rc = smtp_rcpt_to(conn, cc)) ||
        (rc = smtp_rcpt_to(conn, bcc)))
    {
      break;
    }

    /* send the message data */
    rc = smtp_data(conn, msgfile);
    if (rc != 0)
      break;

    mutt_socket_write(conn, "QUIT\r\n");

    rc = 0;
  } while (0);

  if (conn)
    mutt_socket_close(conn);

  if (rc == SMTP_ERR_READ)
    mutt_error(_("SMTP session failed: read error"));
  else if (rc == SMTP_ERR_WRITE)
    mutt_error(_("SMTP session failed: write error"));
  else if (rc == SMTP_ERR_CODE)
    mutt_error(_("Invalid server response"));

  return rc;
}
