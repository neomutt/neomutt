/**
 * @file
 * IMAP GSS authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005,2009 Brendan Cully <brendan@kublai.com>
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
 * @page imap_auth_gss IMAP GSS authentication method
 *
 * IMAP GSS authentication method
 *
 * An overview of the authentication method is in RFC1731.
 *
 * An overview of the C API used is in RFC2744.
 * Of note is section 3.2, which describes gss_buffer_desc.
 * The length should not include a terminating '\0' byte, and the client
 * should not expect the value field to be '\0'terminated.
 */

#include "config.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "auth.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#ifdef HAVE_HEIMDAL
#include <gssapi/gssapi.h>
#define gss_nt_service_name GSS_C_NT_HOSTBASED_SERVICE
#else
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#endif

#define GSS_AUTH_P_NONE 1
#define GSS_AUTH_P_INTEGRITY 2
#define GSS_AUTH_P_PRIVACY 4

/**
 * print_gss_error - Print detailed error message to the debug log
 * @param err_maj Error's major number
 * @param err_min Error's minor number
 */
static void print_gss_error(OM_uint32 err_maj, OM_uint32 err_min)
{
  OM_uint32 maj_stat, min_stat;
  OM_uint32 msg_ctx = 0;
  gss_buffer_desc status_string;
  char buf_maj[512];
  char buf_min[512];

  do
  {
    maj_stat = gss_display_status(&min_stat, err_maj, GSS_C_GSS_CODE,
                                  GSS_C_NO_OID, &msg_ctx, &status_string);
    if (GSS_ERROR(maj_stat))
      break;
    size_t status_len = status_string.length;
    if (status_len >= sizeof(buf_maj))
      status_len = sizeof(buf_maj) - 1;
    strncpy(buf_maj, (char *) status_string.value, status_len);
    buf_maj[status_len] = '\0';
    gss_release_buffer(&min_stat, &status_string);

    maj_stat = gss_display_status(&min_stat, err_min, GSS_C_MECH_CODE,
                                  GSS_C_NULL_OID, &msg_ctx, &status_string);
    if (!GSS_ERROR(maj_stat))
    {
      status_len = status_string.length;
      if (status_len >= sizeof(buf_min))
        status_len = sizeof(buf_min) - 1;
      strncpy(buf_min, (char *) status_string.value, status_len);
      buf_min[status_len] = '\0';
      gss_release_buffer(&min_stat, &status_string);
    }
  } while (!GSS_ERROR(maj_stat) && (msg_ctx != 0));

  mutt_debug(LL_DEBUG2, "((%s:%d )(%s:%d))\n", buf_maj, err_maj, buf_min, err_min);
}

