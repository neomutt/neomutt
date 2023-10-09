/**
 * @file
 * GPGME Key Selection Dialog
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page crypt_dlg_gpgme GPGME Key Selection Dialog
 *
 * The GPGME Key Selection Dialog lets the user select a PGP key.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                       | Type         | See Also           |
 * | :------------------------- | :----------- | :----------------- |
 * | GPGME Key Selection Dialog | WT_DLG_GPGME | dlg_gpgme()        |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #CryptKeyInfo
 *
 * The @ref gui_simple holds a Menu.  The GPGME Key Selection Dialog stores its
 * data (#CryptKeyInfo) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                     |
 * | :---------- | :-------------------------- |
 * | #NT_CONFIG  | gpgme_key_config_observer() |
 * | #NT_WINDOW  | gpgme_key_window_observer() |
 *
 * The GPGME Key Selection Dialog doesn't have any specific colours, so it
 * doesn't need to support #NT_COLOR.
 *
 * The GPGME Key Selection Dialog does not implement MuttWindow::recalc() or
 * MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <ctype.h>
#include <gpgme.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "crypt_gpgme.h"
#include "format_flags.h"
#include "gpgme_functions.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "sort.h"

/// Help Bar for the GPGME key selection dialog
static const struct Mapping GpgmeHelp[] = {
  // clang-format off
  { N_("Exit"),      OP_EXIT },
  { N_("Select"),    OP_GENERIC_SELECT_ENTRY },
  { N_("Check key"), OP_VERIFY_KEY },
  { N_("Help"),      OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * struct CryptEntry - An entry in the Select-Key menu
 */
struct CryptEntry
{
  size_t num;               ///< Index number
  struct CryptKeyInfo *key; ///< Key
};

/**
 * crypt_sort_address - Compare two keys by their addresses - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_address(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_istr_cmp(s->uid, t->uid);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(crypt_fpr_or_lkeyid(s), crypt_fpr_or_lkeyid(t));

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_sort_keyid - Compare two keys by their IDs - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_keyid(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_istr_cmp(crypt_fpr_or_lkeyid(s), crypt_fpr_or_lkeyid(t));
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->uid, t->uid);

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_sort_date - Compare two keys by their dates - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_date(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  unsigned long ts = 0;
  unsigned long tt = 0;
  int rc = 0;

  if (s->kobj->subkeys && (s->kobj->subkeys->timestamp > 0))
    ts = s->kobj->subkeys->timestamp;
  if (t->kobj->subkeys && (t->kobj->subkeys->timestamp > 0))
    tt = t->kobj->subkeys->timestamp;

  if (ts > tt)
  {
    rc = 1;
    goto done;
  }

  if (ts < tt)
  {
    rc = -1;
    goto done;
  }

  rc = mutt_istr_cmp(s->uid, t->uid);

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_sort_trust - Compare two keys by their trust levels - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_trust(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  unsigned long ts = 0;
  unsigned long tt = 0;

  int rc = mutt_numeric_cmp(s->flags & KEYFLAG_RESTRICTIONS, t->flags & KEYFLAG_RESTRICTIONS);
  if (rc != 0)
    goto done;

  // Note: reversed
  rc = mutt_numeric_cmp(t->validity, s->validity);
  if (rc != 0)
    return rc;

  ts = 0;
  tt = 0;
  if (s->kobj->subkeys)
    ts = s->kobj->subkeys->length;
  if (t->kobj->subkeys)
    tt = t->kobj->subkeys->length;

  // Note: reversed
  rc = mutt_numeric_cmp(tt, ts);
  if (rc != 0)
    goto done;

  ts = 0;
  tt = 0;
  if (s->kobj->subkeys && (s->kobj->subkeys->timestamp > 0))
    ts = s->kobj->subkeys->timestamp;
  if (t->kobj->subkeys && (t->kobj->subkeys->timestamp > 0))
    tt = t->kobj->subkeys->timestamp;

  // Note: reversed
  rc = mutt_numeric_cmp(tt, ts);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->uid, t->uid);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(crypt_fpr_or_lkeyid(s), crypt_fpr_or_lkeyid(t));

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_key_abilities - Parse key flags into a string
 * @param flags Flags, see #KeyFlags
 * @retval ptr Flag string
 *
 * @note The string is statically allocated.
 */
