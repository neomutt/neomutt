/**
 * @file
 * Send email to an SMTP server
 *
 * @authors
 * Copyright (C) 2002 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2005-2009 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page smtp Send email to an SMTP server
 *
 * Send email to an SMTP server
 */

/* This file contains code for direct SMTP delivery of email messages. */

#include "config.h"
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "smtp.h"
#include "gui/lib.h"
#include "mutt_account.h"
#include "mutt_globals.h"
#include "mutt_socket.h"
#include "progress.h"
#include "sendlib.h"
#ifdef USE_SSL
#include "config/lib.h"
#endif
#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include "options.h"
#endif

/* These Config Variables are only used in smtp.c */
struct Slist *C_SmtpAuthenticators; ///< Config: (smtp) List of allowed authentication methods
char *C_SmtpOauthRefreshCommand; ///< Config: (smtp) External command to generate OAUTH refresh token
char *C_SmtpPass; ///< Config: (smtp) Password for the SMTP server
char *C_SmtpUser; ///< Config: (smtp) Username for the SMTP server

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

// clang-format off
/**
 * typedef SmtpCapFlags - SMTP server capabilities
 */
typedef uint8_t SmtpCapFlags;           ///< Flags, e.g. #SMTP_CAP_STARTTLS
#define SMTP_CAP_NO_FLAGS            0  ///< No flags are set
#define SMTP_CAP_STARTTLS     (1 << 0) ///< Server supports STARTTLS command
#define SMTP_CAP_AUTH         (1 << 1) ///< Server supports AUTH command
#define SMTP_CAP_DSN          (1 << 2) ///< Server supports Delivery Status Notification
#define SMTP_CAP_EIGHTBITMIME (1 << 3) ///< Server supports 8-bit MIME content
#define SMTP_CAP_SMTPUTF8     (1 << 4) ///< Server accepts UTF-8 strings

#define SMTP_CAP_ALL         ((1 << 5) - 1)
// clang-format on

static char *AuthMechs = NULL;
static SmtpCapFlags Capabilities;

/**
 * valid_smtp_code - Is the is a valid SMTP return code?
 * @param[in]  buf String to check
 * @param[in]  buflen Length of string
 * @param[out] n   Numeric value of code
 * @retval true Valid number
 */
static bool valid_smtp_code(char *buf, size_t buflen, int *n)
{
  char code[4];

  if (buflen < 4)
    return false;
  code[0] = buf[0];
  code[1] = buf[1];
  code[2] = buf[2];
  code[3] = '\0';
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
    const char *s = buf + 4; /* Skip the response code and the space/dash */
    size_t plen;

    if (mutt_istr_startswith(s, "8BITMIME"))
      Capabilities |= SMTP_CAP_EIGHTBITMIME;
    else if ((plen = mutt_istr_startswith(s, "AUTH ")))
    {
      Capabilities |= SMTP_CAP_AUTH;
      FREE(&AuthMechs);
      AuthMechs = mutt_str_dup(s + plen);
    }
    else if (mutt_istr_startswith(s, "DSN"))
      Capabilities |= SMTP_CAP_DSN;
    else if (mutt_istr_startswith(s, "STARTTLS"))
      Capabilities |= SMTP_CAP_STARTTLS;
    else if (mutt_istr_startswith(s, "SMTPUTF8"))
      Capabilities |= SMTP_CAP_SMTPUTF8;

    if (!valid_smtp_code(buf, n, &n))
      return SMTP_ERR_CODE;

  } while (buf[3] == '-');

  if (smtp_success(n) || (n == SMTP_CONTINUE))
    return 0;

  mutt_error(_("SMTP session failed: %s"), buf);
  return -1;
}

/**
 * smtp_rcpt_to - Set the recipient to an Address
 * @param conn Server Connection
 * @param al   AddressList to use
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_ERR_WRITE
 */
