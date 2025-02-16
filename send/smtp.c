/**
 * @file
 * Send email to an SMTP server
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Yousef Akbar <yousef@yhakbar.com>
 * Copyright (C) 2021 Ryan Kavanagh <rak@rak.ac>
 * Copyright (C) 2023 Alejandro Colomar <alx@kernel.org>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
 * Copyright (C) 2023 Rayford Shireman
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
 * @page send_smtp Send email to an SMTP server
 *
 * Send email to an SMTP server
 */

/* This file contains code for direct SMTP delivery of email messages. */

#include "config.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "smtp.h"
#include "progress/lib.h"
#include "question/lib.h"
#include "globals.h"
#include "mutt_socket.h"
#include "sendlib.h"
#ifdef USE_SASL_GNU
#include <gsasl.h>
#endif
#ifdef USE_SASL_CYRUS
#include <sasl/sasl.h>
#endif

#define smtp_success(x) (((x) / 100) == 2)
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
typedef uint8_t SmtpCapFlags;          ///< Flags, e.g. #SMTP_CAP_STARTTLS
#define SMTP_CAP_NO_FLAGS           0  ///< No flags are set
#define SMTP_CAP_STARTTLS     (1 << 0) ///< Server supports STARTTLS command
#define SMTP_CAP_AUTH         (1 << 1) ///< Server supports AUTH command
#define SMTP_CAP_DSN          (1 << 2) ///< Server supports Delivery Status Notification
#define SMTP_CAP_EIGHTBITMIME (1 << 3) ///< Server supports 8-bit MIME content
#define SMTP_CAP_SMTPUTF8     (1 << 4) ///< Server accepts UTF-8 strings
#define SMTP_CAP_ALL         ((1 << 5) - 1)
// clang-format on

/**
 * struct SmtpAccountData - Server connection data
 */
struct SmtpAccountData
{
  const char *auth_mechs;    ///< Allowed authorisation mechanisms
  SmtpCapFlags capabilities; ///< Server capabilities
  struct Connection *conn;   ///< Server Connection
  struct ConfigSubset *sub;  ///< Config scope
  const char *fqdn;          ///< Fully-qualified domain name
};

/**
 * struct SmtpAuth - SMTP authentication multiplexor
 */
struct SmtpAuth
{
  /**
   * @defgroup smtp_authenticate SMTP Authentication API
   *
   * authenticate - Authenticate an SMTP connection
   * @param adata  Smtp Account data
   * @param method Use this named method, or any available method if NULL
   * @retval num Result, e.g. #SMTP_AUTH_SUCCESS
   */
  int (*authenticate)(struct SmtpAccountData *adata, const char *method);

  const char *method; ///< Name of authentication method supported, NULL means variable.
      ///< If this is not null, authenticate may ignore the second parameter.
};

/**
 * valid_smtp_code - Is the is a valid SMTP return code?
 * @param[in]  buf String to check
 * @param[out] n   Numeric value of code
 * @retval true Valid number
 */
static bool valid_smtp_code(char *buf, int *n)
{
  return (mutt_str_atoi(buf, n) - buf) <= 3;
}

/**
 * smtp_get_resp - Read a command response from the SMTP server
 * @param adata SMTP Account data
 * @retval  0 Success (2xx code) or continue (354 code)
 * @retval -1 Write error, or any other response code
 */
static int smtp_get_resp(struct SmtpAccountData *adata)
{
  int n;
  char buf[1024] = { 0 };

  do
  {
    n = mutt_socket_readln(buf, sizeof(buf), adata->conn);
    if (n < 4)
    {
      /* read error, or no response code */
      return SMTP_ERR_READ;
    }
    const char *s = buf + 4; /* Skip the response code and the space/dash */
    size_t plen;

    if (mutt_istr_startswith(s, "8BITMIME"))
    {
      adata->capabilities |= SMTP_CAP_EIGHTBITMIME;
    }
    else if ((plen = mutt_istr_startswith(s, "AUTH ")))
    {
      adata->capabilities |= SMTP_CAP_AUTH;
      FREE(&adata->auth_mechs);
      adata->auth_mechs = mutt_str_dup(s + plen);
    }
    else if (mutt_istr_startswith(s, "DSN"))
    {
      adata->capabilities |= SMTP_CAP_DSN;
    }
    else if (mutt_istr_startswith(s, "STARTTLS"))
    {
      adata->capabilities |= SMTP_CAP_STARTTLS;
    }
    else if (mutt_istr_startswith(s, "SMTPUTF8"))
    {
      adata->capabilities |= SMTP_CAP_SMTPUTF8;
    }

    if (!valid_smtp_code(buf, &n))
      return SMTP_ERR_CODE;

  } while (buf[3] == '-');

  if (smtp_success(n) || (n == SMTP_CONTINUE))
    return 0;

  mutt_error(_("SMTP session failed: %s"), buf);
  return -1;
}