static char *crypt_key_abilities(KeyFlags flags)
{
  static char buf[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buf[0] = '-';
  else if (flags & KEYFLAG_PREFER_SIGNING)
    buf[0] = '.';
  else
    buf[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buf[1] = '-';
  else if (flags & KEYFLAG_PREFER_ENCRYPTION)
    buf[1] = '.';
  else
    buf[1] = 's';

  buf[2] = '\0';

  return buf;
}

/**
 * crypt_flags - Parse the key flags into a single character
 * @param flags Flags, see #KeyFlags
 * @retval char Flag character
 *
 * The returned character describes the most important flag.
 */
static char crypt_flags(KeyFlags flags)
{
  if (flags & KEYFLAG_REVOKED)
    return 'R';
  if (flags & KEYFLAG_EXPIRED)
    return 'X';
  if (flags & KEYFLAG_DISABLED)
    return 'd';
  if (flags & KEYFLAG_CRITICAL)
    return 'c';

  return ' ';
}

/**
 * crypt_format_str - Format a string for the key selection menu - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :-------------------------------------------------------
 * | \%n     | Number
 * | \%p     | Protocol
 * | \%t     | Trust/validity of the key-uid association
 * | \%u     | User id
 * | \%[fmt] | Date of key using strftime(3)
 * |         |
 * | \%a     | Algorithm
 * | \%c     | Capabilities
 * | \%f     | Flags
 * | \%i     | Key fingerprint (or long key id if non-existent)
 * | \%k     | Key id
 * | \%l     | Length
 * |         |
 * | \%A     | Algorithm of the principal key
 * | \%C     | Capabilities of the principal key
 * | \%F     | Flags of the principal key
 * | \%I     | Key fingerprint of the principal key (or long key id if non-existent)
 * | \%K     | Key id of the principal key
 * | \%L     | Length of the principal key
 */
static const char *crypt_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    intptr_t data, MuttFormatFlags flags)
{
  char fmt[128] = { 0 };
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  struct CryptEntry *entry = (struct CryptEntry *) data;
  struct CryptKeyInfo *key = entry->key;

  KeyFlags kflags = (key->flags);

  switch (tolower(op))
  {
    case 'a':
      if (!optional)
      {
        const char *s = NULL;
        snprintf(fmt, sizeof(fmt), "%%%s.3s", prec);
        if (key->kobj->subkeys)
          s = gpgme_pubkey_algo_name(key->kobj->subkeys->pubkey_algo);
        else
          s = "?";
        snprintf(buf, buflen, fmt, s);
      }
      break;

    case 'c':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, crypt_key_abilities(kflags));
      }
      else if (!(kflags & KEYFLAG_ABILITIES))
      {
        optional = false;
      }
      break;

    case 'f':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, crypt_flags(kflags));
      }
      else if (!(kflags & KEYFLAG_RESTRICTIONS))
      {
        optional = false;
      }
      break;

    case 'i':
      if (!optional)
      {
        /* fixme: we need a way to distinguish between main and subkeys.
         * Store the idx in entry? */
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, crypt_fpr_or_lkeyid(key));
      }
      break;

    case 'k':
      if (!optional)
      {
        /* fixme: we need a way to distinguish between main and subkeys.
         * Store the idx in entry? */
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, crypt_keyid(key));
      }
      break;

    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%slu", prec);
        unsigned long val;
        if (key->kobj->subkeys)
          val = key->kobj->subkeys->length;
        else
          val = 0;
        snprintf(buf, buflen, fmt, val);
      }
      break;

    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, entry->num);
      }
      break;

    case 'p':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, gpgme_get_protocol_name(key->kobj->protocol));
      break;

    case 't':
    {
      char *s = NULL;
      if ((kflags & KEYFLAG_ISX509))
      {
        s = "x";
      }
      else
      {
        switch (key->validity)
        {
          case GPGME_VALIDITY_FULL:
            s = "f";
            break;
          case GPGME_VALIDITY_MARGINAL:
            s = "m";
            break;
          case GPGME_VALIDITY_NEVER:
            s = "n";
            break;
          case GPGME_VALIDITY_ULTIMATE:
            s = "u";
            break;
          case GPGME_VALIDITY_UNDEFINED:
            s = "q";
            break;
          case GPGME_VALIDITY_UNKNOWN:
          default:
            s = "?";
            break;
        }
      }
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, *s);
      break;
    }

    case 'u':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, key->uid);
      }
      break;

    case '[':
    {
      char buf2[128];
      bool use_c_locale = false;
      struct tm tm = { 0 };

      char *p = buf;

      const char *cp = src;
      if (*cp == '!')
      {
        use_c_locale = true;
        cp++;
      }

      size_t len = buflen - 1;
      while ((len > 0) && (*cp != ']'))
      {
        if (*cp == '%')
        {
          cp++;
          if (len >= 2)
          {
            *p++ = '%';
            *p++ = *cp;
            len -= 2;
          }
          else
          {
            break; /* not enough space */
          }
          cp++;
        }
        else
        {
          *p++ = *cp++;
          len--;
        }
      }
      *p = '\0';

      if (key->kobj->subkeys && (key->kobj->subkeys->timestamp > 0))
        tm = mutt_date_localtime(key->kobj->subkeys->timestamp);
      else
        tm = mutt_date_localtime(0); // Default to 1970-01-01

      if (use_c_locale)
        strftime_l(buf2, sizeof(buf2), buf, &tm, NeoMutt->time_c_locale);
      else
        strftime(buf2, sizeof(buf2), buf, &tm);

      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, buf2);
      if (len > 0)
        src = cp + 1;
      break;
    }

    default:
      *buf = '\0';
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, crypt_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, crypt_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * crypt_make_entry - Format a PGP Key for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $pgp_entry_format, crypt_format_str()
 */