static int smtp_rcpt_to(struct Connection *conn, const struct AddressList *al)
{
  if (!al)
    return 0;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    /* weed out group mailboxes, since those are for display only */
    if (!a->mailbox || a->group)
    {
      continue;
    }
    char buf[1024];
    if ((Capabilities & SMTP_CAP_DSN) && C_DsnNotify)
      snprintf(buf, sizeof(buf), "RCPT TO:<%s> NOTIFY=%s\r\n", a->mailbox, C_DsnNotify);
    else
      snprintf(buf, sizeof(buf), "RCPT TO:<%s>\r\n", a->mailbox);
    if (mutt_socket_send(conn, buf) == -1)
      return SMTP_ERR_WRITE;
    int rc = smtp_get_resp(conn);
    if (rc != 0)
      return rc;
  }

  return 0;
}

/**
 * smtp_data - Send data to an SMTP server
 * @param conn    SMTP Connection
 * @param msgfile Filename containing data
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_ERR_WRITE
 */
static int smtp_data(struct Connection *conn, const char *msgfile)
{
  char buf[1024];
  struct Progress progress;
  struct stat st;
  int rc, term = 0;
  size_t buflen = 0;

  FILE *fp = fopen(msgfile, "r");
  if (!fp)
  {
    mutt_error(_("SMTP session failed: unable to open %s"), msgfile);
    return -1;
  }
  stat(msgfile, &st);
  unlink(msgfile);
  mutt_progress_init(&progress, _("Sending message..."), MUTT_PROGRESS_NET, st.st_size);

  snprintf(buf, sizeof(buf), "DATA\r\n");
  if (mutt_socket_send(conn, buf) == -1)
  {
    mutt_file_fclose(&fp);
    return SMTP_ERR_WRITE;
  }
  rc = smtp_get_resp(conn);
  if (rc != 0)
  {
    mutt_file_fclose(&fp);
    return rc;
  }

  while (fgets(buf, sizeof(buf) - 1, fp))
  {
    buflen = mutt_str_len(buf);
    term = buflen && buf[buflen - 1] == '\n';
    if (term && ((buflen == 1) || (buf[buflen - 2] != '\r')))
      snprintf(buf + buflen - 1, sizeof(buf) - buflen + 1, "\r\n");
    if (buf[0] == '.')
    {
      if (mutt_socket_send_d(conn, ".", MUTT_SOCK_LOG_FULL) == -1)
      {
        mutt_file_fclose(&fp);
        return SMTP_ERR_WRITE;
      }
    }
    if (mutt_socket_send_d(conn, buf, MUTT_SOCK_LOG_FULL) == -1)
    {
      mutt_file_fclose(&fp);
      return SMTP_ERR_WRITE;
    }
    mutt_progress_update(&progress, ftell(fp), -1);
  }
  if (!term && buflen && (mutt_socket_send_d(conn, "\r\n", MUTT_SOCK_LOG_FULL) == -1))
  {
    mutt_file_fclose(&fp);
    return SMTP_ERR_WRITE;
  }
  mutt_file_fclose(&fp);

  /* terminate the message body */
  if (mutt_socket_send(conn, ".\r\n") == -1)
    return SMTP_ERR_WRITE;

  rc = smtp_get_resp(conn);
  if (rc != 0)
    return rc;

  return 0;
}

/**
 * address_uses_unicode - Do any addresses use Unicode
 * @param a Address list to check
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
 * @param al Address list to check
 * @retval true if any use 8-bit characters
 */
static bool addresses_use_unicode(const struct AddressList *al)
{
  if (!al)
  {
    return false;
  }

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (a->mailbox && !a->group && address_uses_unicode(a->mailbox))
      return true;
  }
  return false;
}

/**
 * smtp_get_field - Get connection login credentials - Implements ConnAccount::get_field()
 */
static const char *smtp_get_field(enum ConnAccountField field)
{
  switch (field)
  {
    case MUTT_CA_LOGIN:
    case MUTT_CA_USER:
      return C_SmtpUser;
    case MUTT_CA_PASS:
      return C_SmtpPass;
    case MUTT_CA_OAUTH_CMD:
      return C_SmtpOauthRefreshCommand;
    case MUTT_CA_HOST:
    default:
      return NULL;
  }
}

/**
 * smtp_fill_account - Create ConnAccount object from SMTP Url
 * @param cac ConnAccount to populate
 * @retval  0 Success
 * @retval -1 Error
 */
