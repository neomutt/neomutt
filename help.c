/**
 * @file
 * Generate the help-page and GUI display it
 *
 * @authors
 * Copyright (C) 1996-2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Yousef Akbar <yousef@yhakbar.com>
 * Copyright (C) 2021 Ihor Antonov <ihor@antonovs.family>
 * Copyright (C) 2023 Tóth János <gomba007@gmail.com>
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
 * @page neo_help Generate the help-page and GUI display it
 *
 * Generate and help-page and GUI display it
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "protos.h"

/**
 * FlagCharsDesc - Descriptions of the $flag_chars flags
 *
 * This must be in the same order as #FlagChars
 */
static const char *FlagCharsDesc[] = {
  N_("message is tagged"),
  N_("message is flagged"),
  N_("message is deleted"),
  N_("attachment is deleted"),
  N_("message has been replied to"),
  N_("message has been read"),
  N_("message is new"),
  N_("thread has been read"),
  N_("thread has at least one new message"),
  N_("message has been read (%S expando)"),
  N_("message has been read (%Z expando)"),
};

/**
 * CryptCharsDesc - Descriptions of the $crypt_chars flags
 *
 * This must be in the same order as #CryptChars
 */
static const char *CryptCharsDesc[] = {
  N_("message signed with a verified key"),
  N_("message is PGP-encrypted"),
  N_("message is signed"),
  N_("message contains a PGP key"),
  N_("message has no cryptography information"),
};

/**
 * ToCharsDesc - Descriptions of the $to_chars flags
 *
 * This must be in the same order as #ToChars
 */
static const char *ToCharsDesc[] = {
  N_("message is not To: you"),
  N_("message is To: you and only you"),
  N_("message is To: you"),
  N_("message is Cc: to you"),
  N_("message is From: you"),
  N_("message is sent to a subscribed mailing list"),
  N_("you are in the Reply-To: list"),
};

/**
 * dump_message_flags - Write out all the message flags
 * @param menu Menu type
 * @param fp   File to write to
 *
 * Display a quick reminder of all the flags in the config options:
 * - $crypt_chars
 * - $flag_chars
 * - $to_chars
 */
static void dump_message_flags(enum MenuType menu, FILE *fp)
{
  if (menu != MENU_INDEX)
    return;

  const char *flag = NULL;
  int cols;

  fprintf(fp, "\n%s\n\n", _("Message flags:"));

  const struct MbTable *c_flag_chars = cs_subset_mbtable(NeoMutt->sub, "flag_chars");
  fprintf(fp, "$flag_chars:\n");
  for (int i = FLAG_CHAR_TAGGED; i <= FLAG_CHAR_ZEMPTY; i++)
  {
    flag = mbtable_get_nth_wchar(c_flag_chars, i);
    cols = mutt_strwidth(flag);
    fprintf(fp, "    '%s'%*s  %s\n", flag, 4 - cols, "", _(FlagCharsDesc[i]));
  }

  const struct MbTable *c_crypt_chars = cs_subset_mbtable(NeoMutt->sub, "crypt_chars");
  fprintf(fp, "\n$crypt_chars:\n");
  for (int i = FLAG_CHAR_CRYPT_GOOD_SIGN; i <= FLAG_CHAR_CRYPT_NO_CRYPTO; i++)
  {
    flag = mbtable_get_nth_wchar(c_crypt_chars, i);
    cols = mutt_strwidth(flag);
    fprintf(fp, "    '%s'%*s  %s\n", flag, 4 - cols, "", _(CryptCharsDesc[i]));
  }

  const struct MbTable *c_to_chars = cs_subset_mbtable(NeoMutt->sub, "to_chars");
  fprintf(fp, "\n$to_chars:\n");
  for (int i = FLAG_CHAR_TO_NOT_IN_THE_LIST; i <= FLAG_CHAR_TO_REPLY_TO; i++)
  {
    flag = mbtable_get_nth_wchar(c_to_chars, i);
    cols = mutt_strwidth(flag);
    fprintf(fp, "    '%s'%*s  %s\n", flag, 4 - cols, "", _(ToCharsDesc[i]));
  }

  fprintf(fp, "\n");
}

/**
 * mutt_help - Display the Help Page
 * @param menu Menu type
 */
