/*
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
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

/* GSS login/authentication code */

/* for HAVE_HEIMDAL */
#include "config.h"

#include "mutt.h"
#include "imap_private.h"

#include <netinet/in.h>

#ifdef HAVE_HEIMDAL
#  include <gssapi.h>
#else
#  include <gssapi/gssapi.h>
#  include <gssapi/gssapi_generic.h>
#endif

#define GSS_BUFSIZE 8192

#define GSS_AUTH_P_NONE      1
#define GSS_AUTH_P_INTEGRITY 2
#define GSS_AUTH_P_PRIVACY   4

/* imap_auth_gss: AUTH=GSSAPI support. Used unconditionally if the server
 *   supports it */
int imap_auth_gss (IMAP_DATA* idata, const char* user)
{
  gss_buffer_desc request_buf, send_token;
  gss_buffer_t sec_token;
  gss_name_t target_name;
  gss_ctx_id_t context;
  gss_OID mech_name;
  gss_qop_t quality;
  int cflags;
  OM_uint32 maj_stat, min_stat;
  char buf1[GSS_BUFSIZE], buf2[GSS_BUFSIZE], server_conf_flags;
  unsigned long buf_size;

  dprint (2, (debugfile, "Attempting GSS login...\n"));

  /* get an IMAP service ticket for the server */
  snprintf (buf1, sizeof (buf1), "imap@%s", idata->conn->account.host);
  request_buf.value = buf1;
  request_buf.length = strlen (buf1) + 1;
  maj_stat = gss_import_name (&min_stat, &request_buf, gss_nt_service_name,
    &target_name);
  if (maj_stat != GSS_S_COMPLETE)
  {
    dprint (2, (debugfile, "Couldn't get service name for [%s]\n", buf1));
    return -1;
  }
#ifdef DEBUG	
  else if (debuglevel >= 2)
  {
    maj_stat = gss_display_name (&min_stat, target_name, &request_buf,
      &mech_name);
    dprint (2, (debugfile, "Using service name [%s]\n",
      (char*) request_buf.value));
    maj_stat = gss_release_buffer (&min_stat, &request_buf);
  }
#endif
  /* now begin login */
  mutt_message _("Authenticating (GSSAPI)...");

  imap_cmd_start (idata, "AUTHENTICATE GSSAPI");

  /* expect a null continuation response ("+") */
  if (mutt_socket_readln (buf1, sizeof (buf1), idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));
    gss_release_name (&min_stat, &target_name);

    return -1;
  }

  if (buf1[0] != '+')
  {
    dprint (2, (debugfile, "Invalid response from server: %s\n", buf1));
    gss_release_name (&min_stat, &target_name);

    return -1;
  }

  /* now start the security context initialisation loop... */
  dprint (2, (debugfile, "Sending credentials\n"));
  sec_token = GSS_C_NO_BUFFER;
  context = GSS_C_NO_CONTEXT;
  do
  {
    /* build token */
    maj_stat = gss_init_sec_context (&min_stat, 
				     GSS_C_NO_CREDENTIAL, 
				     &context,
				     target_name, 
				     GSS_C_NO_OID,
				     GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG,
				     0, 
				     GSS_C_NO_CHANNEL_BINDINGS, 
				     sec_token, 
				     NULL, 
				     &send_token,
				     (unsigned int*) &cflags, 
				     NULL);
    if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
    {
      dprint (1, (debugfile, "Error exchanging credentials\n"));
      gss_release_name (&min_stat, &target_name);
      /* end authentication attempt */
      mutt_socket_write (idata->conn, "*\r\n");
      mutt_socket_readln (buf1, sizeof (buf1), idata->conn);

      return -1;
    }

    /* send token */
    mutt_to_base64 ((unsigned char*) buf1, send_token.value,
      send_token.length);
    gss_release_buffer (&min_stat, &send_token);
    strcpy (buf1 + strlen (buf1), "\r\n");
    mutt_socket_write (idata->conn, buf1);

    if (maj_stat == GSS_S_CONTINUE_NEEDED)
    {
      if (mutt_socket_readln (buf1, sizeof (buf1), idata->conn) < 0)
      {
        dprint (1, (debugfile, "Error receiving server response.\n"));
        gss_release_name (&min_stat, &target_name);

        return -1;
      }

      request_buf.length = mutt_from_base64 (buf2, buf1 + 2);
      request_buf.value = buf2;
      sec_token = &request_buf;
    }
  }
  while (maj_stat == GSS_S_CONTINUE_NEEDED);

  gss_release_name (&min_stat, &target_name);

  /* get security flags and buffer size */
  if (mutt_socket_readln (buf1, sizeof (buf1), idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

    return -1;
  }
  request_buf.length = mutt_from_base64 (buf2, buf1 + 2);
  request_buf.value = buf2;

  maj_stat = gss_unwrap (&min_stat, context, &request_buf, &send_token,
    &cflags, &quality);
  if (maj_stat != GSS_S_COMPLETE)
  {
    dprint (2, (debugfile, "Couldn't unwrap security level data\n"));
    gss_release_buffer (&min_stat, &send_token);

    mutt_socket_write(idata->conn, "*\r\n");
    return -1;
  }
  dprint (2, (debugfile, "Credential exchange complete\n"));

  /* first octet is security levels supported. We want NONE */
  server_conf_flags = ((char*) send_token.value)[0];
  if ( !(((char*) send_token.value)[0] & GSS_AUTH_P_NONE) )
  {
    dprint (2, (debugfile, "Server requires integrity or privace\n"));
    gss_release_buffer (&min_stat, &send_token);

    mutt_socket_write(idata->conn, "*\r\n");
    return -1;
  }

  /* we don't care about buffer size if we don't wrap content. But here it is */
  ((char*) send_token.value)[0] = 0;
  buf_size = ntohl (*((long *) send_token.value));
  gss_release_buffer (&min_stat, &send_token);
  dprint (2, (debugfile, "Unwrapped security level flags: %c%c%c\n",
    server_conf_flags & GSS_AUTH_P_NONE      ? 'N' : '-',
    server_conf_flags & GSS_AUTH_P_INTEGRITY ? 'I' : '-',
    server_conf_flags & GSS_AUTH_P_PRIVACY   ? 'P' : '-'));
  dprint (2, (debugfile, "Maximum GSS token size is %ld\n", buf_size));

  /* agree to terms (hack!) */
  buf_size = htonl (buf_size); /* not relevant without integrity/privacy */
  memcpy (buf1, &buf_size, 4);
  buf1[0] = GSS_AUTH_P_NONE;
  /* server decides if principal can log in as user */
  strncpy (buf1 + 4, user, sizeof (buf1) - 4);
  request_buf.value = buf1;
  request_buf.length = 4 + strlen (user) + 1;
  maj_stat = gss_wrap (&min_stat, context, 0, GSS_C_QOP_DEFAULT, &request_buf,
    &cflags, &send_token);
  if (maj_stat != GSS_S_COMPLETE)
  {
    dprint (2, (debugfile, "Error creating login request\n"));

    mutt_socket_write(idata->conn, "*\r\n");
    return -1;
  }

  mutt_to_base64 ((unsigned char*) buf1, send_token.value, send_token.length);
  dprint (2, (debugfile, "Requesting authorisation as %s\n", user));
  strncat (buf1, "\r\n", sizeof (buf1));
  mutt_socket_write (idata->conn, buf1);

  /* Joy of victory or agony of defeat? */
  if (mutt_socket_readln (buf1, GSS_BUFSIZE, idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

    mutt_socket_write(idata->conn, "*\r\n");
    return -1;
  }
  if (imap_code (buf1))
  {
    /* flush the security context */
    dprint (2, (debugfile, "Releasing GSS credentials\n"));
    maj_stat = gss_delete_sec_context (&min_stat, &context, &send_token);
    if (maj_stat != GSS_S_COMPLETE)
    {
      dprint (1, (debugfile, "Error releasing credentials\n"));

      return -1;
    }
    /* send_token may contain a notification to the server to flush
     * credentials. RFC 1731 doesn't specify what to do, and since this
     * support is only for authentication, we'll assume the server knows
     * enough to flush its own credentials */
    gss_release_buffer (&min_stat, &send_token);

    dprint (2, (debugfile, "GSS login complete\n"));

    return 0;
  }

  /* logon failed */
  dprint (2, (debugfile, "GSS login failed.\n"));

  return -1;
}