static int smtp_fill_account(struct ConnAccount *cac)
{
  cac->flags = 0;
  cac->port = 0;
  cac->type = MUTT_ACCT_TYPE_SMTP;
  cac->service = "smtp";
  cac->get_field = smtp_get_field;

  struct Url *url = url_parse(C_SmtpUrl);
  if (!url || ((url->scheme != U_SMTP) && (url->scheme != U_SMTPS)) ||
      !url->host || (mutt_account_fromurl(cac, url) < 0))
  {
    url_free(&url);
    mutt_error(_("Invalid SMTP URL: %s"), C_SmtpUrl);
    return -1;
  }

  if (url->scheme == U_SMTPS)
    cac->flags |= MUTT_ACCT_SSL;

  if (cac->port == 0)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      cac->port = SMTPS_PORT;
    else
    {
      static unsigned short SmtpPort = 0;
      if (SmtpPort == 0)
      {
        struct servent *service = getservbyname("smtp", "tcp");
        if (service)
          SmtpPort = ntohs(service->s_port);
        else
          SmtpPort = SMTP_PORT;
        mutt_debug(LL_DEBUG3, "Using default SMTP port %d\n", SmtpPort);
      }
      cac->port = SmtpPort;
    }
  }

  url_free(&url);
  return 0;
}

/**
 * smtp_helo - Say hello to an SMTP Server
 * @param conn  SMTP Connection
 * @param esmtp If true, use ESMTP
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_ERR_WRITE
 */
static int smtp_helo(struct Connection *conn, bool esmtp)
{
  Capabilities = 0;

  if (!esmtp)
  {
    /* if TLS or AUTH are requested, use EHLO */
    if (conn->account.flags & MUTT_ACCT_USER)
      esmtp = true;
#ifdef USE_SSL
    if (C_SslForceTls || (C_SslStarttls != MUTT_NO))
      esmtp = true;
#endif
  }

  const char *fqdn = mutt_fqdn(false);
  if (!fqdn)
    fqdn = NONULL(ShortHostname);

  char buf[1024];
  snprintf(buf, sizeof(buf), "%s %s\r\n", esmtp ? "EHLO" : "HELO", fqdn);
  /* XXX there should probably be a wrapper in mutt_socket.c that
   * repeatedly calls conn->write until all data is sent.  This
   * currently doesn't check for a short write.  */
  if (mutt_socket_send(conn, buf) == -1)
    return SMTP_ERR_WRITE;
  return smtp_get_resp(conn);
}

#ifdef USE_SASL
/**
 * smtp_auth_sasl - Authenticate using SASL
 * @param conn     SMTP Connection
 * @param mechlist List of mechanisms to use
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 */
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

  if ((rc != SASL_OK) && (rc != SASL_CONTINUE))
  {
    mutt_debug(LL_DEBUG2, "%s unavailable\n", mech);
    sasl_dispose(&saslconn);
    return SMTP_AUTH_UNAVAIL;
  }

  if (!OptNoCurses)
    mutt_message(_("Authenticating (%s)..."), mech);

  bufsize = MAX((len * 2), 1024);
  buf = mutt_mem_malloc(bufsize);

  snprintf(buf, bufsize, "AUTH %s", mech);
  if (len)
  {
    mutt_str_cat(buf, bufsize, " ");
    if (sasl_encode64(data, len, buf + mutt_str_len(buf),
                      bufsize - mutt_str_len(buf), &len) != SASL_OK)
    {
      mutt_debug(LL_DEBUG1, "#1 error base64-encoding client response\n");
      goto fail;
    }
  }
  mutt_str_cat(buf, bufsize, "\r\n");

  do
  {
    if (mutt_socket_send(conn, buf) < 0)
      goto fail;
    rc = mutt_socket_readln_d(buf, bufsize, conn, MUTT_SOCK_LOG_FULL);
    if (rc < 0)
      goto fail;
    if (!valid_smtp_code(buf, rc, &rc))
      goto fail;

    if (rc != SMTP_READY)
      break;

    if (sasl_decode64(buf + 4, strlen(buf + 4), buf, bufsize - 1, &len) != SASL_OK)
    {
      mutt_debug(LL_DEBUG1, "error base64-decoding server response\n");
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
        mutt_debug(LL_DEBUG1, "#2 error base64-encoding client response\n");
        goto fail;
      }
    }
    mutt_str_copy(buf + len, "\r\n", bufsize - len);
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
#endif

