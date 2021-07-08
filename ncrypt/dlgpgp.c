/**
 * @file
 * PGP Key Selection Dialog
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
 * @page crypt_dlgpgp PGP Key Selection Dialog
 *
 * ## Overview
 *
 * The PGP Key Selection Dialog lets the user select a PGP key.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                     | Type       | See Also             |
 * | :----------------------- | :--------- | :------------------- |
 * | PGP Key Selection Dialog | WT_DLG_PGP | dlg_select_pgp_key() |
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
 * - #PgpUid
 *
 * The @ref gui_simple holds a Menu.  The PGP Key Selection Dialog stores its
 * data (#PgpUid) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                   |
 * | :---------- | :------------------------ | 
 * | #NT_CONFIG  | pgp_key_config_observer() |
 * | #NT_WINDOW  | pgp_key_window_observer() |
 *
 * The PGP Key Selection Dialog doesn't have any specific colours, so it
 * doesn't need to support #NT_COLOR.
 *
 * The PGP Key Selection Dialog does not implement MuttWindow::recalc() or
 * MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "question/lib.h"
#include "format_flags.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgplib.h"

/// Help Bar for the PGP key selection dialog
static const struct Mapping PgpHelp[] = {
  // clang-format off
  { N_("Exit"),      OP_EXIT },
  { N_("Select"),    OP_GENERIC_SELECT_ENTRY },
  { N_("Check key"), OP_VERIFY_KEY },
  { N_("Help"),      OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * struct PgpEntry - An entry in a PGP key menu
 */
struct PgpEntry
{
  size_t num;
  struct PgpUid *uid;
};

static const char trust_flags[] = "?- +";

/**
 * pgp_compare_key_address - Compare Key addresses and IDs for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_key_address(const void *a, const void *b)
{
  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  int r = mutt_istr_cmp((*s)->addr, (*t)->addr);
  if (r != 0)
    return (r > 0);

  return mutt_istr_cmp(pgp_fpr_or_lkeyid((*s)->parent), pgp_fpr_or_lkeyid((*t)->parent)) > 0;
}

/**
 * pgp_compare_address_qsort - Compare the addresses of two PGP keys
 * @param a First address
 * @param b Second address
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_address_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !pgp_compare_key_address(a, b) :
                                            pgp_compare_key_address(a, b);
}

/**
 * pgp_compare_key_date - Compare Key dates for sorting
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_key_date(const void *a, const void *b)
{
  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  int r = ((*s)->parent->gen_time - (*t)->parent->gen_time);
  if (r != 0)
    return r > 0;
  return mutt_istr_cmp((*s)->addr, (*t)->addr) > 0;
}

/**
 * pgp_compare_date_qsort - Compare the dates of two PGP keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_date_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !pgp_compare_key_date(a, b) :
                                            pgp_compare_key_date(a, b);
}

/**
 * pgp_compare_keyid - Compare Key IDs and addresses for sorting
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_keyid(const void *a, const void *b)
{
  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  int r = mutt_istr_cmp(pgp_fpr_or_lkeyid((*s)->parent), pgp_fpr_or_lkeyid((*t)->parent));
  if (r != 0)
    return (r > 0);
  return mutt_istr_cmp((*s)->addr, (*t)->addr) > 0;
}

/**
 * pgp_compare_keyid_qsort - Compare key IDs
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_keyid_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !pgp_compare_keyid(a, b) :
                                            pgp_compare_keyid(a, b);
}

/**
 * pgp_compare_key_trust - Compare the trust of keys for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 *
 * Compare two trust values, the key length, the creation dates. the addresses
 * and the key IDs.
 */
