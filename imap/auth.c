/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/* IMAP login/authentication code */

#include "mutt.h"
#include "imap_private.h"
#include "md5.h"

#define MD5_BLOCK_LEN 64
#define MD5_DIGEST_LEN 16

#ifdef USE_GSS
#include <netinet/in.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>

#define GSS_BUFSIZE 8192

#define GSS_AUTH_P_NONE      1
#define GSS_AUTH_P_INTEGRITY 2
#define GSS_AUTH_P_PRIVACY   4
#endif

/* forward declarations */
static void hmac_md5 (const char* password, char* challenge,
  unsigned char* response);
static int imap_auth_cram_md5 (IMAP_DATA* idata, const char* user,
  const char* pass);
static int imap_auth_anon (IMAP_DATA *idata);
#ifdef USE_GSS
static int imap_auth_gss (IMAP_DATA* idata, const char* user);
#endif

/* hmac_md5: produce CRAM-MD5 challenge response. */
static void hmac_md5 (const char* password, char* challenge,
  unsigned char* response)
{
  MD5_CTX ctx;
  unsigned char ipad[MD5_BLOCK_LEN], opad[MD5_BLOCK_LEN];
  unsigned char secret[MD5_BLOCK_LEN+1];
  unsigned char hash_passwd[MD5_DIGEST_LEN];
  unsigned int secret_len, chal_len;
  int i;

  secret_len = strlen (password);
  chal_len = strlen (challenge);

  /* passwords longer than MD5_BLOCK_LEN bytes are substituted with their MD5
   * digests */
  if (secret_len > MD5_BLOCK_LEN)
  {
    MD5Init (&ctx);
    MD5Update (&ctx, (unsigned char*) password, secret_len);
    MD5Final (hash_passwd, &ctx);
    strfcpy ((char*) secret, (char*) hash_passwd, MD5_DIGEST_LEN);
    secret_len = MD5_DIGEST_LEN;
  }
  else
    strfcpy ((char *) secret, password, sizeof (secret));

  memset (ipad, 0, sizeof (ipad));
  memset (opad, 0, sizeof (opad));
  memcpy (ipad, secret, secret_len);
  memcpy (opad, secret, secret_len);

  for (i = 0; i < MD5_BLOCK_LEN; i++)
  {
    ipad[i] ^= 0x36;
    opad[i] ^= 0x5c;
  }

  /* inner hash: challenge and ipadded secret */
  MD5Init (&ctx);
  MD5Update (&ctx, ipad, MD5_BLOCK_LEN);
  MD5Update (&ctx, (unsigned char*) challenge, chal_len);
  MD5Final (response, &ctx);

  /* outer hash: inner hash and opadded secret */
  MD5Init (&ctx);
  MD5Update (&ctx, opad, MD5_BLOCK_LEN);
  MD5Update (&ctx, response, MD5_DIGEST_LEN);
  MD5Final (response, &ctx);
}

#ifdef USE_GSS
/* imap_auth_gss: AUTH=GSSAPI support. Used unconditionally if the server
 *   supports it */
