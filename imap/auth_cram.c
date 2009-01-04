/*
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/* IMAP login/authentication code */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "imap_private.h"
#include "auth.h"
#include "md5.h"

#define MD5_BLOCK_LEN 64
#define MD5_DIGEST_LEN 16

/* forward declarations */
static void hmac_md5 (const char* password, char* challenge,
  unsigned char* response);

/* imap_auth_cram_md5: AUTH=CRAM-MD5 support. */
imap_auth_res_t imap_auth_cram_md5 (IMAP_DATA* idata, const char* method)
{
  char ibuf[LONG_STRING*2], obuf[LONG_STRING];
  unsigned char hmac_response[MD5_DIGEST_LEN];
  int len;
  int rc;

  if (!mutt_bit_isset (idata->capabilities, ACRAM_MD5))
    return IMAP_AUTH_UNAVAIL;

  mutt_message _("Authenticating (CRAM-MD5)...");

  /* get auth info */
  if (mutt_account_getlogin (&idata->conn->account))
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass (&idata->conn->account))
    return IMAP_AUTH_FAILURE;

  imap_cmd_start (idata, "AUTHENTICATE CRAM-MD5");

  /* From RFC 2195:
   * The data encoded in the first ready response contains a presumptively
   * arbitrary string of random digits, a timestamp, and the fully-qualified
   * primary host name of the server. The syntax of the unencoded form must
   * correspond to that of an RFC 822 'msg-id' [RFC822] as described in [POP3].
   */
  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);
  
  if (rc != IMAP_CMD_RESPOND)
  {
    dprint (1, (debugfile, "Invalid response from server: %s\n", ibuf));
    goto bail;
  }

  if ((len = mutt_from_base64 (obuf, idata->buf + 2)) == -1)
  {
    dprint (1, (debugfile, "Error decoding base64 response.\n"));
    goto bail;
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
  hmac_md5 (idata->conn->account.pass, obuf, hmac_response);
  /* dubious optimisation I saw elsewhere: make the whole string in one call */
  snprintf (obuf, sizeof (obuf),
    "%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    idata->conn->account.user,
    hmac_response[0], hmac_response[1], hmac_response[2], hmac_response[3],
    hmac_response[4], hmac_response[5], hmac_response[6], hmac_response[7],
    hmac_response[8], hmac_response[9], hmac_response[10], hmac_response[11],
    hmac_response[12], hmac_response[13], hmac_response[14], hmac_response[15]);
  dprint(2, (debugfile, "CRAM response: %s\n", obuf));

  /* XXX - ibuf must be long enough to store the base64 encoding of obuf, 
   * plus the additional debris
   */
  
  mutt_to_base64 ((unsigned char*) ibuf, (unsigned char*) obuf, strlen (obuf),
		  sizeof (ibuf) - 2);
  safe_strcat (ibuf, sizeof (ibuf), "\r\n");
  mutt_socket_write (idata->conn, ibuf);

  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_OK)
  {
    dprint (1, (debugfile, "Error receiving server response.\n"));
    goto bail;
  }

  if (imap_code (idata->buf))
    return IMAP_AUTH_SUCCESS;

 bail:
  mutt_error _("CRAM-MD5 authentication failed.");
  mutt_sleep (2);
  return IMAP_AUTH_FAILURE;
}

/* hmac_md5: produce CRAM-MD5 challenge response. */
static void hmac_md5 (const char* password, char* challenge,
  unsigned char* response)
{
  struct md5_ctx ctx;
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
    md5_buffer (password, secret_len, hash_passwd);
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
  md5_init_ctx (&ctx);
  md5_process_bytes (ipad, MD5_BLOCK_LEN, &ctx);
  md5_process_bytes (challenge, chal_len, &ctx);
  md5_finish_ctx (&ctx, response);

  /* outer hash: inner hash and opadded secret */
  md5_init_ctx (&ctx);
  md5_process_bytes (opad, MD5_BLOCK_LEN, &ctx);
  md5_process_bytes (response, MD5_DIGEST_LEN, &ctx);
  md5_finish_ctx (&ctx, response);
}