static int pgp_compare_key_trust(const void *a, const void *b)
{
  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  int r = (((*s)->parent->flags & KEYFLAG_RESTRICTIONS) -
           ((*t)->parent->flags & KEYFLAG_RESTRICTIONS));
  if (r != 0)
    return r > 0;
  r = ((*s)->trust - (*t)->trust);
  if (r != 0)
    return r < 0;
  r = ((*s)->parent->keylen - (*t)->parent->keylen);
  if (r != 0)
    return r < 0;
  r = ((*s)->parent->gen_time - (*t)->parent->gen_time);
  if (r != 0)
    return r < 0;
  r = mutt_istr_cmp((*s)->addr, (*t)->addr);
  if (r != 0)
    return r > 0;
  return mutt_istr_cmp(pgp_fpr_or_lkeyid((*s)->parent), pgp_fpr_or_lkeyid((*t)->parent)) > 0;
}

/**
 * pgp_compare_trust_qsort - Compare the trust levels of two PGP keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_trust_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !pgp_compare_key_trust(a, b) :
                                            pgp_compare_key_trust(a, b);
}

/**
 * pgp_key_abilities - Turn PGP key abilities into a string
 * @param flags Flags, see #KeyFlags
 * @retval ptr Abilities string
 *
 * @note This returns a pointer to a static buffer
 */
static char *pgp_key_abilities(KeyFlags flags)
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
 * pgp_flags - Turn PGP key flags into a string
 * @param flags Flags, see #KeyFlags
 * @retval char Flag character
 */
static char pgp_flags(KeyFlags flags)
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
 * pgp_entry_format_str - Format an entry on the PGP key selection menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%n     | Number
 * | \%t     | Trust/validity of the key-uid association
 * | \%u     | User id
 * | \%[fmt] | Date of key using strftime(3)
 * |         |
 * | \%a     | Algorithm
 * | \%c     | Capabilities
 * | \%f     | Flags
 * | \%k     | Key id
 * | \%l     | Length
 * |         |
 * | \%A     | Algorithm of the principal key
 * | \%C     | Capabilities of the principal key
 * | \%F     | Flags of the principal key
 * | \%K     | Key id of the principal key
 * | \%L     | Length of the principal key
 */