static int imap_auth_gss (IMAP_DATA* idata, const char* user)
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

  char seq[16];

  dprint (2, (debugfile, "Attempting GSS login...\n"));

  /* get an IMAP service ticket for the server */
  snprintf (buf1, sizeof (buf1), "imap@%s", idata->conn->server);
  request_buf.value = buf1;
  request_buf.length = strlen (buf1) + 1;
  maj_stat = gss_import_name (&min_stat, &request_buf, gss_nt_service_name,
    &target_name);
  if (maj_stat != GSS_S_COMPLETE)
  {
    dprint (2, (debugfile, "Couldn't get service name for [%s]\n", buf1));
    return -1;
  }
  else if (debuglevel >= 2)
  {
    maj_stat = gss_display_name (&min_stat, target_name, &request_buf,
      &mech_name);
    dprint (2, (debugfile, "Using service name [%s]\n",
      (char*) request_buf.value));
    maj_stat = gss_release_buffer (&min_stat, &request_buf);
  }

  /* now begin login */
  mutt_message _("Authenticating (GSSAPI)...");
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf1, sizeof (buf1), "%s AUTHENTICATE GSSAPI\r\n", seq);
  mutt_socket_write (idata->conn, buf1);

  /* expect a null continuation response ("+") */
  if (mutt_socket_read_line_d (buf1, sizeof (buf1), idata->conn) < 0)
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
    maj_stat = gss_init_sec_context (&min_stat, GSS_C_NO_CREDENTIAL, &context,
      target_name, NULL, 0, 0, NULL, sec_token, NULL, &send_token,
      (unsigned int*) &cflags, NULL);
    if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
    {
      dprint (1, (debugfile, "Error exchanging credentials\n"));
      gss_release_name (&min_stat, &target_name);
      /* end authentication attempt */
      mutt_socket_write (idata->conn, "*\r\n");
      mutt_socket_read_line_d (buf1, sizeof (buf1), idata->conn);

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
      if (mutt_socket_read_line_d (buf1, sizeof (buf1), idata->conn) < 0)
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
  if (mutt_socket_read_line_d (buf1, sizeof (buf1), idata->conn) < 0)
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

    return -1;
  }
  dprint (2, (debugfile, "Credential exchange complete\n"));

  /* first octet is security levels supported. We want NONE */
  server_conf_flags = ((char*) send_token.value)[0];
  if ( !(((char*) send_token.value)[0] & GSS_AUTH_P_NONE) )
  {
    dprint (2, (debugfile, "Server requires integrity or privace\n"));
    gss_release_buffer (&min_stat, &send_token);

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

    return -1;
  }

  mutt_to_base64 ((unsigned char*) buf1, send_token.value, send_token.length);
  dprint (2, (debugfile, "Requesting authorisation as %s\n", user));
  strncat (buf1, "\r\n", sizeof (buf1));
  mutt_socket_write (idata->conn, buf1);

  /* Joy of victory or agony of defeat? */
  if (mutt_socket_read_line_d (buf1, GSS_BUFSIZE, idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

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
#endif

/* imap_auth_cram_md5: AUTH=CRAM-MD5 support. Used unconditionally if the
 *   server supports it */
static int imap_auth_cram_md5 (IMAP_DATA* idata, const char* user,
  const char* pass)
{
  char ibuf[LONG_STRING], obuf[LONG_STRING];
  unsigned char hmac_response[MD5_DIGEST_LEN];
  int len;
  char seq[16];

  dprint (2, (debugfile, "Attempting CRAM-MD5 login...\n"));
  mutt_message _("Authenticating (CRAM-MD5)...");
  imap_make_sequence (seq, sizeof (seq));
  snprintf (obuf, LONG_STRING, "%s AUTHENTICATE CRAM-MD5\r\n", seq);
  mutt_socket_write (idata->conn, obuf);

  /* From RFC 2195:
   * The data encoded in the first ready response contains a presumptively
   * arbitrary string of random digits, a timestamp, and the fully-qualified
   * primary host name of the server. The syntax of the unencoded form must
   * correspond to that of an RFC 822 'msg-id' [RFC822] as described in [POP3].
   */
  if (mutt_socket_read_line_d (ibuf, LONG_STRING, idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

    return -1;
  }

  if (ibuf[0] != '+')
  {
    dprint (1, (debugfile, "Invalid response from server: %s\n", ibuf));

    return -1;
  }

  if ((len = mutt_from_base64 (obuf, ibuf + 2)) == -1)
  {
    dprint (1, (debugfile, "Error decoding base64 response.\n"));

    return -1;
  }

  obuf[len] = '\0';
  dprint (2, (debugfile, "CRAM challenge: %s\n", obuf));

  /* The client makes note of the data and then responds with a string
   * consisting of the user name, a space, and a 'digest'. The latter is
   * computed by applying the keyed MD5 algorithm from [KEYED-MD5] where the
   * key is a shared secret and the digested text is the timestamp (including
   * angle-brackets).
   * 
   * Note: The user name shouldn't be quoted. Since the digest can't contain
   *   spaces, there is no ambiguity. Some servers get this wrong, we'll work
   *   around them when the bug report comes in. Until then, we'll remain
   *   blissfully RFC-compliant.
   */
   hmac_md5 (pass, obuf, hmac_response);
   dprint (2, (debugfile, "CRAM response: %s,[%s]->", obuf, pass));
   /* dubious optimisation I saw elsewhere: make the whole string in one call */
  snprintf (obuf, sizeof (obuf),
    "%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    user,
    hmac_response[0], hmac_response[1], hmac_response[2], hmac_response[3],
    hmac_response[4], hmac_response[5], hmac_response[6], hmac_response[7],
    hmac_response[8], hmac_response[9], hmac_response[10], hmac_response[11],
    hmac_response[12], hmac_response[13], hmac_response[14], hmac_response[15]);
  dprint(2, (debugfile, "%s\n", obuf));

  mutt_to_base64 ((unsigned char*) ibuf, (unsigned char*) obuf, strlen (obuf));
  strcpy (ibuf + strlen (ibuf), "\r\n");
  mutt_socket_write (idata->conn, ibuf);

  if (mutt_socket_read_line_d (ibuf, LONG_STRING, idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

    return -1;
  }

  if (imap_code (ibuf))
  {
    dprint (2, (debugfile, "CRAM login complete.\n"));

    return 0;
  }

  dprint (2, (debugfile, "CRAM login failed.\n"));
  return -1;
}

/* this is basically a stripped-down version of the cram-md5 method. */

static int imap_auth_anon (IMAP_DATA* idata)
{
  char ibuf[LONG_STRING], obuf[LONG_STRING];
  char seq[16];

  dprint (2, (debugfile, "Attempting anonymous login...\n"));
  mutt_message _("Authenticating (anonymous)...");
  imap_make_sequence (seq, sizeof (seq));
  snprintf (obuf, LONG_STRING, "%s AUTHENTICATE ANONYMOUS\r\n", seq);
  mutt_socket_write (idata->conn, obuf);

  if (mutt_socket_read_line_d (ibuf, LONG_STRING, idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

    return -1;
  }

  if (ibuf[0] != '+')
  {
    dprint (1, (debugfile, "Invalid response from server: %s\n", ibuf));

    return -1;
  }

  strfcpy (ibuf, "ZHVtbXkK\r\n", sizeof (ibuf)); 	/* base64 ("dummy") */

  mutt_socket_write (idata->conn, ibuf);

  if (mutt_socket_read_line_d (ibuf, LONG_STRING, idata->conn) < 0)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));

    return -1;
  }

  if (imap_code (ibuf))
  {
    dprint (2, (debugfile, "Anonymous login complete.\n"));

    return 0;
  }

  dprint (2, (debugfile, "Anonymous login failed.\n"));
  return -1;
}



/* imap_authenticate: loop until success or user abort. At each loop, all
 *   supported authentication methods are tried, from strongest to weakest.
 *   Currently available:
 *     GSSAPI: strongest available form, requires Kerberos V infrastructure,
 *       or possibly alternatively Heimdal.
 *     CRAM-MD5: like APOP or CHAP. Safe against replay and sniffing, but
 *       requires that your key be stored on the server, readable by the
 *       server account. UW-IMAP supports this method since at least 4.5, if
 *       the key file exists on the server.
 *     LOGIN: ugh. Don't use this if you can help it. Uses cleartext password
 *       exchange, furthermore uses unix login techniques so this same password
 *       can be used to log in to the server or others that share the
 *       credentials database.
 *   Unavailable:
 *     KERBEROS_V4. Superceded by GSSAPI.
 */
int imap_authenticate (IMAP_DATA *idata, CONNECTION *conn)
{
  char buf[LONG_STRING];
  char user[SHORT_STRING], q_user[SHORT_STRING];
  char ckey[SHORT_STRING];
  char pass[SHORT_STRING], q_pass[SHORT_STRING];

  int r = 1;

  while (r != 0)
  {
    if (!ImapUser)
    {
      strfcpy (user, NONULL(Username), sizeof (user));
      if (mutt_get_field (_("IMAP Username: "), user, sizeof (user), 0) != 0)
      {
	user[0] = 0;
	return (-1);
      }
    }
    else
      strfcpy (user, ImapUser, sizeof (user));

    if (!user[0])
    {
      if (!mutt_bit_isset (idata->capabilities, AUTH_ANON))
      {
	mutt_error _("Anonymous authentication not supported.");
	return -1;
      }
      
      return imap_auth_anon (idata);
    }
    
#ifdef USE_GSS
    /* attempt GSSAPI authentication, if available */
    if (mutt_bit_isset (idata->capabilities, AGSSAPI))
    {
      if ((r = imap_auth_gss (idata, user)))
      {
        mutt_error _("GSSAPI authentication failed.");
        sleep (1);
      }
      else
        return 0;
    }
    else
      dprint (2, (debugfile, "GSSAPI authentication is not available\n"));
#endif

    /* attempt CRAM-MD5 if available */
    if (mutt_bit_isset (idata->capabilities, ACRAM_MD5))
    {
      if (!ImapCRAMKey)
      {
        ckey[0] = '\0';
        snprintf (buf, sizeof (buf), _("CRAM key for %s@%s: "), user,
          conn->server);
        if (mutt_get_field (buf, ckey, sizeof (ckey), M_PASS) != 0)
          return -1;
      }
      else
        strfcpy (ckey, ImapCRAMKey, sizeof (ckey));

      if (*ckey)
      {
	if ((r = imap_auth_cram_md5 (idata, user, ckey)))
	{
	  mutt_error _("CRAM-MD5 authentication failed.");
	  sleep (1);
	}
	else
	  return 0;
      }
      else
      {
	mutt_message _("Skipping CRAM-MD5 authentication.");
	sleep (1);
      }
    }
    else
      dprint (2, (debugfile, "CRAM-MD5 authentication is not available\n"));
        
    if (!ImapPass)
    {
      pass[0]=0;
      snprintf (buf, sizeof (buf), _("Password for %s@%s: "), user, conn->server);
      if (mutt_get_field (buf, pass, sizeof (pass), M_PASS) != 0 ||
	  !pass[0])
      {
	return (-1);
      }
    }
    else
      strfcpy (pass, ImapPass, sizeof (pass));

    imap_quote_string (q_user, sizeof (q_user), user);
    imap_quote_string (q_pass, sizeof (q_pass), pass);

    mutt_message _("Logging in...");
    snprintf (buf, sizeof (buf), "LOGIN %s %s", q_user, q_pass);
    r = imap_exec (buf, sizeof (buf), idata, buf, IMAP_OK_FAIL);
    if (r == -1)
    {
      /* connection or protocol problem */
      imap_error ("imap_open_connection()", buf);
      return (-1);
    }
    else if (r == -2)
    {
      /* Login failed, try again */
      mutt_error _("Login failed.");
      sleep (1);

      FREE (&ImapUser);
      FREE (&ImapPass);
    }
    else
    {
      /* If they have a successful login, we may as well cache the 
       * user/password. */
      if (!ImapUser)
	ImapUser = safe_strdup (user);
      if (!ImapPass)
	ImapPass = safe_strdup (pass);
    }
  }
  return 0;
}