static void crypt_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct CryptKeyInfo **key_table = menu->mdata;
  struct CryptEntry entry = { 0 };

  entry.key = key_table[line];
  entry.num = line + 1;

  const char *const c_pgp_entry_format = cs_subset_string(NeoMutt->sub, "pgp_entry_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols, NONULL(c_pgp_entry_format),
                      crypt_format_str, (intptr_t) &entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * gpgme_key_table_free - Free the key table - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 *
 * @note The keys are owned by the caller of the dialog
 */
static void gpgme_key_table_free(struct Menu *menu, void **ptr)
{
  FREE(ptr);
}

/**
 * gpgme_key_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int gpgme_key_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "pgp_entry_format") &&
      !mutt_str_equal(ev_c->name, "pgp_sort_keys"))
  {
    return 0;
  }

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * gpgme_key_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int gpgme_key_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->sub->notify, gpgme_key_config_observer, menu);
  notify_observer_remove(win_menu->notify, gpgme_key_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_gpgme - Get the user to select a key - @ingroup gui_dlg
 * @param[in]  keys         List of keys to select from
 * @param[in]  p            Address to match
 * @param[in]  s            Real name to display
 * @param[in]  app          Flags, e.g. #APPLICATION_PGP
 * @param[out] forced_valid Set to true if user overrode key's validity
 * @retval ptr Key selected by user
 *
 * The Select GPGME Key Dialog lets the user select a PGP Key to use.
 */
struct CryptKeyInfo *dlg_gpgme(struct CryptKeyInfo *keys, struct Address *p,
                               const char *s, unsigned int app, bool *forced_valid)
{
  int keymax;
  int i;
  sort_t f = NULL;
  enum MenuType menu_to_use = MENU_GENERIC;
  bool unusable = false;

  /* build the key table */
  keymax = 0;
  i = 0;
  struct CryptKeyInfo **key_table = NULL;
  const bool c_pgp_show_unusable = cs_subset_bool(NeoMutt->sub, "pgp_show_unusable");
  for (struct CryptKeyInfo *k = keys; k; k = k->next)
  {
    if (!c_pgp_show_unusable && (k->flags & KEYFLAG_CANTUSE))
    {
      unusable = true;
      continue;
    }

    if (i == keymax)
    {
      keymax += 20;
      mutt_mem_realloc(&key_table, sizeof(struct CryptKeyInfo *) * keymax);
    }

    key_table[i++] = k;
  }

  if (!i && unusable)
  {
    mutt_error(_("All matching keys are marked expired/revoked"));
    return NULL;
  }

  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  switch (c_pgp_sort_keys & SORT_MASK)
  {
    case SORT_ADDRESS:
      f = crypt_sort_address;
      break;
    case SORT_DATE:
      f = crypt_sort_date;
      break;
    case SORT_KEYID:
      f = crypt_sort_keyid;
      break;
    case SORT_TRUST:
    default:
      f = crypt_sort_trust;
      break;
  }

  if (key_table)
  {
    bool sort_reverse = c_pgp_sort_keys & SORT_REVERSE;
    mutt_qsort_r(key_table, i, sizeof(struct CryptKeyInfo *), f, &sort_reverse);
  }

  if (app & APPLICATION_PGP)
    menu_to_use = MENU_KEY_SELECT_PGP;
  else if (app & APPLICATION_SMIME)
    menu_to_use = MENU_KEY_SELECT_SMIME;

  struct MuttWindow *dlg = simple_dialog_new(menu_to_use, WT_DLG_GPGME, GpgmeHelp);

  struct Menu *menu = dlg->wdata;
  menu->max = i;
  menu->make_entry = crypt_make_entry;
  menu->mdata = key_table;
  menu->mdata_free = gpgme_key_table_free;

  struct GpgmeData gd = { false, menu, key_table, NULL, forced_valid };
  dlg->wdata = &gd;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, gpgme_key_config_observer, menu);
  notify_observer_add(menu->win->notify, NT_WINDOW, gpgme_key_window_observer, menu->win);

  const char *ts = NULL;

  if ((app & APPLICATION_PGP) && (app & APPLICATION_SMIME))
    ts = _("PGP and S/MIME keys matching");
  else if ((app & APPLICATION_PGP))
    ts = _("PGP keys matching");
  else if ((app & APPLICATION_SMIME))
    ts = _("S/MIME keys matching");
  else
    ts = _("keys matching");

  char buf[1024] = { 0 };
  if (p)
  {
    /* L10N: 1$s is one of the previous four entries.
       %2$s is an address.
       e.g. "S/MIME keys matching <john.doe@example.com>" */
    snprintf(buf, sizeof(buf), _("%s <%s>"), ts, buf_string(p->mailbox));
  }
  else
  {
    /* L10N: e.g. 'S/MIME keys matching "John Doe".' */
    snprintf(buf, sizeof(buf), _("%s \"%s\""), ts, s);
  }

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, buf);

  mutt_clear_error();

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(menu_to_use, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(menu_to_use);
      continue;
    }
    mutt_clear_error();

    int rc = gpgme_function_dispatcher(dlg, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!gd.done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
  return gd.key;
}