/**
 * smtp_auth_oauth - Authenticate an SMTP connection using OAUTHBEARER
 * @param conn Connection info
 * @retval num Result, e.g. #SMTP_AUTH_SUCCESS
 */
static int smtp_auth_oauth(struct Connection *conn)
{
  mutt_message(_("Authenticating (%s)..."), "OAUTHBEARER");

  /* We get the access token from the smtp_oauth_refresh_command */
  char *oauthbearer = mutt_account_getoauthbearer(&conn->account);
  if (!oauthbearer)
    return SMTP_AUTH_FAIL;

  size_t ilen = strlen(oauthbearer) + 30;
  char *ibuf = mutt_mem_malloc(ilen);
  snprintf(ibuf, ilen, "AUTH OAUTHBEARER %s\r\n", oauthbearer);

  int rc = mutt_socket_send(conn, ibuf);
  FREE(&oauthbearer);
  FREE(&ibuf);

  if (rc == -1)
    return SMTP_AUTH_FAIL;
  if (smtp_get_resp(conn) != 0)
    return SMTP_AUTH_FAIL;

  return SMTP_AUTH_SUCCESS;
}

/**
 * smtp_auth_plain - Authenticate using plain text
 * @param conn SMTP Connection
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 */
static int smtp_auth_plain(struct Connection *conn)
{
  char buf[1024];

  /* Get username and password. Bail out of any can't be retrieved. */
  if ((mutt_account_getuser(&conn->account) < 0) ||
      (mutt_account_getpass(&conn->account) < 0))
  {
    goto error;
  }

  /* Build the initial client response. */
  size_t len = mutt_sasl_plain_msg(buf, sizeof(buf), "AUTH PLAIN", conn->account.user,
                                   conn->account.user, conn->account.pass);

  /* Terminate as per SMTP protocol. Bail out if there's no room left. */
  if (snprintf(buf + len, sizeof(buf) - len, "\r\n") != 2)
  {
    goto error;
  }

  /* Send request, receive response (with a check for OK code). */
  if ((mutt_socket_send(conn, buf) < 0) || smtp_get_resp(conn))
  {
    goto error;
  }

  /* If we got here, auth was successful. */
  return 0;

error:
  mutt_error(_("SASL authentication failed"));
  return -1;
}

/**
 * smtp_auth - Authenticate to an SMTP server
 * @param conn SMTP Connection
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 */
static int smtp_auth(struct Connection *conn)
{
  int r = SMTP_AUTH_UNAVAIL;

  if (C_SmtpAuthenticators)
  {
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &C_SmtpAuthenticators->head, entries)
    {
      mutt_debug(LL_DEBUG2, "Trying method %s\n", np->data);

      if (strcmp(np->data, "oauthbearer") == 0)
      {
        r = smtp_auth_oauth(conn);
      }
      else if (strcmp(np->data, "plain") == 0)
      {
        r = smtp_auth_plain(conn);
      }
      else
      {
#ifdef USE_SASL
        r = smtp_auth_sasl(conn, np->data);
#else
        mutt_error(_("SMTP authentication method %s requires SASL"), np->data);
        continue;
#endif
      }

      if ((r == SMTP_AUTH_FAIL) && (C_SmtpAuthenticators->count > 1))
      {
        mutt_error(_("%s authentication failed, trying next method"), np->data);
      }
      else if (r != SMTP_AUTH_UNAVAIL)
        break;
    }
  }
  else
  {
#ifdef USE_SASL
    r = smtp_auth_sasl(conn, AuthMechs);
#else
    mutt_error(_("SMTP authentication requires SASL"));
    r = SMTP_AUTH_UNAVAIL;
#endif
  }

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

  return (r == SMTP_AUTH_SUCCESS) ? 0 : -1;
}

/**
 * smtp_open - Open an SMTP Connection
 * @param conn  SMTP Connection
 * @param esmtp If true, use ESMTP
 * @retval  0 Success
 * @retval -1 Error
 */
