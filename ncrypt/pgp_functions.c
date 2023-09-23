/**
 * @file
 * Pgp functions
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
 * @page pgp_functions Pgp functions
 *
 * Pgp functions
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "pgp_functions.h"
#include "lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "question/lib.h"
#include "globals.h"
#include "mutt_logging.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgplib.h"

/**
 * op_exit - Exit this menu - Implements ::pgp_function_t - @ingroup pgp_function_api
 */
static int op_exit(struct PgpData *pd, int op)
{
  pd->done = true;
  return FR_SUCCESS;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::pgp_function_t - @ingroup pgp_function_api
 */
static int op_generic_select_entry(struct PgpData *pd, int op)
{
  /* XXX make error reporting more verbose */

  const int index = menu_get_index(pd->menu);
  struct PgpUid *cur_key = pd->key_table[index];
  if (OptPgpCheckTrust)
  {
    if (!pgp_key_is_valid(cur_key->parent))
    {
      mutt_error(_("This key can't be used: expired/disabled/revoked"));
      return FR_ERROR;
    }
  }

  if (OptPgpCheckTrust && (!pgp_id_is_valid(cur_key) || !pgp_id_is_strong(cur_key)))
  {
    const char *str = "";
    char buf2[1024];

    if (cur_key->flags & KEYFLAG_CANTUSE)
    {
      str = _("ID is expired/disabled/revoked. Do you really want to use the key?");
    }
    else
    {
      switch (cur_key->trust & 0x03)
      {
        case 0:
          str = _("ID has undefined validity. Do you really want to use the key?");
          break;
        case 1:
          str = _("ID is not valid. Do you really want to use the key?");
          break;
        case 2:
          str = _("ID is only marginally valid. Do you really want to use the key?");
          break;
      }
    }

    snprintf(buf2, sizeof(buf2), "%s", str);

    if (query_yesorno(buf2, MUTT_NO) != MUTT_YES)
    {
      mutt_clear_error();
      return FR_NO_ACTION;
    }
  }

  pd->key = cur_key->parent;
  pd->done = true;
  return FR_SUCCESS;
}

/**
 * op_verify_key - Verify a PGP public key - Implements ::pgp_function_t - @ingroup pgp_function_api
 */
static int op_verify_key(struct PgpData *pd, int op)
{
  FILE *fp_null = fopen("/dev/null", "w");
  if (!fp_null)
  {
    mutt_perror(_("Can't open /dev/null"));
    return FR_ERROR;
  }
  struct Buffer *tempfile = NULL;
  tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  FILE *fp_tmp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_tmp)
  {
    mutt_perror(_("Can't create temporary file"));
    mutt_file_fclose(&fp_null);
    buf_pool_release(&tempfile);
    return FR_ERROR;
  }

  mutt_message(_("Invoking PGP..."));

  const int index = menu_get_index(pd->menu);
  struct PgpUid *cur_key = pd->key_table[index];
  char tmpbuf[256] = { 0 };
  snprintf(tmpbuf, sizeof(tmpbuf), "0x%s",
           pgp_fpr_or_lkeyid(pgp_principal_key(cur_key->parent)));

  pid_t pid = pgp_invoke_verify_key(NULL, NULL, NULL, -1, fileno(fp_tmp),
                                    fileno(fp_null), tmpbuf);
  if (pid == -1)
  {
    mutt_perror(_("Can't create filter"));
    unlink(buf_string(tempfile));
    mutt_file_fclose(&fp_tmp);
    mutt_file_fclose(&fp_null);
  }

  filter_wait(pid);
  mutt_file_fclose(&fp_tmp);
  mutt_file_fclose(&fp_null);
  mutt_clear_error();
  char title[1024] = { 0 };
  snprintf(title, sizeof(title), _("Key ID: 0x%s"),
           pgp_keyid(pgp_principal_key(cur_key->parent)));

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = title;
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);
  menu_queue_redraw(pd->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_view_id - View the key's user id - Implements ::pgp_function_t - @ingroup pgp_function_api
 */
static int op_view_id(struct PgpData *pd, int op)
{
  const int index = menu_get_index(pd->menu);
  struct PgpUid *cur_key = pd->key_table[index];
  mutt_message("%s", NONULL(cur_key->addr));
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * PgpFunctions - All the NeoMutt functions that the Pgp supports
 */
static const struct PgpFunction PgpFunctions[] = {
  // clang-format off
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { OP_VERIFY_KEY,             op_verify_key },
  { OP_VIEW_ID,                op_view_id },
  { 0, NULL },
  // clang-format on
};

/**
 * pgp_function_dispatcher - Perform a Pgp function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int pgp_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return FR_ERROR;

  struct PgpData *pd = dlg->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; PgpFunctions[i].op != OP_NULL; i++)
  {
    const struct PgpFunction *fn = &PgpFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(pd, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