/**
 * imap_auth_gss - GSS Authentication support - Implements ImapAuth::authenticate()
 * @param adata Imap Account data
 * @param method Name of this authentication method
 * @retval #ImapAuthRes Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_gss(struct ImapAccountData *adata, const char *method)
{
  gss_buffer_desc request_buf, send_token;
  gss_buffer_t sec_token;
  gss_name_t target_name;
  gss_ctx_id_t context;
  gss_OID mech_name;
  char server_conf_flags;
  gss_qop_t quality;
  int cflags;
  OM_uint32 maj_stat, min_stat;
  unsigned long buf_size;
  int rc, retval = IMAP_AUTH_FAILURE;

  if (!(adata->capabilities & IMAP_CAP_AUTH_GSSAPI))
    return IMAP_AUTH_UNAVAIL;

  if (mutt_account_getuser(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  struct Buffer *buf1 = mutt_buffer_pool_get();
  struct Buffer *buf2 = mutt_buffer_pool_get();

  /* get an IMAP service ticket for the server */
  mutt_buffer_printf(buf1, "imap@%s", adata->conn->account.host);
  request_buf.value = buf1->data;
  request_buf.length = mutt_buffer_len(buf1);

  maj_stat = gss_import_name(&min_stat, &request_buf, gss_nt_service_name, &target_name);
  if (maj_stat != GSS_S_COMPLETE)
  {
    mutt_debug(LL_DEBUG2, "Couldn't get service name for [%s]\n", buf1->data);
    retval = IMAP_AUTH_UNAVAIL;
    goto cleanup;
  }
  else if (C_DebugLevel >= 2)
  {
    gss_display_name(&min_stat, target_name, &request_buf, &mech_name);
    mutt_debug(LL_DEBUG2, "Using service name [%s]\n", (char *) request_buf.value);
    gss_release_buffer(&min_stat, &request_buf);
  }
  /* Acquire initial credentials - without a TGT GSSAPI is UNAVAIL */
  sec_token = GSS_C_NO_BUFFER;
  context = GSS_C_NO_CONTEXT;

  /* build token */
  maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL, &context, target_name,
                                  GSS_C_NO_OID, GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG,
                                  0, GSS_C_NO_CHANNEL_BINDINGS, sec_token, NULL,
                                  &send_token, (unsigned int *) &cflags, NULL);
  if ((maj_stat != GSS_S_COMPLETE) && (maj_stat != GSS_S_CONTINUE_NEEDED))
  {
    print_gss_error(maj_stat, min_stat);
    mutt_debug(LL_DEBUG1, "Error acquiring credentials - no TGT?\n");
    gss_release_name(&min_stat, &target_name);

    retval = IMAP_AUTH_UNAVAIL;
    goto cleanup;
  }

  /* now begin login */
  mutt_message(_("Authenticating (%s)..."), "GSSAPI");

  imap_cmd_start(adata, "AUTHENTICATE GSSAPI");

  /* expect a null continuation response ("+") */
  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

  if (rc != IMAP_RES_RESPOND)
  {
    mutt_debug(LL_DEBUG2, "Invalid response from server: %s\n", buf1->data);
    gss_release_name(&min_stat, &target_name);
    goto bail;
  }

  /* now start the security context initialisation loop... */
  mutt_debug(LL_DEBUG2, "Sending credentials\n");
  mutt_b64_buffer_encode(buf1, send_token.value, send_token.length);
  gss_release_buffer(&min_stat, &send_token);
  mutt_buffer_addstr(buf1, "\r\n");
  mutt_socket_send(adata->conn, mutt_b2s(buf1));

  while (maj_stat == GSS_S_CONTINUE_NEEDED)
  {
    /* Read server data */
    do
    {
      rc = imap_cmd_step(adata);
    } while (rc == IMAP_RES_CONTINUE);

    if (rc != IMAP_RES_RESPOND)
    {
      mutt_debug(LL_DEBUG1, "#1 Error receiving server response\n");
      gss_release_name(&min_stat, &target_name);
      goto bail;
    }

    if (mutt_b64_buffer_decode(buf2, adata->buf + 2) < 0)
    {
      mutt_debug(LL_DEBUG1, "Invalid base64 server response\n");
      gss_release_name(&min_stat, &target_name);
      goto err_abort_cmd;
    }
    request_buf.value = buf2->data;
    request_buf.length = mutt_buffer_len(buf2);
    sec_token = &request_buf;

    /* Write client data */
    maj_stat = gss_init_sec_context(
        &min_stat, GSS_C_NO_CREDENTIAL, &context, target_name, GSS_C_NO_OID,
        GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
        sec_token, NULL, &send_token, (unsigned int *) &cflags, NULL);
    if ((maj_stat != GSS_S_COMPLETE) && (maj_stat != GSS_S_CONTINUE_NEEDED))
    {
      print_gss_error(maj_stat, min_stat);
      mutt_debug(LL_DEBUG1, "Error exchanging credentials\n");
      gss_release_name(&min_stat, &target_name);

      goto err_abort_cmd;
    }
    mutt_b64_buffer_encode(buf1, send_token.value, send_token.length);
    gss_release_buffer(&min_stat, &send_token);
    mutt_buffer_addstr(buf1, "\r\n");
    mutt_socket_send(adata->conn, mutt_b2s(buf1));
  }

  gss_release_name(&min_stat, &target_name);

  /* get security flags and buffer size */
  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

  if (rc != IMAP_RES_RESPOND)
  {
    mutt_debug(LL_DEBUG1, "#2 Error receiving server response\n");
    goto bail;
  }
  if (mutt_b64_buffer_decode(buf2, adata->buf + 2) < 0)
  {
    mutt_debug(LL_DEBUG1, "Invalid base64 server response\n");
    goto err_abort_cmd;
  }
  request_buf.value = buf2->data;
  request_buf.length = mutt_buffer_len(buf2);

  maj_stat = gss_unwrap(&min_stat, context, &request_buf, &send_token, &cflags, &quality);
  if (maj_stat != GSS_S_COMPLETE)
  {
    print_gss_error(maj_stat, min_stat);
    mutt_debug(LL_DEBUG2, "Couldn't unwrap security level data\n");
    gss_release_buffer(&min_stat, &send_token);
    goto err_abort_cmd;
  }
  mutt_debug(LL_DEBUG2, "Credential exchange complete\n");

  /* first octet is security levels supported. We want NONE */
  server_conf_flags = ((char *) send_token.value)[0];
  if (!(((char *) send_token.value)[0] & GSS_AUTH_P_NONE))
  {
    mutt_debug(LL_DEBUG2, "Server requires integrity or privacy\n");
    gss_release_buffer(&min_stat, &send_token);
    goto err_abort_cmd;
  }

  /* we don't care about buffer size if we don't wrap content. But here it is */
  ((char *) send_token.value)[0] = '\0';
  buf_size = ntohl(*((long *) send_token.value));
  gss_release_buffer(&min_stat, &send_token);
  mutt_debug(LL_DEBUG2, "Unwrapped security level flags: %c%c%c\n",
             (server_conf_flags & GSS_AUTH_P_NONE) ? 'N' : '-',
             (server_conf_flags & GSS_AUTH_P_INTEGRITY) ? 'I' : '-',
             (server_conf_flags & GSS_AUTH_P_PRIVACY) ? 'P' : '-');
  mutt_debug(LL_DEBUG2, "Maximum GSS token size is %ld\n", buf_size);

  /* agree to terms (hack!) */
  buf_size = htonl(buf_size); /* not relevant without integrity/privacy */
  mutt_buffer_reset(buf1);
  mutt_buffer_addch(buf1, GSS_AUTH_P_NONE);
  mutt_buffer_addstr_n(buf1, ((char *) &buf_size) + 1, 3);
  /* server decides if principal can log in as user */
  mutt_buffer_addstr(buf1, adata->conn->account.user);
  request_buf.value = buf1->data;
  request_buf.length = mutt_buffer_len(buf1);
  maj_stat = gss_wrap(&min_stat, context, 0, GSS_C_QOP_DEFAULT, &request_buf,
                      &cflags, &send_token);
  if (maj_stat != GSS_S_COMPLETE)
  {
    mutt_debug(LL_DEBUG2, "Error creating login request\n");
    goto err_abort_cmd;
  }

  mutt_b64_buffer_encode(buf1, send_token.value, send_token.length);
  mutt_debug(LL_DEBUG2, "Requesting authorisation as %s\n", adata->conn->account.user);
  mutt_buffer_addstr(buf1, "\r\n");
  mutt_socket_send(adata->conn, mutt_b2s(buf1));

  /* Joy of victory or agony of defeat? */
  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);
  if (rc == IMAP_RES_RESPOND)
  {
    mutt_debug(LL_DEBUG1, "Unexpected server continuation request\n");
    goto err_abort_cmd;
  }
  if (imap_code(adata->buf))
  {
    /* flush the security context */
    mutt_debug(LL_DEBUG2, "Releasing GSS credentials\n");
    maj_stat = gss_delete_sec_context(&min_stat, &context, &send_token);
    if (maj_stat != GSS_S_COMPLETE)
      mutt_debug(LL_DEBUG1, "Error releasing credentials\n");

    /* send_token may contain a notification to the server to flush
     * credentials. RFC1731 doesn't specify what to do, and since this
     * support is only for authentication, we'll assume the server knows
     * enough to flush its own credentials */
    gss_release_buffer(&min_stat, &send_token);

    retval = IMAP_AUTH_SUCCESS;
    goto cleanup;
  }
  else
    goto bail;

err_abort_cmd:
  mutt_socket_send(adata->conn, "*\r\n");
  do
  {
    rc = imap_cmd_step(adata);
  } while (rc == IMAP_RES_CONTINUE);

bail:
  mutt_error(_("GSSAPI authentication failed"));
  retval = IMAP_AUTH_FAILURE;

cleanup:
  mutt_buffer_pool_release(&buf1);
  mutt_buffer_pool_release(&buf2);

  return retval;
}
