/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
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

/* IMAP login/authentication code */

#include "mutt.h"
#include "imap_private.h"
#include "md5.h"

#define MD5_BLOCK_LEN 64
#define MD5_DIGEST_LEN 16

/* external authenticator prototypes */
#ifdef USE_GSS
int imap_auth_gss (IMAP_DATA* idata, const char* user);
#endif

/* forward declarations */
static void hmac_md5 (const char* password, char* challenge,
  unsigned char* response);
static int imap_auth_cram_md5 (IMAP_DATA* idata, const char* user,
  const char* pass);
static int imap_auth_anon (IMAP_DATA *idata);

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
  /* dubious optimisation I saw elsewhere: make the whole string in one call */
  snprintf (obuf, sizeof (obuf),
    "%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    user,
    hmac_response[0], hmac_response[1], hmac_response[2], hmac_response[3],
    hmac_response[4], hmac_response[5], hmac_response[6], hmac_response[7],
    hmac_response[8], hmac_response[9], hmac_response[10], hmac_response[11],
    hmac_response[12], hmac_response[13], hmac_response[14], hmac_response[15]);
  dprint(2, (debugfile, "CRAM response: %s\n", obuf));

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
    if (! (conn->mx.flags & M_IMAP_USER))
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
    }
    else
      strfcpy (user, conn->mx.user, sizeof (user));

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
      if (!(conn->mx.flags & M_IMAP_CRAM))
      {
	if (!ImapCRAMKey)
	{
	  ckey[0] = '\0';
	  snprintf (buf, sizeof (buf), _("CRAM key for %s@%s: "), user,
		    conn->mx.host);
	  if (mutt_get_field (buf, ckey, sizeof (ckey), M_PASS) != 0)
	    return -1;
	}
	else
	  strfcpy (ckey, ImapCRAMKey, sizeof (ckey));
      }
      else
        strfcpy (ckey, conn->mx.pass, sizeof (ckey));

      if (*ckey)
      {
	if ((r = imap_auth_cram_md5 (idata, user, ckey)))
	{
	  mutt_error _("CRAM-MD5 authentication failed.");
	  sleep (1);
	  if (!(conn->mx.flags & M_IMAP_CRAM))
	    FREE (&ImapCRAMKey);
	  conn->mx.flags &= ~M_IMAP_CRAM;
	}
	else
	{
	  strfcpy (conn->mx.pass, ckey, sizeof (conn->mx.pass));
	  conn->mx.flags |= M_IMAP_CRAM;
	  return 0;
	}
      }
      else
      {
	mutt_message _("Skipping CRAM-MD5 authentication.");
	sleep (1);
      }
    }
    else
      dprint (2, (debugfile, "CRAM-MD5 authentication is not available\n"));
        
    if (! (conn->mx.flags & M_IMAP_PASS))
    {
      if (!ImapPass)
      {
	pass[0]=0;
	snprintf (buf, sizeof (buf), _("Password for %s@%s: "), user, conn->mx.host);
	if (mutt_get_field (buf, pass, sizeof (pass), M_PASS) != 0 ||
	    !pass[0])
	{
	  return (-1);
	}
      }
      else
	strfcpy (pass, ImapPass, sizeof (pass));
    }
    else
      strfcpy (pass, conn->mx.pass, sizeof (pass));

    imap_quote_string (q_user, sizeof (q_user), user);
    imap_quote_string (q_pass, sizeof (q_pass), pass);

    mutt_message _("Logging in...");
    snprintf (buf, sizeof (buf), "LOGIN %s %s", q_user, q_pass);
    r = imap_exec (buf, sizeof (buf), idata, buf, IMAP_OK_FAIL);
    if (r == -1)
    {
      /* connection or protocol problem */
      imap_error ("imap_authenticate", buf);
      return (-1);
    }
    else if (r == -2)
    {
      /* Login failed, try again */
      mutt_error _("Login failed.");
      sleep (1);

      if (!(conn->mx.flags & M_IMAP_USER))
	FREE (&ImapUser);
      if (!(conn->mx.flags & M_IMAP_PASS))
	FREE (&ImapPass);
      conn->mx.flags &= ~M_IMAP_PASS;
    }
    else
    {
      /* If they have a successful login, we may as well cache the 
       * user/password. */
      if (!(conn->mx.flags & M_IMAP_USER))
	strfcpy (conn->mx.user, user, sizeof (conn->mx.user));
      if (!(conn->mx.flags & M_IMAP_PASS))
	strfcpy (conn->mx.pass, pass, sizeof (conn->mx.pass));
      
      conn->mx.flags |= (M_IMAP_USER | M_IMAP_PASS);
    }
  }
  return 0;
}
