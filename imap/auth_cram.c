/**
 * @file
 * IMAP CRAM-MD5 authentication method
 *
 * @authors
 * Copyright (C) 1999-2001,2005 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
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
 * @page imap_auth_cram CRAM-MD5 authentication
 *
 * IMAP CRAM-MD5 authentication method
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "adata.h"
#include "auth.h"

#define MD5_BLOCK_LEN 64
#define MD5_DIGEST_LEN 16

/**
 * hmac_md5 - Produce CRAM-MD5 challenge response
 * @param[in]  password  Password to encrypt
 * @param[in]  challenge Challenge from server
 * @param[out] response  Buffer for the response
 */
static void hmac_md5(const char *password, const char *challenge, unsigned char *response)
{
  struct Md5Ctx md5ctx = { 0 };
  unsigned char ipad[MD5_BLOCK_LEN] = { 0 };
  unsigned char opad[MD5_BLOCK_LEN] = { 0 };
  unsigned char secret[MD5_BLOCK_LEN + 1] = { 0 };

  size_t secret_len = strlen(password);

  /* passwords longer than MD5_BLOCK_LEN bytes are substituted with their MD5
   * digests */
  if (secret_len > MD5_BLOCK_LEN)
  {
    unsigned char hash_passwd[MD5_DIGEST_LEN];
    mutt_md5_bytes(password, secret_len, hash_passwd);
    mutt_str_copy((char *) secret, (char *) hash_passwd, MD5_DIGEST_LEN);
    secret_len = MD5_DIGEST_LEN;
  }
  else
  {
    mutt_str_copy((char *) secret, password, sizeof(secret));
  }

  memcpy(ipad, secret, secret_len);
  memcpy(opad, secret, secret_len);

  for (int i = 0; i < MD5_BLOCK_LEN; i++)
  {
    ipad[i] ^= 0x36;
    opad[i] ^= 0x5c;
  }

  /* inner hash: challenge and ipadded secret */
  mutt_md5_init_ctx(&md5ctx);
  mutt_md5_process_bytes(ipad, MD5_BLOCK_LEN, &md5ctx);
  mutt_md5_process(challenge, &md5ctx);
  mutt_md5_finish_ctx(&md5ctx, response);

  /* outer hash: inner hash and opadded secret */
  mutt_md5_init_ctx(&md5ctx);
  mutt_md5_process_bytes(opad, MD5_BLOCK_LEN, &md5ctx);
  mutt_md5_process_bytes(response, MD5_DIGEST_LEN, &md5ctx);
  mutt_md5_finish_ctx(&md5ctx, response);
}

/**
 * imap_auth_cram_md5 - Authenticate using CRAM-MD5 - Implements ImapAuth::authenticate() - @ingroup imap_authenticate
 */
enum ImapAuthRes imap_auth_cram_md5(struct ImapAccountData *adata, const char *method)
{
  if (!(adata->capabilities & IMAP_CAP_AUTH_CRAM_MD5))
    return IMAP_AUTH_UNAVAIL;

  // L10N: (%s) is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
  mutt_message(_("Authenticating (%s)..."), "CRAM-MD5");

  /* get auth info */
  if (mutt_account_getlogin(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;
  if (mutt_account_getpass(&adata->conn->account) < 0)
    return IMAP_AUTH_FAILURE;

  imap_cmd_start(adata, "AUTHENTICATE CRAM-MD5");

  struct Buffer *ibuf = buf_pool_get();
  struct Buffer *obuf = buf_pool_get();
  unsigned char hmac_response[MD5_DIGEST_LEN];
  int rc_step;
  enum ImapAuthRes rc = IMAP_AUTH_FAILURE;

  /* From RFC2195:
   * The data encoded in the first ready response contains a presumptively
   * arbitrary string of random digits, a timestamp, and the fully-qualified
   * primary host name of the server. The syntax of the unencoded form must
   * correspond to that of an RFC822 'msg-id' [RFC822] as described in [POP3].  */
  do
  {
    rc_step = imap_cmd_step(adata);
  } while (rc_step == IMAP_RES_CONTINUE);

  if (rc_step != IMAP_RES_RESPOND)
  {
    mutt_debug(LL_DEBUG1, "Invalid response from server\n");
    goto bail;
  }

  if (mutt_b64_decode(adata->buf + 2, obuf->data, obuf->dsize) == -1)
  {
    mutt_debug(LL_DEBUG1, "Error decoding base64 response\n");
    goto bail;
  }

  mutt_debug(LL_DEBUG2, "CRAM challenge: %s\n", buf_string(obuf));

  /* The client makes note of the data and then responds with a string
   * consisting of the user name, a space, and a 'digest'. The latter is
   * computed by applying the keyed MD5 algorithm from [KEYED-MD5] where the
   * key is a shared secret and the digested text is the timestamp (including
   * angle-brackets).
   *
   * Note: The user name shouldn't be quoted. Since the digest can't contain
   *   spaces, there is no ambiguity. Some servers get this wrong, we'll work
   *   around them when the bug report comes in. Until then, we'll remain
   *   blissfully RFC-compliant.  */
  hmac_md5(adata->conn->account.pass, buf_string(obuf), hmac_response);
  /* dubious optimisation I saw elsewhere: make the whole string in one call */
  int off = buf_printf(obuf, "%s ", adata->conn->account.user);
  mutt_md5_toascii(hmac_response, obuf->data + off);
  mutt_debug(LL_DEBUG2, "CRAM response: %s\n", buf_string(obuf));

  /* ibuf must be long enough to store the base64 encoding of obuf,
   * plus the additional debris */
  mutt_b64_encode(obuf->data, obuf->dsize, ibuf->data, ibuf->dsize - 2);
  buf_addstr(ibuf, "\r\n");
  mutt_socket_send(adata->conn, buf_string(ibuf));

  do
  {
    rc_step = imap_cmd_step(adata);
  } while (rc_step == IMAP_RES_CONTINUE);

  if (rc_step != IMAP_RES_OK)
  {
    mutt_debug(LL_DEBUG1, "Error receiving server response\n");
    goto bail;
  }

  if (imap_code(adata->buf))
    rc = IMAP_AUTH_SUCCESS;

bail:
  if (rc != IMAP_AUTH_SUCCESS)
  {
    // L10N: %s is the method name, e.g. Anonymous, CRAM-MD5, GSSAPI, SASL
    mutt_error(_("%s authentication failed"), "CRAM-MD5");
  }

  buf_pool_release(&ibuf);
  buf_pool_release(&obuf);
  return rc;
}