static int smtp_open(struct Connection *conn, bool esmtp)
{
  int rc;

  if (mutt_socket_open(conn))
    return -1;

  /* get greeting string */
  rc = smtp_get_resp(conn);
  if (rc != 0)
    return rc;

  rc = smtp_helo(conn, esmtp);
  if (rc != 0)
    return rc;

#ifdef USE_SSL
  enum QuadOption ans = MUTT_NO;
  if (conn->ssf != 0)
    ans = MUTT_NO;
  else if (C_SslForceTls)
    ans = MUTT_YES;
  else if ((Capabilities & SMTP_CAP_STARTTLS) &&
           ((ans = query_quadoption(C_SslStarttls,
                                    _("Secure connection with TLS?"))) == MUTT_ABORT))
  {
    return -1;
  }

  if (ans == MUTT_YES)
  {
    if (mutt_socket_send(conn, "STARTTLS\r\n") < 0)
      return SMTP_ERR_WRITE;
    rc = smtp_get_resp(conn);
    // Clear any data after the STARTTLS acknowledgement
    mutt_socket_empty(conn);
    if (rc != 0)
      return rc;

    if (mutt_ssl_starttls(conn))
    {
      mutt_error(_("Could not negotiate TLS connection"));
      return -1;
    }

    /* re-EHLO to get authentication mechanisms */
    rc = smtp_helo(conn, esmtp);
    if (rc != 0)
      return rc;
  }
#endif

  if (conn->account.flags & MUTT_ACCT_USER)
  {
    if (!(Capabilities & SMTP_CAP_AUTH))
    {
      mutt_error(_("SMTP server does not support authentication"));
      return -1;
    }

    return smtp_auth(conn);
  }

  return 0;
}

/**
 * mutt_smtp_send - Send a message using SMTP
 * @param from     From Address
 * @param to       To Address
 * @param cc       Cc Address
 * @param bcc      Bcc Address
 * @param msgfile  Message to send to the server
 * @param eightbit If true, try for an 8-bit friendly connection
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_smtp_send(const struct AddressList *from, const struct AddressList *to,
                   const struct AddressList *cc, const struct AddressList *bcc,
                   const char *msgfile, bool eightbit)
{
  struct Connection *conn = NULL;
  struct ConnAccount cac = { { 0 } };
  const char *envfrom = NULL;
  char buf[1024];
  int rc = -1;

  /* it might be better to synthesize an envelope from from user and host
   * but this condition is most likely arrived at accidentally */
  if (C_EnvelopeFromAddress)
    envfrom = C_EnvelopeFromAddress->mailbox;
  else if (from && !TAILQ_EMPTY(from))
    envfrom = TAILQ_FIRST(from)->mailbox;
  else
  {
    mutt_error(_("No from address given"));
    return -1;
  }

  if (smtp_fill_account(&cac) < 0)
    return rc;

  conn = mutt_conn_find(&cac);
  if (!conn)
    return -1;

  do
  {
    /* send our greeting */
    rc = smtp_open(conn, eightbit);
    if (rc != 0)
      break;
    FREE(&AuthMechs);

    /* send the sender's address */
    int len = snprintf(buf, sizeof(buf), "MAIL FROM:<%s>", envfrom);
    if (eightbit && (Capabilities & SMTP_CAP_EIGHTBITMIME))
    {
      mutt_strn_cat(buf, sizeof(buf), " BODY=8BITMIME", 15);
      len += 14;
    }
    if (C_DsnReturn && (Capabilities & SMTP_CAP_DSN))
      len += snprintf(buf + len, sizeof(buf) - len, " RET=%s", C_DsnReturn);
    if ((Capabilities & SMTP_CAP_SMTPUTF8) &&
        (address_uses_unicode(envfrom) || addresses_use_unicode(to) ||
         addresses_use_unicode(cc) || addresses_use_unicode(bcc)))
    {
      snprintf(buf + len, sizeof(buf) - len, " SMTPUTF8");
    }
    mutt_strn_cat(buf, sizeof(buf), "\r\n", 3);
    if (mutt_socket_send(conn, buf) == -1)
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

    mutt_socket_send(conn, "QUIT\r\n");

    rc = 0;
  } while (false);

  mutt_socket_close(conn);
  FREE(&conn);

  if (rc == SMTP_ERR_READ)
    mutt_error(_("SMTP session failed: read error"));
  else if (rc == SMTP_ERR_WRITE)
    mutt_error(_("SMTP session failed: write error"));
  else if (rc == SMTP_ERR_CODE)
    mutt_error(_("Invalid server response"));

  return rc;
}
