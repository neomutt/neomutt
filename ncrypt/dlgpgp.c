/**
 * @file
 * PGP key selection dialog
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
 * @page crypt_dlgpgp PGP key selection dialog
 *
 * PGP key selection dialog
 */

#include "config.h"
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "gui/lib.h"
#include "format_flags.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#include "pgplib.h"
#include "protos.h"

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
  return (C_PgpSortKeys & SORT_REVERSE) ? !pgp_compare_key_address(a, b) :
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
  return (C_PgpSortKeys & SORT_REVERSE) ? !pgp_compare_key_date(a, b) :
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
  return (C_PgpSortKeys & SORT_REVERSE) ? !pgp_compare_keyid(a, b) :
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
  return (C_PgpSortKeys & SORT_REVERSE) ? !pgp_compare_key_trust(a, b) :
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
  return src;
}

/**
 * pgp_make_entry - Format a menu item for the pgp key list - Implements Menu::make_entry()
 */
static void pgp_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct PgpUid **key_table = menu->mdata;
  struct PgpEntry entry;

  entry.uid = key_table[line];
  entry.num = line + 1;

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(C_PgpEntryFormat), pgp_entry_format_str,
                      (intptr_t) &entry, MUTT_FORMAT_ARROWCURSOR);
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

  for (i = 0, kp = keys; kp; kp = kp->next)
  {
    if (!C_PgpShowUnusable && (kp->flags & KEYFLAG_CANTUSE))
    {
      unusable = true;
      continue;
    }

    for (a = kp->address; a; a = a->next)
    {
      if (!C_PgpShowUnusable && (a->flags & KEYFLAG_CANTUSE))
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
  switch (C_PgpSortKeys & SORT_MASK)
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

  menu = mutt_menu_new(MENU_PGP);
  struct MuttWindow *dlg = dialog_create_simple_index(menu, WT_DLG_PGP);
  dlg->help_data = PgpHelp;
  dlg->help_menu = MENU_PGP;

  menu->max = i;
  menu->make_entry = pgp_make_entry;
  menu->mdata = key_table;
  mutt_menu_push_current(menu);

  if (p)
    snprintf(buf, sizeof(buf), _("PGP keys matching <%s>"), p->mailbox);
  else
    snprintf(buf, sizeof(buf), _("PGP keys matching \"%s\""), s);

  menu->title = buf;

  kp = NULL;

  mutt_clear_error();

  while (!done)
  {
    switch (mutt_menu_loop(menu))
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

        snprintf(tmpbuf, sizeof(tmpbuf), "0x%s",
                 pgp_fpr_or_lkeyid(pgp_principal_key(key_table[menu->current]->parent)));

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
                 pgp_keyid(pgp_principal_key(key_table[menu->current]->parent)));
        mutt_do_pager(title, mutt_buffer_string(tempfile), MUTT_PAGER_NO_FLAGS, NULL);
        mutt_buffer_pool_release(&tempfile);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_VIEW_ID:

        mutt_message("%s", NONULL(key_table[menu->current]->addr));
        break;

      case OP_GENERIC_SELECT_ENTRY:

        /* XXX make error reporting more verbose */

        if (OptPgpCheckTrust)
        {
          if (!pgp_key_is_valid(key_table[menu->current]->parent))
          {
            mutt_error(_("This key can't be used: expired/disabled/revoked"));
            break;
          }
        }

        if (OptPgpCheckTrust && (!pgp_id_is_valid(key_table[menu->current]) ||
                                 !pgp_id_is_strong(key_table[menu->current])))
        {
          const char *str = "";
          char buf2[1024];

          if (key_table[menu->current]->flags & KEYFLAG_CANTUSE)
          {
            str = _("ID is expired/disabled/revoked. Do you really want to use "
                    "the key?");
          }
          else
          {
            switch (key_table[menu->current]->trust & 0x03)
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

        kp = key_table[menu->current]->parent;
        done = true;
        break;

      case OP_EXIT:

        kp = NULL;
        done = true;
        break;
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_destroy_simple_index(&dlg);
  FREE(&key_table);

  return kp;
}
