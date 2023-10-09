/**
 * @file
 * IMAP GNU SASL authentication method
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page imap_auth_gsasl GNU SASL authentication
 *
 * IMAP GNU SASL authentication method
 */

#include "config.h"
#include <stddef.h>
#include <gsasl.h>
#include "private.h"
#include "mutt/lib.h"
#include "conn/lib.h"
#include "adata.h"
#include "auth.h"

/**
 * imap_auth_gsasl - GNU SASL authenticator - Implements ImapAuth::authenticate() - @ingroup imap_authenticate
 */
enum ImapAuthRes imap_auth_gsasl(struct ImapAccountData *adata, const char *method)
{
  Gsasl_session *gsasl_session = NULL;
  struct Buffer *output_buf = NULL;
  char *imap_step_output = NULL;
  int rc = IMAP_AUTH_FAILURE;
  int gsasl_rc = GSASL_OK;
  int imap_step_rc = IMAP_RES_CONTINUE;

  const char *chosen_mech = mutt_gsasl_get_mech(method, adata->capstr);
  if (!chosen_mech)
  {
    mutt_debug(LL_DEBUG2, "mutt_gsasl_get_mech() returned no usable mech\n");
    return IMAP_AUTH_UNAVAIL;
  }

  mutt_debug(LL_DEBUG2, "using mech %s\n", chosen_mech);

  if (mutt_gsasl_client_new(adata->conn, chosen_mech, &gsasl_session) < 0)
  {
    mutt_debug(LL_DEBUG1, "Error allocating GSASL connection.\n");
    return IMAP_AUTH_UNAVAIL;
  }

  mutt_message(_("Authenticating (%s)..."), chosen_mech);

  output_buf = buf_pool_get();
  buf_printf(output_buf, "AUTHENTICATE %s", chosen_mech);
  if (adata->capabilities & IMAP_CAP_SASL_IR)
  {
    char *gsasl_step_output = NULL;
    gsasl_rc = gsasl_step64(gsasl_session, "", &gsasl_step_output);
    if ((gsasl_rc != GSASL_NEEDS_MORE) && (gsasl_rc != GSASL_OK))
    {
      mutt_debug(LL_DEBUG1, "gsasl_step64() failed (%d): %s\n", gsasl_rc,
                 gsasl_strerror(gsasl_rc));
      rc = IMAP_AUTH_UNAVAIL;
      goto bail;
    }

    buf_addch(output_buf, ' ');
    buf_addstr(output_buf, gsasl_step_output);
    gsasl_free(gsasl_step_output);
  }
  imap_cmd_start(adata, buf_string(output_buf));

  do
  {
    do
    {
      imap_step_rc = imap_cmd_step(adata);
    } while (imap_step_rc == IMAP_RES_CONTINUE);

    if ((imap_step_rc == IMAP_RES_BAD) || (imap_step_rc == IMAP_RES_NO))
      goto bail;

    if (imap_step_rc != IMAP_RES_RESPOND)
      break;

    imap_step_output = imap_next_word(adata->buf);

    char *gsasl_step_output = NULL;
    gsasl_rc = gsasl_step64(gsasl_session, imap_step_output, &gsasl_step_output);
    if ((gsasl_rc == GSASL_NEEDS_MORE) || (gsasl_rc == GSASL_OK))
    {
      buf_strcpy(output_buf, gsasl_step_output);
      gsasl_free(gsasl_step_output);
    }
    else
    {
      // sasl error occurred, send an abort string
      mutt_debug(LL_DEBUG1, "gsasl_step64() failed (%d): %s\n", gsasl_rc,
                 gsasl_strerror(gsasl_rc));
      buf_strcpy(output_buf, "*");
    }

    buf_addstr(output_buf, "\r\n");
    mutt_socket_send(adata->conn, buf_string(output_buf));
  } while ((gsasl_rc == GSASL_NEEDS_MORE) || (gsasl_rc == GSASL_OK));

  if (imap_step_rc != IMAP_RES_OK)
  {
    do
      imap_step_rc = imap_cmd_step(adata);
    while (imap_step_rc == IMAP_RES_CONTINUE);
  }

  if (imap_step_rc == IMAP_RES_RESPOND)
  {
    mutt_socket_send(adata->conn, "*\r\n");
    goto bail;
  }

  if ((gsasl_rc != GSASL_OK) || (imap_step_rc != IMAP_RES_OK))
    goto bail;

  if (imap_code(adata->buf))
    rc = IMAP_AUTH_SUCCESS;

bail:
  buf_pool_release(&output_buf);
  mutt_gsasl_client_finish(&gsasl_session);

  if (rc == IMAP_AUTH_FAILURE)
  {
    mutt_debug(LL_DEBUG2, "%s failed\n", chosen_mech);
    mutt_error(_("SASL authentication failed"));
  }

  return rc;
}
