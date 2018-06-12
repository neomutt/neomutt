/**
 * @file
 * IMAP CRAM-MD5 authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
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
 * @page imap_auth_cram IMAP CRAM-MD5 authentication method
 *
 * IMAP CRAM-MD5 authentication method
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "auth.h"
#include "globals.h"
#include "mutt_account.h"
#include "mutt_socket.h"
#include "options.h"
#include "protos.h"

#define MD5_BLOCK_LEN 64
#define MD5_DIGEST_LEN 16

/**
 * hmac_md5 - produce CRAM-MD5 challenge response
 * @param[in]  password  Password to encrypt
 * @param[in]  challenge Challenge from server
 * @param[out] response  Buffer for the response
 */
static void hmac_md5(const char *password, char *challenge, unsigned char *response)
{
  struct Md5Ctx ctx;
  unsigned char ipad[MD5_BLOCK_LEN] = { 0 };
  unsigned char opad[MD5_BLOCK_LEN] = { 0 };
  unsigned char secret[MD5_BLOCK_LEN + 1];
  size_t secret_len;

  secret_len = strlen(password);

  /* passwords longer than MD5_BLOCK_LEN bytes are substituted with their MD5
   * digests */
  if (secret_len > MD5_BLOCK_LEN)
  {
    unsigned char hash_passwd[MD5_DIGEST_LEN];
    mutt_md5_bytes(password, secret_len, hash_passwd);
    mutt_str_strfcpy((char *) secret, (char *) hash_passwd, MD5_DIGEST_LEN);
    secret_len = MD5_DIGEST_LEN;
  }
  else
    mutt_str_strfcpy((char *) secret, password, sizeof(secret));

  memcpy(ipad, secret, secret_len);
  memcpy(opad, secret, secret_len);

  for (int i = 0; i < MD5_BLOCK_LEN; i++)
  {
    ipad[i] ^= 0x36;
    opad[i] ^= 0x5c;
  }

  /* inner hash: challenge and ipadded secret */
  mutt_md5_init_ctx(&ctx);
  mutt_md5_process_bytes(ipad, MD5_BLOCK_LEN, &ctx);
  mutt_md5_process(challenge, &ctx);
  mutt_md5_finish_ctx(&ctx, response);

  /* outer hash: inner hash and opadded secret */
  mutt_md5_init_ctx(&ctx);
  mutt_md5_process_bytes(opad, MD5_BLOCK_LEN, &ctx);
  mutt_md5_process_bytes(response, MD5_DIGEST_LEN, &ctx);
  mutt_md5_finish_ctx(&ctx, response);
}

/**
 * imap_auth_cram_md5 - Authenticate using CRAM-MD5
 * @param idata  Server data
 * @param method Name of this authentication method
 * @retval enum Result, e.g. #IMAP_AUTH_SUCCESS
 */
enum ImapAuthRes imap_auth_cram_md5(struct ImapData *idata, const char *method)
{
  char ibuf[LONG_STRING * 2], obuf[LONG_STRING];
  unsigned char hmac_response[MD5_DIGEST_LEN];
  int len;
  int rc;

  if (!mutt_bit_isset(idata->capabilities, ACRAM_MD5))
    return IMAP_AUTH_UNAVAIL;

  mutt_message(_("Authenticating (CRAM-MD5)..."));

  /* get auth info */
  if (mutt_account_getlogin(&idata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(&idata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  imap_cmd_start(idata, "AUTHENTICATE CRAM-MD5");

  /* From RFC2195:
   * The data encoded in the first ready response contains a presumptively
   * arbitrary string of random digits, a timestamp, and the fully-qualified
   * primary host name of the server. The syntax of the unencoded form must
   * correspond to that of an RFC822 'msg-id' [RFC822] as described in [POP3].
   */
  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_RESPOND)
  {
    mutt_debug(1, "Invalid response from server: %s\n", ibuf);
    goto bail;
  }

  len = mutt_b64_decode(obuf, idata->buf + 2);
  if (len == -1)
  {
    mutt_debug(1, "Error decoding base64 response.\n");
    goto bail;
  }

  obuf[len] = '\0';
  mutt_debug(2, "CRAM challenge: %s\n", obuf);

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
  hmac_md5(idata->conn->account.pass, obuf, hmac_response);
  /* dubious optimisation I saw elsewhere: make the whole string in one call */
  int off = snprintf(obuf, sizeof(obuf), "%s ", idata->conn->account.user);
  mutt_md5_toascii(hmac_response, obuf + off);
  mutt_debug(2, "CRAM response: %s\n", obuf);

  /* ibuf must be long enough to store the base64 encoding of obuf,
   * plus the additional debris */
  mutt_b64_encode(ibuf, obuf, strlen(obuf), sizeof(ibuf) - 2);
  mutt_str_strcat(ibuf, sizeof(ibuf), "\r\n");
  mutt_socket_send(idata->conn, ibuf);

  do
    rc = imap_cmd_step(idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_OK)
  {
    mutt_debug(1, "Error receiving server response.\n");
    goto bail;
  }

  if (imap_code(idata->buf))
    return IMAP_AUTH_SUCCESS;

bail:
  mutt_error(_("CRAM-MD5 authentication failed."));
  return IMAP_AUTH_FAILURE;
}