static const char *pgp_entry_format_str(char *buf, size_t buflen, size_t col, int cols,
                                        char op, const char *src, const char *prec,
                                        const char *if_str, const char *else_str,
                                        intptr_t data, MuttFormatFlags flags)
{
  char fmt[128];
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  struct PgpEntry *entry = (struct PgpEntry *) data;
  struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;
  struct PgpKeyInfo *pkey = pgp_principal_key(key);

  if (isupper((unsigned char) op))
    key = pkey;

  KeyFlags kflags = key->flags | (pkey->flags & KEYFLAG_RESTRICTIONS) | uid->flags;

  switch (tolower(op))
  {
    case 'a':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, key->algorithm);
      }
      break;
    case 'c':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, pgp_key_abilities(kflags));
      }
      else if (!(kflags & KEYFLAG_ABILITIES))
        optional = false;
      break;
    case 'f':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, pgp_flags(kflags));
      }
      else if (!(kflags & KEYFLAG_RESTRICTIONS))
        optional = false;
      break;
    case 'k':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, pgp_this_keyid(key));
      }
      break;
    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, key->keylen);
      }
      break;
    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, entry->num);
      }
      break;
    case 't':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, trust_flags[uid->trust & 0x03]);
      }
      else if (!(uid->trust & 0x03))
      {
        /* undefined trust */
        optional = false;
      }
      break;
    case 'u':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(uid->addr));
      }
      break;
    case '[':

    {
      char buf2[128];
      bool do_locales = true;
      size_t len;

      char *p = buf;

      const char *cp = src;
      if (*cp == '!')
      {
        do_locales = false;
        cp++;
      }

      len = buflen - 1;
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
            break; /* not enough space */
          cp++;
        }
        else
        {
          *p++ = *cp++;
          len--;
        }
      }
      *p = '\0';

      if (!do_locales)
        setlocale(LC_TIME, "C");
      mutt_date_localtime_format(buf2, sizeof(buf2), buf, key->gen_time);
      if (!do_locales)
        setlocale(LC_TIME, "");

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
    mutt_expando_format(buf, buflen, col, cols, if_str, pgp_entry_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, pgp_entry_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * pgp_make_entry - Format a menu item for the pgp key list - Implements Menu::make_entry()
 */
static void pgp_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct PgpUid **key_table = menu->mdata;
  struct PgpEntry entry;

  entry.uid = key_table[line];
  entry.num = line + 1;

  const char *const c_pgp_entry_format =
      cs_subset_string(NeoMutt->sub, "pgp_entry_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols,
                      NONULL(c_pgp_entry_format), pgp_entry_format_str,
                      (intptr_t) &entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * pgp_key_table_free - Free the key table - Implements Menu::mdata_free()
 *
 * @note The keys are owned by the caller of the dialog
 */
static void pgp_key_table_free(struct Menu *menu, void **ptr)
{
  FREE(ptr);
}

/**
 * pgp_key_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int pgp_key_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
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
 * pgp_key_window_observer - Notification that a Window has changed - Implements ::observer_t
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int pgp_key_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->notify, pgp_key_config_observer, menu);
  notify_observer_remove(win_menu->notify, pgp_key_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_select_pgp_key - Let the user select a key to use
 * @param keys List of PGP keys
 * @param p    Address to match
 * @param s    String to match
 * @retval ptr Selected PGP key
 */
struct PgpKeyInfo *dlg_select_pgp_key(struct PgpKeyInfo *keys,
                                      struct Address *p, const char *s)
{
  struct PgpUid **key_table = NULL;
  struct Menu *menu = NULL;
  int i;
  bool done = false;
  char buf[1024], tmpbuf[256];
  struct PgpKeyInfo *kp = NULL;
  struct PgpUid *a = NULL;
  struct Buffer *tempfile = NULL;

  bool unusable = false;

  int keymax = 0;

  const bool c_pgp_show_unusable =
      cs_subset_bool(NeoMutt->sub, "pgp_show_unusable");
  for (i = 0, kp = keys; kp; kp = kp->next)
  {
    if (!c_pgp_show_unusable && (kp->flags & KEYFLAG_CANTUSE))
    {
      unusable = true;
      continue;
    }

    for (a = kp->address; a; a = a->next)
    {
      if (!c_pgp_show_unusable && (a->flags & KEYFLAG_CANTUSE))
      {
        unusable = true;
        continue;
      }

      if (i == keymax)
      {
        keymax += 5;
        mutt_mem_realloc(&key_table, sizeof(struct PgpUid *) * keymax);
      }

      key_table[i++] = a;
    }
  }

  if ((i == 0) && unusable)
  {
    mutt_error(_("All matching keys are expired, revoked, or disabled"));
    return NULL;
  }

  int (*f)(const void *, const void *);
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  switch (c_pgp_sort_keys & SORT_MASK)
  {
    case SORT_ADDRESS:
      f = pgp_compare_address_qsort;
      break;
    case SORT_DATE:
      f = pgp_compare_date_qsort;
      break;
    case SORT_KEYID:
      f = pgp_compare_keyid_qsort;
      break;
    case SORT_TRUST:
    default:
      f = pgp_compare_trust_qsort;
      break;
  }
  qsort(key_table, i, sizeof(struct PgpUid *), f);

  struct MuttWindow *dlg = simple_dialog_new(MENU_PGP, WT_DLG_PGP, PgpHelp);

  menu = dlg->wdata;
  menu->max = i;
  menu->make_entry = pgp_make_entry;
  menu->mdata = key_table;
  menu->mdata_free = pgp_key_table_free;

  struct MuttWindow *win_menu = menu->win;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->notify, NT_CONFIG, pgp_key_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, pgp_key_window_observer, win_menu);

  if (p)
    snprintf(buf, sizeof(buf), _("PGP keys matching <%s>"), p->mailbox);
  else
    snprintf(buf, sizeof(buf), _("PGP keys matching \"%s\""), s);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, buf);

  kp = NULL;

  mutt_clear_error();

  while (!done)
  {
    switch (menu_loop(menu))
    {
      case OP_VERIFY_KEY:
      {
        FILE *fp_null = fopen("/dev/null", "w");
        if (!fp_null)
        {
          mutt_perror(_("Can't open /dev/null"));
          break;
        }
        tempfile = mutt_buffer_pool_get();
        mutt_buffer_mktemp(tempfile);
        FILE *fp_tmp = mutt_file_fopen(mutt_buffer_string(tempfile), "w");
        if (!fp_tmp)
        {
          mutt_perror(_("Can't create temporary file"));
          mutt_file_fclose(&fp_null);
          mutt_buffer_pool_release(&tempfile);
          break;
        }

        mutt_message(_("Invoking PGP..."));

        const int index = menu_get_index(menu);
        struct PgpUid *cur_key = key_table[index];
        snprintf(tmpbuf, sizeof(tmpbuf), "0x%s",
                 pgp_fpr_or_lkeyid(pgp_principal_key(cur_key->parent)));

        pid_t pid = pgp_invoke_verify_key(NULL, NULL, NULL, -1, fileno(fp_tmp),
                                          fileno(fp_null), tmpbuf);
        if (pid == -1)
        {
          mutt_perror(_("Can't create filter"));
          unlink(mutt_buffer_string(tempfile));
          mutt_file_fclose(&fp_tmp);
          mutt_file_fclose(&fp_null);
        }

        filter_wait(pid);
        mutt_file_fclose(&fp_tmp);
        mutt_file_fclose(&fp_null);
        mutt_clear_error();
        char title[1024];
        snprintf(title, sizeof(title), _("Key ID: 0x%s"),
                 pgp_keyid(pgp_principal_key(cur_key->parent)));

        struct PagerData pdata = { 0 };
        struct PagerView pview = { &pdata };

        pdata.fname = mutt_buffer_string(tempfile);

        pview.banner = title;
        pview.flags = MUTT_PAGER_NO_FLAGS;
        pview.mode = PAGER_MODE_OTHER;

        mutt_do_pager(&pview);
        mutt_buffer_pool_release(&tempfile);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_VIEW_ID:
      {
        const int index = menu_get_index(menu);
        struct PgpUid *cur_key = key_table[index];
        mutt_message("%s", NONULL(cur_key->addr));
        break;
      }

      case OP_GENERIC_SELECT_ENTRY:
      {
        /* XXX make error reporting more verbose */

        const int index = menu_get_index(menu);
        struct PgpUid *cur_key = key_table[index];
        if (OptPgpCheckTrust)
        {
          if (!pgp_key_is_valid(cur_key->parent))
          {
            mutt_error(_("This key can't be used: expired/disabled/revoked"));
            break;
          }
        }

        if (OptPgpCheckTrust && (!pgp_id_is_valid(cur_key) || !pgp_id_is_strong(cur_key)))
        {
          const char *str = "";
          char buf2[1024];

          if (cur_key->flags & KEYFLAG_CANTUSE)
          {
            str = _("ID is expired/disabled/revoked. Do you really want to use "
                    "the key?");
          }
          else
          {
            switch (cur_key->trust & 0x03)
            {
              case 0:
                str = _("ID has undefined validity. Do you really want to use "
                        "the key?");
                break;
              case 1:
                str = _("ID is not valid. Do you really want to use the key?");
                break;
              case 2:
                str = _("ID is only marginally valid. Do you really want to "
                        "use the key?");
                break;
            }
          }

          snprintf(buf2, sizeof(buf2), "%s", str);

          if (mutt_yesorno(buf2, MUTT_NO) != MUTT_YES)
          {
            mutt_clear_error();
            break;
          }
        }

        kp = cur_key->parent;
        done = true;
        break;
      }

      case OP_EXIT:

        kp = NULL;
        done = true;
        break;
    }
  }

  simple_dialog_free(&dlg);
  return kp;
}