/**
 * smtp_rcpt_to - Set the recipient to an Address
 * @param adata SMTP Account data
 * @param al    AddressList to use
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_ERR_WRITE
 */
static int smtp_rcpt_to(struct SmtpAccountData *adata, const struct AddressList *al)
{
  if (!al)
    return 0;

  const char *const c_dsn_notify = cs_subset_string(adata->sub, "dsn_notify");

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    /* weed out group mailboxes, since those are for display only */
    if (!a->mailbox || a->group)
    {
      continue;
    }
    char buf[1024] = { 0 };
    if ((adata->capabilities & SMTP_CAP_DSN) && c_dsn_notify)
    {
      snprintf(buf, sizeof(buf), "RCPT TO:<%s> NOTIFY=%s\r\n",
               buf_string(a->mailbox), c_dsn_notify);
    }
    else
    {
      snprintf(buf, sizeof(buf), "RCPT TO:<%s>\r\n", buf_string(a->mailbox));
    }
    if (mutt_socket_send(adata->conn, buf) == -1)
      return SMTP_ERR_WRITE;
    int rc = smtp_get_resp(adata);
    if (rc != 0)
      return rc;
  }

  return 0;
}

/**
 * smtp_data - Send data to an SMTP server
 * @param adata   SMTP Account data
 * @param msgfile Filename containing data
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_ERR_WRITE
 */
static int smtp_data(struct SmtpAccountData *adata, const char *msgfile)
{
  char buf[1024] = { 0 };
  struct Progress *progress = NULL;
  int rc = SMTP_ERR_WRITE;
  int term = 0;
  size_t buflen = 0;

  FILE *fp = mutt_file_fopen(msgfile, "r");
  if (!fp)
  {
    mutt_error(_("SMTP session failed: unable to open %s"), msgfile);
    return -1;
  }
  const long size = mutt_file_get_size_fp(fp);
  if (size == 0)
  {
    mutt_file_fclose(&fp);
    return -1;
  }
  unlink(msgfile);
  progress = progress_new(MUTT_PROGRESS_NET, size);
  progress_set_message(progress, _("Sending message..."));

  snprintf(buf, sizeof(buf), "DATA\r\n");
  if (mutt_socket_send(adata->conn, buf) == -1)
  {
    mutt_file_fclose(&fp);
    goto done;
  }
  rc = smtp_get_resp(adata);
  if (rc != 0)
  {
    mutt_file_fclose(&fp);
    goto done;
  }

  rc = SMTP_ERR_WRITE;
  while (fgets(buf, sizeof(buf) - 1, fp))
  {
    buflen = mutt_str_len(buf);
    term = buflen && buf[buflen - 1] == '\n';
    if (term && ((buflen == 1) || (buf[buflen - 2] != '\r')))
      snprintf(buf + buflen - 1, sizeof(buf) - buflen + 1, "\r\n");
    if (buf[0] == '.')
    {
      if (mutt_socket_send_d(adata->conn, ".", MUTT_SOCK_LOG_FULL) == -1)
      {
        mutt_file_fclose(&fp);
        goto done;
      }
    }
    if (mutt_socket_send_d(adata->conn, buf, MUTT_SOCK_LOG_FULL) == -1)
    {
      mutt_file_fclose(&fp);
      goto done;
    }
    progress_update(progress, MAX(0, ftell(fp)), -1);
  }
  if (!term && buflen &&
      (mutt_socket_send_d(adata->conn, "\r\n", MUTT_SOCK_LOG_FULL) == -1))
  {
    mutt_file_fclose(&fp);
    goto done;
  }
  mutt_file_fclose(&fp);

  /* terminate the message body */
  if (mutt_socket_send(adata->conn, ".\r\n") == -1)
    goto done;

  rc = smtp_get_resp(adata);

done:
  progress_free(&progress);
  return rc;
}

/**
 * smtp_get_field - Get connection login credentials - Implements ConnAccount::get_field() - @ingroup conn_account_get_field
 */
static const char *smtp_get_field(enum ConnAccountField field, void *gf_data)
{
  struct SmtpAccountData *adata = gf_data;
  if (!adata)
    return NULL;

  switch (field)
  {
    case MUTT_CA_LOGIN:
    case MUTT_CA_USER:
    {
      const char *const c_smtp_user = cs_subset_string(adata->sub, "smtp_user");
      return c_smtp_user;
    }
    case MUTT_CA_PASS:
    {
      const char *const c_smtp_pass = cs_subset_string(adata->sub, "smtp_pass");
      return c_smtp_pass;
    }
    case MUTT_CA_OAUTH_CMD:
    {
      const char *const c_smtp_oauth_refresh_command = cs_subset_string(adata->sub, "smtp_oauth_refresh_command");
      return c_smtp_oauth_refresh_command;
    }
    case MUTT_CA_HOST:
    default:
      return NULL;
  }
}