void mutt_help(enum MenuType menu)
{
  struct BindingInfoArray bia_bind = ARRAY_HEAD_INITIALIZER;
  struct BindingInfoArray bia_macro = ARRAY_HEAD_INITIALIZER;
  struct BindingInfoArray bia_gen = ARRAY_HEAD_INITIALIZER;
  struct BindingInfoArray bia_unbound = ARRAY_HEAD_INITIALIZER;
  struct Buffer *banner = NULL;
  struct Buffer *tempfile = NULL;
  struct BindingInfo *bi = NULL;

  // ---------------------------------------------------------------------------
  // Gather the data

  gather_menu(menu, &bia_bind, &bia_macro);

  ARRAY_SORT(&bia_bind, binding_sort, NULL);
  ARRAY_SORT(&bia_macro, binding_sort, NULL);

  int wb0 = measure_column(&bia_bind, 0);
  int wb1 = measure_column(&bia_bind, 1);

  const bool need_generic = (menu != MENU_EDITOR) && (menu != MENU_PAGER) &&
                            (menu != MENU_GENERIC);
  if (need_generic)
  {
    gather_menu(MENU_GENERIC, &bia_gen, &bia_macro);

    ARRAY_SORT(&bia_gen, binding_sort, NULL);
    wb0 = MAX(wb0, measure_column(&bia_gen, 0));
    wb1 = MAX(wb1, measure_column(&bia_gen, 1));
  }

  const int wm0 = measure_column(&bia_macro, 0);

  const struct MenuFuncOp *funcs = km_get_table(menu);
  gather_unbound(funcs, &Keymaps[menu], NULL, &bia_unbound);

  if (need_generic)
    gather_unbound(OpGeneric, &Keymaps[MENU_GENERIC], &Keymaps[menu], &bia_unbound);

  ARRAY_SORT(&bia_unbound, binding_sort, NULL);
  const int wu1 = measure_column(&bia_unbound, 1);

  // ---------------------------------------------------------------------------
  // Save the data to a file

  tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  FILE *fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp)
  {
    mutt_perror("%s", buf_string(tempfile));
    goto cleanup;
  }

  const char *menu_name = mutt_map_get_name(menu, MenuNames);

  fprintf(fp, "%s bindings\n", menu_name);
  fprintf(fp, "\n");
  ARRAY_FOREACH(bi, &bia_bind)
  {
    // key text description
    fprintf(fp, "%*s  %*s  %s\n", -wb0, bi->a[0], -wb1, bi->a[1], bi->a[2]);
  }
  fprintf(fp, "\n");

  if (need_generic)
  {
    fprintf(fp, "%s bindings\n", "generic");
    fprintf(fp, "\n");
    ARRAY_FOREACH(bi, &bia_gen)
    {
      // key function description
      fprintf(fp, "%*s  %*s  %s\n", -wb0, bi->a[0], -wb1, bi->a[1], bi->a[2]);
    }
    fprintf(fp, "\n");
  }

  fprintf(fp, "macros\n");
  fprintf(fp, "\n");
  ARRAY_FOREACH(bi, &bia_macro)
  {
    if (bi->a[2]) // description
    {
      // key description, macro-text, blank line
      fprintf(fp, "%*s  %s\n", -wm0, bi->a[0], bi->a[2]);
      fprintf(fp, "%s\n", bi->a[1]);
      fprintf(fp, "\n");
    }
    else
    {
      // key macro-text
      fprintf(fp, "%*s  %s\n", -wm0, bi->a[0], bi->a[1]);
    }
  }
  fprintf(fp, "\n");

  fprintf(fp, "unbound functions\n");
  fprintf(fp, "\n");
  ARRAY_FOREACH(bi, &bia_unbound)
  {
    // function description
    fprintf(fp, "%*s  %s\n", -wu1, bi->a[1], bi->a[2]);
  }

  dump_message_flags(menu, fp);
  mutt_file_fclose(&fp);

  // ---------------------------------------------------------------------------
  // Display data

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pview.mode = PAGER_MODE_HELP;
  pview.flags = MUTT_PAGER_MARKER | MUTT_PAGER_NOWRAP | MUTT_PAGER_STRIPES;

  banner = buf_pool_get();
  buf_printf(banner, _("Help for %s"), menu_name);
  pdata.fname = buf_string(tempfile);
  pview.banner = buf_string(banner);
  mutt_do_pager(&pview, NULL);

cleanup:

  ARRAY_FOREACH(bi, &bia_bind)
  {
    FREE(&bi->a[0]);
  }

  ARRAY_FOREACH(bi, &bia_macro)
  {
    FREE(&bi->a[0]);
    FREE(&bi->a[1]);
  }

  ARRAY_FOREACH(bi, &bia_gen)
  {
    FREE(&bi->a[0]);
  }

  buf_pool_release(&banner);
  buf_pool_release(&tempfile);
  ARRAY_FREE(&bia_bind);
  ARRAY_FREE(&bia_macro);
  ARRAY_FREE(&bia_gen);
  ARRAY_FREE(&bia_unbound);
}