/**
 * smtp_fill_account - Create ConnAccount object from SMTP Url
 * @param adata SMTP Account data
 * @param cac ConnAccount to populate
 * @retval  0 Success
 * @retval -1 Error
 */
static int smtp_fill_account(struct SmtpAccountData *adata, struct ConnAccount *cac)
{
  cac->flags = 0;
  cac->port = 0;
  cac->type = MUTT_ACCT_TYPE_SMTP;
  cac->service = "smtp";
  cac->get_field = smtp_get_field;
  cac->gf_data = adata;

  const char *const c_smtp_url = cs_subset_string(adata->sub, "smtp_url");

  struct Url *url = url_parse(c_smtp_url);
  if (!url || ((url->scheme != U_SMTP) && (url->scheme != U_SMTPS)) ||
      !url->host || (account_from_url(cac, url) < 0))
  {
    url_free(&url);
    mutt_error(_("Invalid SMTP URL: %s"), c_smtp_url);
    return -1;
  }

  if (url->scheme == U_SMTPS)
    cac->flags |= MUTT_ACCT_SSL;

  if (cac->port == 0)
  {
    if (cac->flags & MUTT_ACCT_SSL)
    {
      cac->port = SMTPS_PORT;
    }
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
 * @param adata SMTP Account data
 * @param esmtp If true, use ESMTP
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_ERR_WRITE
 */
static int smtp_helo(struct SmtpAccountData *adata, bool esmtp)
{
  adata->capabilities = SMTP_CAP_NO_FLAGS;

  if (!esmtp)
  {
    /* if TLS or AUTH are requested, use EHLO */
    if (adata->conn->account.flags & MUTT_ACCT_USER)
      esmtp = true;
#ifdef USE_SSL
    const bool c_ssl_force_tls = cs_subset_bool(adata->sub, "ssl_force_tls");
    const enum QuadOption c_ssl_starttls = cs_subset_quad(adata->sub, "ssl_starttls");

    if (c_ssl_force_tls || (c_ssl_starttls != MUTT_NO))
      esmtp = true;
#endif
  }

  char buf[1024] = { 0 };
  snprintf(buf, sizeof(buf), "%s %s\r\n", esmtp ? "EHLO" : "HELO", adata->fqdn);
  /* XXX there should probably be a wrapper in mutt_socket.c that
   * repeatedly calls adata->conn->write until all data is sent.  This
   * currently doesn't check for a short write.  */
  if (mutt_socket_send(adata->conn, buf) == -1)
    return SMTP_ERR_WRITE;
  return smtp_get_resp(adata);
}

#if defined(USE_SASL_CYRUS) || defined(USE_SASL_GNU)
/**
 * smtp_code - Extract an SMTP return code from a string
 * @param[in]  buf Buffer containing the string to parse
 * @param[out] n   SMTP return code result
 * @retval true Success
 */
static int smtp_code(const struct Buffer *buf, int *n)
{
  if (buf_len(buf) < 3)
    return false;

  char code[4] = { 0 };
  const char *str = buf_string(buf);

  code[0] = str[0];
  code[1] = str[1];
  code[2] = str[2];
  code[3] = 0;

  const char *end = mutt_str_atoi(code, n);
  if (!end || (*end != '\0'))
    return false;
  return true;
}

/**
 * smtp_get_auth_response - Get the SMTP authorisation response
 * @param[in]  conn         Connection to a server
 * @param[in]  input_buf    Temp buffer for strings returned by the server
 * @param[out] smtp_rc      SMTP return code result
 * @param[out] response_buf Text after the SMTP response code
 * @retval  0 Success
 * @retval -1 Error
 *
 * Did the SMTP authorisation succeed?
 */
static int smtp_get_auth_response(struct Connection *conn, struct Buffer *input_buf,
                                  int *smtp_rc, struct Buffer *response_buf)
{
  buf_reset(response_buf);
  do
  {
    if (mutt_socket_buffer_readln(input_buf, conn) < 0)
      return -1;
    if (!smtp_code(input_buf, smtp_rc))
    {
      return -1;
    }

    if (*smtp_rc != SMTP_READY)
      break;

    const char *smtp_response = input_buf->data + 3;
    if (*smtp_response)
    {
      smtp_response++;
      buf_addstr(response_buf, smtp_response);
    }
  } while (input_buf->data[3] == '-');

  return 0;
}
#endif

#ifdef USE_SASL_GNU
/**
 * smtp_auth_gsasl - Authenticate using SASL - Implements SmtpAuth::authenticate() - @ingroup smtp_authenticate
 * @param adata    SMTP Account data
 * @param mechlist List of mechanisms to use
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 */
static int smtp_auth_gsasl(struct SmtpAccountData *adata, const char *mechlist)
{
  Gsasl_session *gsasl_session = NULL;
  struct Buffer *input_buf = NULL, *output_buf = NULL, *smtp_response_buf = NULL;
  int rc = SMTP_AUTH_FAIL, gsasl_rc = GSASL_OK, smtp_rc;

  const char *chosen_mech = mutt_gsasl_get_mech(mechlist, adata->auth_mechs);
  if (!chosen_mech)
  {
    mutt_debug(LL_DEBUG2, "returned no usable mech\n");
    return SMTP_AUTH_UNAVAIL;
  }

  mutt_debug(LL_DEBUG2, "using mech %s\n", chosen_mech);

  if (mutt_gsasl_client_new(adata->conn, chosen_mech, &gsasl_session) < 0)
  {
    mutt_debug(LL_DEBUG1, "Error allocating GSASL connection\n");
    return SMTP_AUTH_UNAVAIL;
  }

  if (OptGui)
    mutt_message(_("Authenticating (%s)..."), chosen_mech);

  input_buf = buf_pool_get();
  output_buf = buf_pool_get();
  smtp_response_buf = buf_pool_get();

  buf_printf(output_buf, "AUTH %s", chosen_mech);

  /* Work around broken SMTP servers. See Debian #1010658.
   * The msmtp source also forces IR for PLAIN because the author
   * encountered difficulties with a server requiring it. */
  if (mutt_str_equal(chosen_mech, "PLAIN"))
  {
    char *gsasl_step_output = NULL;
    gsasl_rc = gsasl_step64(gsasl_session, "", &gsasl_step_output);
    if (gsasl_rc != GSASL_NEEDS_MORE && gsasl_rc != GSASL_OK)
    {
      mutt_debug(LL_DEBUG1, "gsasl_step64() failed (%d): %s\n", gsasl_rc,
                 gsasl_strerror(gsasl_rc));
      goto fail;
    }

    buf_addch(output_buf, ' ');
    buf_addstr(output_buf, gsasl_step_output);
    gsasl_free(gsasl_step_output);
  }

  buf_addstr(output_buf, "\r\n");

  do
  {
    if (mutt_socket_send(adata->conn, buf_string(output_buf)) < 0)
      goto fail;

    if (smtp_get_auth_response(adata->conn, input_buf, &smtp_rc, smtp_response_buf) < 0)
      goto fail;

    if (smtp_rc != SMTP_READY)
      break;

    char *gsasl_step_output = NULL;
    gsasl_rc = gsasl_step64(gsasl_session, buf_string(smtp_response_buf), &gsasl_step_output);
    if ((gsasl_rc == GSASL_NEEDS_MORE) || (gsasl_rc == GSASL_OK))
    {
      buf_strcpy(output_buf, gsasl_step_output);
      buf_addstr(output_buf, "\r\n");
      gsasl_free(gsasl_step_output);
    }
    else
    {
      mutt_debug(LL_DEBUG1, "gsasl_step64() failed (%d): %s\n", gsasl_rc,
                 gsasl_strerror(gsasl_rc));
    }
  } while ((gsasl_rc == GSASL_NEEDS_MORE) || (gsasl_rc == GSASL_OK));

  if (smtp_rc == SMTP_READY)
  {
    mutt_socket_send(adata->conn, "*\r\n");
    goto fail;
  }

  if (smtp_success(smtp_rc) && (gsasl_rc == GSASL_OK))
    rc = SMTP_AUTH_SUCCESS;

fail:
  buf_pool_release(&input_buf);
  buf_pool_release(&output_buf);
  buf_pool_release(&smtp_response_buf);
  mutt_gsasl_client_finish(&gsasl_session);

  if (rc == SMTP_AUTH_FAIL)
    mutt_debug(LL_DEBUG2, "%s failed\n", chosen_mech);

  return rc;
}
#endif

#ifdef USE_SASL_CYRUS
/**
 * smtp_auth_sasl - Authenticate using SASL - Implements SmtpAuth::authenticate() - @ingroup smtp_authenticate
 * @param adata    SMTP Account data
 * @param mechlist List of mechanisms to use
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 */
static int smtp_auth_sasl(struct SmtpAccountData *adata, const char *mechlist)
{
  sasl_conn_t *saslconn = NULL;
  sasl_interact_t *interaction = NULL;
  const char *mech = NULL;
  const char *data = NULL;
  unsigned int data_len = 0;
  struct Buffer *temp_buf = NULL;
  struct Buffer *output_buf = NULL;
  struct Buffer *smtp_response_buf = NULL;
  int rc = SMTP_AUTH_FAIL;
  int rc_sasl;
  int rc_smtp;

  if (mutt_sasl_client_new(adata->conn, &saslconn) < 0)
    return SMTP_AUTH_FAIL;

  do
  {
    rc_sasl = sasl_client_start(saslconn, mechlist, &interaction, &data, &data_len, &mech);
    if (rc_sasl == SASL_INTERACT)
      mutt_sasl_interact(interaction);
  } while (rc_sasl == SASL_INTERACT);

  if ((rc_sasl != SASL_OK) && (rc_sasl != SASL_CONTINUE))
  {
    mutt_debug(LL_DEBUG2, "%s unavailable\n", NONULL(mech));
    sasl_dispose(&saslconn);
    return SMTP_AUTH_UNAVAIL;
  }

  if (OptGui)
  {
    // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
    mutt_message(_("Authenticating (%s)..."), mech);
  }

  temp_buf = buf_pool_get();
  output_buf = buf_pool_get();
  smtp_response_buf = buf_pool_get();

  buf_printf(output_buf, "AUTH %s", mech);
  if (data_len > 0)
  {
    buf_addch(output_buf, ' ');
    mutt_b64_buffer_encode(temp_buf, data, data_len);
    buf_addstr(output_buf, buf_string(temp_buf));
  }
  buf_addstr(output_buf, "\r\n");

  do
  {
    if (mutt_socket_send(adata->conn, buf_string(output_buf)) < 0)
      goto fail;

    if (smtp_get_auth_response(adata->conn, temp_buf, &rc_smtp, smtp_response_buf) < 0)
      goto fail;

    if (rc_smtp != SMTP_READY)
      break;

    if (mutt_b64_buffer_decode(temp_buf, buf_string(smtp_response_buf)) < 0)
    {
      mutt_debug(LL_DEBUG1, "error base64-decoding server response\n");
      goto fail;
    }

    do
    {
      rc_sasl = sasl_client_step(saslconn, buf_string(temp_buf), buf_len(temp_buf),
                                 &interaction, &data, &data_len);
      if (rc_sasl == SASL_INTERACT)
        mutt_sasl_interact(interaction);
    } while (rc_sasl == SASL_INTERACT);

    if (data_len > 0)
      mutt_b64_buffer_encode(output_buf, data, data_len);
    else
      buf_reset(output_buf);

    buf_addstr(output_buf, "\r\n");
  } while (rc_sasl != SASL_FAIL);

  if (smtp_success(rc_smtp))
  {
    mutt_sasl_setup_conn(adata->conn, saslconn);
    rc = SMTP_AUTH_SUCCESS;
  }
  else
  {
    if (rc_smtp == SMTP_READY)
      mutt_socket_send(adata->conn, "*\r\n");
    sasl_dispose(&saslconn);
  }

fail:
  buf_pool_release(&temp_buf);
  buf_pool_release(&output_buf);
  buf_pool_release(&smtp_response_buf);
  return rc;
}
#endif

/**
 * smtp_auth_oauth_xoauth2 - Authenticate an SMTP connection using OAUTHBEARER/XOAUTH2
 * @param adata   SMTP Account data
 * @param method  Authentication method
 * @param xoauth2 Use XOAUTH2 token (if true), OAUTHBEARER token otherwise
 * @retval num Result, e.g. #SMTP_AUTH_SUCCESS
 */
static int smtp_auth_oauth_xoauth2(struct SmtpAccountData *adata, const char *method, bool xoauth2)
{
  /* If they did not explicitly request or configure oauth then fail quietly */
  const char *const c_smtp_oauth_refresh_command = cs_subset_string(NeoMutt->sub, "smtp_oauth_refresh_command");
  if (!method && !c_smtp_oauth_refresh_command)
    return SMTP_AUTH_UNAVAIL;

  const char *authtype = xoauth2 ? "XOAUTH2" : "OAUTHBEARER";

  // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_message(_("Authenticating (%s)..."), authtype);

  /* We get the access token from the smtp_oauth_refresh_command */
  char *oauthbearer = mutt_account_getoauthbearer(&adata->conn->account, xoauth2);
  if (!oauthbearer)
    return SMTP_AUTH_FAIL;

  char *ibuf = NULL;
  mutt_str_asprintf(&ibuf, "AUTH %s %s\r\n", authtype, oauthbearer);

  int rc = mutt_socket_send(adata->conn, ibuf);
  FREE(&oauthbearer);
  FREE(&ibuf);

  if (rc == -1)
    return SMTP_AUTH_FAIL;
  if (smtp_get_resp(adata) != 0)
    return SMTP_AUTH_FAIL;

  return SMTP_AUTH_SUCCESS;
}

/**
 * smtp_auth_oauth - Authenticate an SMTP connection using OAUTHBEARER - Implements SmtpAuth::authenticate() - @ingroup smtp_authenticate
 * @param adata   SMTP Account data
 * @param method  Authentication method (not used)
 * @retval num Result, e.g. #SMTP_AUTH_SUCCESS
 */
static int smtp_auth_oauth(struct SmtpAccountData *adata, const char *method)
{
  return smtp_auth_oauth_xoauth2(adata, method, false);
}

/**
 * smtp_auth_xoauth2 - Authenticate an SMTP connection using XOAUTH2 - Implements SmtpAuth::authenticate() - @ingroup smtp_authenticate
 * @param adata   SMTP Account data
 * @param method  Authentication method (not used)
 * @retval num Result, e.g. #SMTP_AUTH_SUCCESS
 */
static int smtp_auth_xoauth2(struct SmtpAccountData *adata, const char *method)
{
  return smtp_auth_oauth_xoauth2(adata, method, true);
}

/**
 * smtp_auth_plain - Authenticate using plain text - Implements SmtpAuth::authenticate() - @ingroup smtp_authenticate
 * @param adata SMTP Account data
 * @param method     Authentication method (not used)
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 *
 * @note method is "PLAIN"
 */
static int smtp_auth_plain(struct SmtpAccountData *adata, const char *method)
{
  struct Buffer *buf = NULL;
  struct ConnAccount *cac = &adata->conn->account;
  int rc = -1;

  /* Get username and password. Bail out of any can't be retrieved. */
  if ((mutt_account_getuser(cac) < 0) || (mutt_account_getpass(cac) < 0))
    goto error;

  /* Build the initial client response. */
  buf = buf_pool_get();
  mutt_sasl_plain_msg(buf, "AUTH PLAIN", cac->user, cac->user, cac->pass);
  buf_add_printf(buf, "\r\n");

  /* Send request, receive response (with a check for OK code). */
  if ((mutt_socket_send(adata->conn, buf_string(buf)) < 0) || smtp_get_resp(adata))
    goto error;

  rc = 0; // Auth was successful

error:
  if (rc != 0)
  {
    // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
    mutt_error(_("%s authentication failed"), "SASL");
  }
  buf_pool_release(&buf);
  return rc;
}

/**
 * smtp_auth_login - Authenticate using plain text - Implements SmtpAuth::authenticate() - @ingroup smtp_authenticate
 * @param adata  SMTP Account data
 * @param method Authentication method (not used)
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 *
 * @note method is "LOGIN"
 */
static int smtp_auth_login(struct SmtpAccountData *adata, const char *method)
{
  char b64[1024] = { 0 };
  char buf[1026] = { 0 };

  /* Get username and password. Bail out of any can't be retrieved. */
  if ((mutt_account_getuser(&adata->conn->account) < 0) ||
      (mutt_account_getpass(&adata->conn->account) < 0))
  {
    goto error;
  }

  /* Send the AUTH LOGIN request. */
  if (mutt_socket_send(adata->conn, "AUTH LOGIN\r\n") < 0)
  {
    goto error;
  }

  /* Read the 334 VXNlcm5hbWU6 challenge ("Username:" base64-encoded) */
  int rc = mutt_socket_readln_d(buf, sizeof(buf), adata->conn, MUTT_SOCK_LOG_FULL);
  if ((rc < 0) || !mutt_str_equal(buf, "334 VXNlcm5hbWU6"))
  {
    goto error;
  }

  /* Send the username */
  size_t len = snprintf(buf, sizeof(buf), "%s", adata->conn->account.user);
  mutt_b64_encode(buf, len, b64, sizeof(b64));
  snprintf(buf, sizeof(buf), "%s\r\n", b64);
  if (mutt_socket_send(adata->conn, buf) < 0)
  {
    goto error;
  }

  /* Read the 334 UGFzc3dvcmQ6 challenge ("Password:" base64-encoded) */
  rc = mutt_socket_readln_d(buf, sizeof(buf), adata->conn, MUTT_SOCK_LOG_FULL);
  if ((rc < 0) || !mutt_str_equal(buf, "334 UGFzc3dvcmQ6"))
  {
    goto error;
  }

  /* Send the password */
  len = snprintf(buf, sizeof(buf), "%s", adata->conn->account.pass);
  mutt_b64_encode(buf, len, b64, sizeof(b64));
  snprintf(buf, sizeof(buf), "%s\r\n", b64);
  if (mutt_socket_send(adata->conn, buf) < 0)
  {
    goto error;
  }

  /* Check the final response */
  if (smtp_get_resp(adata) < 0)
  {
    goto error;
  }

  /* If we got here, auth was successful. */
  return 0;

error:
  // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_error(_("%s authentication failed"), "LOGIN");
  return -1;
}

/**
 * SmtpAuthenticators - Accepted authentication methods
 */
static const struct SmtpAuth SmtpAuthenticators[] = {
  // clang-format off
  { smtp_auth_oauth, "oauthbearer" },
  { smtp_auth_xoauth2, "xoauth2" },
  { smtp_auth_plain, "plain" },
  { smtp_auth_login, "login" },
#ifdef USE_SASL_CYRUS
  { smtp_auth_sasl, NULL },
#endif
#ifdef USE_SASL_GNU
  { smtp_auth_gsasl, NULL },
#endif
  // clang-format on
};

/**
 * smtp_auth_is_valid - Check if string is a valid smtp authentication method
 * @param authenticator Authenticator string to check
 * @retval true Argument is a valid auth method
 *
 * Validate whether an input string is an accepted smtp authentication method as
 * defined by #SmtpAuthenticators.
 */
bool smtp_auth_is_valid(const char *authenticator)
{
  for (size_t i = 0; i < mutt_array_size(SmtpAuthenticators); i++)
  {
    const struct SmtpAuth *auth = &SmtpAuthenticators[i];
    if (auth->method && mutt_istr_equal(auth->method, authenticator))
      return true;
  }

  return false;
}

/**
 * smtp_authenticate - Authenticate to an SMTP server
 * @param adata SMTP Account data
 * @retval  0 Success
 * @retval <0 Error, e.g. #SMTP_AUTH_FAIL
 */
static int smtp_authenticate(struct SmtpAccountData *adata)
{
  int r = SMTP_AUTH_UNAVAIL;

  const struct Slist *c_smtp_authenticators = cs_subset_slist(adata->sub, "smtp_authenticators");
  if (c_smtp_authenticators && (c_smtp_authenticators->count > 0))
  {
    mutt_debug(LL_DEBUG2, "Trying user-defined smtp_authenticators\n");

    /* Try user-specified list of authentication methods */
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &c_smtp_authenticators->head, entries)
    {
      mutt_debug(LL_DEBUG2, "Trying method %s\n", np->data);

      for (size_t i = 0; i < mutt_array_size(SmtpAuthenticators); i++)
      {
        const struct SmtpAuth *auth = &SmtpAuthenticators[i];
        if (!auth->method || mutt_istr_equal(auth->method, np->data))
        {
          r = auth->authenticate(adata, np->data);
          if (r == SMTP_AUTH_SUCCESS)
            return r;
        }
      }
    }
  }
  else
  {
    /* Fall back to default: any authenticator */
#if defined(USE_SASL_CYRUS)
    mutt_debug(LL_DEBUG2, "Falling back to smtp_auth_sasl, if using sasl\n");
    r = smtp_auth_sasl(adata, adata->auth_mechs);
#elif defined(USE_SASL_GNU)
    mutt_debug(LL_DEBUG2, "Falling back to smtp_auth_gsasl, if using gsasl\n");
    r = smtp_auth_gsasl(adata, adata->auth_mechs);
#else
    mutt_debug(LL_DEBUG2, "Falling back to using any authenticator available\n");
    /* Try all available authentication methods */
    for (size_t i = 0; i < mutt_array_size(SmtpAuthenticators); i++)
    {
      const struct SmtpAuth *auth = &SmtpAuthenticators[i];
      mutt_debug(LL_DEBUG2, "Trying method %s\n", auth->method ? auth->method : "<variable>");
      r = auth->authenticate(adata, auth->method);
      if (r == SMTP_AUTH_SUCCESS)
        return r;
    }
#endif
  }

  if (r != SMTP_AUTH_SUCCESS)
    mutt_account_unsetpass(&adata->conn->account);

  if (r == SMTP_AUTH_FAIL)
  {
    // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
    mutt_error(_("%s authentication failed"), "SASL");
  }
  else if (r == SMTP_AUTH_UNAVAIL)
  {
    mutt_error(_("No authenticators available"));
  }

  return (r == SMTP_AUTH_SUCCESS) ? 0 : -1;
}

/**
 * smtp_open - Open an SMTP Connection
 * @param adata SMTP Account data
 * @param esmtp If true, use ESMTP
 * @retval  0 Success
 * @retval -1 Error
 */
static int smtp_open(struct SmtpAccountData *adata, bool esmtp)
{
  int rc;

  if (mutt_socket_open(adata->conn))
    return -1;

  const bool force_auth = cs_subset_string(adata->sub, "smtp_user");
  esmtp |= force_auth;

  /* get greeting string */
  rc = smtp_get_resp(adata);
  if (rc != 0)
    return rc;

  rc = smtp_helo(adata, esmtp);
  if (rc != 0)
    return rc;

#ifdef USE_SSL
  const bool c_ssl_force_tls = cs_subset_bool(adata->sub, "ssl_force_tls");
  enum QuadOption ans = MUTT_NO;
  if (adata->conn->ssf != 0)
    ans = MUTT_NO;
  else if (c_ssl_force_tls)
    ans = MUTT_YES;
  else if ((adata->capabilities & SMTP_CAP_STARTTLS) &&
           ((ans = query_quadoption(_("Secure connection with TLS?"),
                                    adata->sub, "ssl_starttls")) == MUTT_ABORT))
  {
    return -1;
  }

  if (ans == MUTT_YES)
  {
    if (mutt_socket_send(adata->conn, "STARTTLS\r\n") < 0)
      return SMTP_ERR_WRITE;
    rc = smtp_get_resp(adata);
    // Clear any data after the STARTTLS acknowledgement
    mutt_socket_empty(adata->conn);
    if (rc != 0)
      return rc;

    if (mutt_ssl_starttls(adata->conn))
    {
      mutt_error(_("Could not negotiate TLS connection"));
      return -1;
    }

    /* re-EHLO to get authentication mechanisms */
    rc = smtp_helo(adata, esmtp);
    if (rc != 0)
      return rc;
  }
#endif

  if (force_auth || adata->conn->account.flags & MUTT_ACCT_USER)
  {
    if (!(adata->capabilities & SMTP_CAP_AUTH))
    {
      mutt_error(_("SMTP server does not support authentication"));
      return -1;
    }

    return smtp_authenticate(adata);
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
 * @param sub      Config Subset
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_smtp_send(const struct AddressList *from, const struct AddressList *to,
                   const struct AddressList *cc, const struct AddressList *bcc,
                   const char *msgfile, bool eightbit, struct ConfigSubset *sub)
{
  struct SmtpAccountData adata = { 0 };
  struct ConnAccount cac = { { 0 } };
  const char *envfrom = NULL;
  int rc = -1;

  adata.sub = sub;
  adata.fqdn = mutt_fqdn(false, adata.sub);
  if (!adata.fqdn)
    adata.fqdn = NONULL(ShortHostname);

  const struct Address *c_envelope_from_address = cs_subset_address(adata.sub, "envelope_from_address");

  if (smtp_fill_account(&adata, &cac) < 0)
    return rc;

  adata.conn = mutt_conn_find(&cac);
  if (!adata.conn)
    return -1;

  /* it might be better to synthesize an envelope from from user and host
   * but this condition is most likely arrived at accidentally */
  if (c_envelope_from_address)
  {
    envfrom = buf_string(c_envelope_from_address->mailbox);
  }
  else if (from && !TAILQ_EMPTY(from))
  {
    envfrom = buf_string(TAILQ_FIRST(from)->mailbox);
  }
  else
  {
    mutt_error(_("No from address given"));
    mutt_socket_close(adata.conn);
    return -1;
  }

  const char *const c_dsn_return = cs_subset_string(adata.sub, "dsn_return");

  struct Buffer *buf = buf_pool_get();
  do
  {
    /* send our greeting */
    rc = smtp_open(&adata, eightbit);
    if (rc != 0)
      break;
    FREE(&adata.auth_mechs);

    /* send the sender's address */
    buf_printf(buf, "MAIL FROM:<%s>", envfrom);
    if (eightbit && (adata.capabilities & SMTP_CAP_EIGHTBITMIME))
      buf_addstr(buf, " BODY=8BITMIME");

    if (c_dsn_return && (adata.capabilities & SMTP_CAP_DSN))
      buf_add_printf(buf, " RET=%s", c_dsn_return);

    if ((adata.capabilities & SMTP_CAP_SMTPUTF8) &&
        (mutt_addr_uses_unicode(envfrom) || mutt_addrlist_uses_unicode(to) ||
         mutt_addrlist_uses_unicode(cc) || mutt_addrlist_uses_unicode(bcc)))
    {
      buf_addstr(buf, " SMTPUTF8");
    }
    buf_addstr(buf, "\r\n");
    if (mutt_socket_send(adata.conn, buf_string(buf)) == -1)
    {
      rc = SMTP_ERR_WRITE;
      break;
    }
    rc = smtp_get_resp(&adata);
    if (rc != 0)
      break;

    /* send the recipient list */
    if ((rc = smtp_rcpt_to(&adata, to)) || (rc = smtp_rcpt_to(&adata, cc)) ||
        (rc = smtp_rcpt_to(&adata, bcc)))
    {
      break;
    }

    /* send the message data */
    rc = smtp_data(&adata, msgfile);
    if (rc != 0)
      break;

    mutt_socket_send(adata.conn, "QUIT\r\n");

    rc = 0;
  } while (false);

  mutt_socket_close(adata.conn);
  FREE(&adata.conn);
  FREE(&adata.auth_mechs);

  if (rc == SMTP_ERR_READ)
    mutt_error(_("SMTP session failed: read error"));
  else if (rc == SMTP_ERR_WRITE)
    mutt_error(_("SMTP session failed: write error"));
  else if (rc == SMTP_ERR_CODE)
    mutt_error(_("Invalid server response"));

  buf_pool_release(&buf);
  return rc;
}
