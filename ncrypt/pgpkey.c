/**
 * @file
 * PGP key management routines
 *
 * @authors
 * Copyright (C) 1996-1997,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (c) 1998-2003 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page crypt_pgpkey PGP key management routines
 *
 * PGP key management routines
 */

#include "config.h"
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "pgpkey.h"
#include "lib.h"
#include "crypt.h"
#include "format_flags.h"
#include "gnupgparse.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "pgpinvoke.h"
#include "protos.h"
#include "recvattach.h"
#include "sort.h"
#include "send/lib.h"
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgp.h"
#include "pgplib.h"
#endif

/**
 * struct PgpCache - List of cached PGP keys
 */
struct PgpCache
{
  char *what;
  char *dflt;
  struct PgpCache *next;
};

/**
 * struct PgpEntry - An entry in a PGP key menu
 */
struct PgpEntry
{
  size_t num;
  struct PgpUid *uid;
};

static struct PgpCache *id_defaults = NULL;

static const char trust_flags[] = "?- +";

// clang-format off
typedef uint8_t PgpKeyValidFlags; ///< Pgp Key valid fields, e.g. #PGP_KV_VALID
#define PGP_KV_NO_FLAGS       0   ///< No flags are set
#define PGP_KV_VALID    (1 << 0)  ///< PGP Key ID is valid
#define PGP_KV_ADDR     (1 << 1)  ///< PGP Key address is valid
#define PGP_KV_STRING   (1 << 2)  ///< PGP Key name string is valid
#define PGP_KV_STRONGID (1 << 3)  ///< PGP Key is strong
// clang-format on

#define PGP_KV_MATCH (PGP_KV_ADDR | PGP_KV_STRING)

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
 * pgp_principal_key - Get the main (parent) PGP key
 * @param key Key to start with
 * @retval ptr PGP Key
 */
static struct PgpKeyInfo *pgp_principal_key(struct PgpKeyInfo *key)
{
  if (key->flags & KEYFLAG_SUBKEY && key->parent)
    return key->parent;
  return key;
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
 * compare_key_address - Compare Key addresses and IDs for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int compare_key_address(const void *a, const void *b)
{
  int r;

  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  r = mutt_istr_cmp((*s)->addr, (*t)->addr);
  if (r != 0)
    return (r > 0);

  return mutt_istr_cmp(pgp_fpr_or_lkeyid((*s)->parent), pgp_fpr_or_lkeyid((*t)->parent)) > 0;
}

/**
 * pgp_compare_address - Compare the addresses of two PGP keys
 * @param a First address
 * @param b Second address
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_address(const void *a, const void *b)
{
  return (C_PgpSortKeys & SORT_REVERSE) ? !compare_key_address(a, b) :
                                          compare_key_address(a, b);
}

/**
 * compare_keyid - Compare Key IDs and addresses for sorting
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int compare_keyid(const void *a, const void *b)
{
  int r;

  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  r = mutt_istr_cmp(pgp_fpr_or_lkeyid((*s)->parent), pgp_fpr_or_lkeyid((*t)->parent));
  if (r != 0)
    return (r > 0);
  return mutt_istr_cmp((*s)->addr, (*t)->addr) > 0;
}

/**
 * pgp_compare_keyid - Compare key IDs
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_keyid(const void *a, const void *b)
{
  return (C_PgpSortKeys & SORT_REVERSE) ? !compare_keyid(a, b) : compare_keyid(a, b);
}

/**
 * compare_keyid - Compare Key IDs and addresses for sorting
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int compare_key_date(const void *a, const void *b)
{
  int r;
  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  r = ((*s)->parent->gen_time - (*t)->parent->gen_time);
  if (r != 0)
    return r > 0;
  return mutt_istr_cmp((*s)->addr, (*t)->addr) > 0;
}

/**
 * pgp_compare_date - Compare the dates of two PGP keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_date(const void *a, const void *b)
{
  return (C_PgpSortKeys & SORT_REVERSE) ? !compare_key_date(a, b) :
                                          compare_key_date(a, b);
}

/**
 * compare_key_trust - Compare the trust of keys for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 *
 * Compare two trust values, the key length, the creation dates. the addresses
 * and the key IDs.
 */
static int compare_key_trust(const void *a, const void *b)
{
  int r;

  struct PgpUid const *const *s = (struct PgpUid const *const *) a;
  struct PgpUid const *const *t = (struct PgpUid const *const *) b;

  r = (((*s)->parent->flags & KEYFLAG_RESTRICTIONS) -
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
 * pgp_compare_trust - Compare the trust levels of two PGP keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int pgp_compare_trust(const void *a, const void *b)
{
  return (C_PgpSortKeys & SORT_REVERSE) ? !compare_key_trust(a, b) :
                                          compare_key_trust(a, b);
}

/**
 * pgp_key_is_valid - Is a PGP key valid?
 * @param k Key to examine
 * @retval true If key is valid
 */
static bool pgp_key_is_valid(struct PgpKeyInfo *k)
{
  struct PgpKeyInfo *pk = pgp_principal_key(k);
  if (k->flags & KEYFLAG_CANTUSE)
    return false;
  if (pk->flags & KEYFLAG_CANTUSE)
    return false;

  return true;
}

/**
 * pgp_id_is_strong - Is a PGP key strong?
 * @param uid UID of a PGP key
 * @retval true If key is strong
 */
static bool pgp_id_is_strong(struct PgpUid *uid)
{
  if ((uid->trust & 3) < 3)
    return false;
  /* else */
  return true;
}

/**
 * pgp_id_is_valid - Is a PGP key valid
 * @param uid UID of a PGP key
 * @retval true If key is valid
 */
static bool pgp_id_is_valid(struct PgpUid *uid)
{
  if (!pgp_key_is_valid(uid->parent))
    return false;
  if (uid->flags & KEYFLAG_CANTUSE)
    return false;
  /* else */
  return true;
}

/**
 * pgp_id_matches_addr - Does the key ID match the address
 * @param addr   First email address
 * @param u_addr Second email address
 * @param uid    UID of PGP key
 * @retval num Flags, e.g. #PGP_KV_VALID
 */
static PgpKeyValidFlags pgp_id_matches_addr(struct Address *addr,
                                            struct Address *u_addr, struct PgpUid *uid)
{
  PgpKeyValidFlags flags = PGP_KV_NO_FLAGS;

  if (pgp_id_is_valid(uid))
    flags |= PGP_KV_VALID;

  if (pgp_id_is_strong(uid))
    flags |= PGP_KV_STRONGID;

  if (addr->mailbox && u_addr->mailbox && mutt_istr_equal(addr->mailbox, u_addr->mailbox))
  {
    flags |= PGP_KV_ADDR;
  }

  if (addr->personal && u_addr->personal &&
      mutt_istr_equal(addr->personal, u_addr->personal))
  {
    flags |= PGP_KV_STRING;
  }

  return flags;
}

/**
 * pgp_select_key - Let the user select a key to use
 * @param keys List of PGP keys
 * @param p    Address to match
 * @param s    String to match
 * @retval ptr Selected PGP key
 */
static struct PgpKeyInfo *pgp_select_key(struct PgpKeyInfo *keys,
                                         struct Address *p, const char *s)
{
  struct PgpUid **key_table = NULL;
  struct Menu *menu = NULL;
  int i;
  bool done = false;
  char helpstr[1024], buf[1024], tmpbuf[256];
  char cmd[1024];
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
      f = pgp_compare_address;
      break;
    case SORT_DATE:
      f = pgp_compare_date;
      break;
    case SORT_KEYID:
      f = pgp_compare_keyid;
      break;
    case SORT_TRUST:
    default:
      f = pgp_compare_trust;
      break;
  }
  qsort(key_table, i, sizeof(struct PgpUid *), f);

  helpstr[0] = '\0';
  mutt_make_help(buf, sizeof(buf), _("Exit  "), MENU_PGP, OP_EXIT);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Select  "), MENU_PGP, OP_GENERIC_SELECT_ENTRY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Check key  "), MENU_PGP, OP_VERIFY_KEY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Help"), MENU_PGP, OP_HELP);
  strcat(helpstr, buf);

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_PGP, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  dialog_push(dlg);

  menu = mutt_menu_new(MENU_PGP);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->max = i;
  menu->make_entry = pgp_make_entry;
  menu->help = helpstr;
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
        FILE *fp_tmp = mutt_file_fopen(mutt_b2s(tempfile), "w");
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
          unlink(mutt_b2s(tempfile));
          mutt_file_fclose(&fp_tmp);
          mutt_file_fclose(&fp_null);
        }

        filter_wait(pid);
        mutt_file_fclose(&fp_tmp);
        mutt_file_fclose(&fp_null);
        mutt_clear_error();
        snprintf(cmd, sizeof(cmd), _("Key ID: 0x%s"),
                 pgp_keyid(pgp_principal_key(key_table[menu->current]->parent)));
        mutt_do_pager(cmd, mutt_b2s(tempfile), MUTT_PAGER_NO_FLAGS, NULL);
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
  FREE(&key_table);
  dialog_pop();
  mutt_window_free(&dlg);

  return kp;
}

/**
 * pgp_ask_for_key - Ask the user for a PGP key
 * @param tag       Prompt for the user
 * @param whatfor   Use for key, e.g. "signing"
 * @param abilities Abilities to match, see #KeyFlags
 * @param keyring   PGP keyring to use
 * @retval ptr Selected PGP key
 */
struct PgpKeyInfo *pgp_ask_for_key(char *tag, char *whatfor, KeyFlags abilities,
                                   enum PgpRing keyring)
{
  struct PgpKeyInfo *key = NULL;
  char resp[128];
  struct PgpCache *l = NULL;

  mutt_clear_error();

  resp[0] = '\0';
  if (whatfor)
  {
    for (l = id_defaults; l; l = l->next)
    {
      if (mutt_istr_equal(whatfor, l->what))
      {
        mutt_str_copy(resp, l->dflt, sizeof(resp));
        break;
      }
    }
  }

  while (true)
  {
    resp[0] = '\0';
    if (mutt_get_field(tag, resp, sizeof(resp), MUTT_COMP_NO_FLAGS) != 0)
      return NULL;

    if (whatfor)
    {
      if (l)
        mutt_str_replace(&l->dflt, resp);
      else
      {
        l = mutt_mem_malloc(sizeof(struct PgpCache));
        l->next = id_defaults;
        id_defaults = l;
        l->what = mutt_str_dup(whatfor);
        l->dflt = mutt_str_dup(resp);
      }
    }

    key = pgp_getkeybystr(resp, abilities, keyring);
    if (key)
      return key;

    mutt_error(_("No matching keys found for \"%s\""), resp);
  }
  /* not reached */
}

/**
 * pgp_class_make_key_attachment - Implements CryptModuleSpecs::pgp_make_key_attachment()
 */
struct Body *pgp_class_make_key_attachment(void)
{
  struct Body *att = NULL;
  char buf[1024];
  char tmp[256];
  struct stat sb;
  pid_t pid;
  OptPgpCheckTrust = false;
  struct Buffer *tempf = NULL;

  struct PgpKeyInfo *key = pgp_ask_for_key(_("Please enter the key ID: "), NULL,
                                           KEYFLAG_NO_FLAGS, PGP_PUBRING);

  if (!key)
    return NULL;

  snprintf(tmp, sizeof(tmp), "0x%s", pgp_fpr_or_lkeyid(pgp_principal_key(key)));
  pgp_key_free(&key);

  tempf = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempf);
  FILE *fp_tmp = mutt_file_fopen(mutt_b2s(tempf), "w");
  if (!fp_tmp)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  FILE *fp_null = fopen("/dev/null", "w");
  if (!fp_null)
  {
    mutt_perror(_("Can't open /dev/null"));
    mutt_file_fclose(&fp_tmp);
    unlink(mutt_b2s(tempf));
    goto cleanup;
  }

  mutt_message(_("Invoking PGP..."));

  pid = pgp_invoke_export(NULL, NULL, NULL, -1, fileno(fp_tmp), fileno(fp_null), tmp);
  if (pid == -1)
  {
    mutt_perror(_("Can't create filter"));
    unlink(mutt_b2s(tempf));
    mutt_file_fclose(&fp_tmp);
    mutt_file_fclose(&fp_null);
    goto cleanup;
  }

  filter_wait(pid);

  mutt_file_fclose(&fp_tmp);
  mutt_file_fclose(&fp_null);

  att = mutt_body_new();
  att->filename = mutt_buffer_strdup(tempf);
  att->unlink = true;
  att->use_disp = false;
  att->type = TYPE_APPLICATION;
  att->subtype = mutt_str_dup("pgp-keys");
  snprintf(buf, sizeof(buf), _("PGP Key %s"), tmp);
  att->description = mutt_str_dup(buf);
  mutt_update_encoding(att);

  stat(mutt_b2s(tempf), &sb);
  att->length = sb.st_size;

cleanup:
  mutt_buffer_pool_release(&tempf);
  return att;
}

/**
 * pgp_add_string_to_hints - Split a string and add the parts to a List
 * @param[in]  str   String to parse
 * @param[out] hints List of string parts
 *
 * The string str is split by whitespace and punctuation and the parts added to
 * hints.
 */
static void pgp_add_string_to_hints(const char *str, struct ListHead *hints)
{
  char *scratch = mutt_str_dup(str);
  if (!scratch)
    return;

  for (char *t = strtok(scratch, " ,.:\"()<>\n"); t;
       t = strtok(NULL, " ,.:\"()<>\n"))
  {
    if (strlen(t) > 3)
      mutt_list_insert_tail(hints, mutt_str_dup(t));
  }

  FREE(&scratch);
}

/**
 * pgp_get_lastp - Get the last PGP key in a list
 * @param p List of PGP keys
 * @retval ptr Last PGP key in list
 */
static struct PgpKeyInfo **pgp_get_lastp(struct PgpKeyInfo *p)
{
  for (; p; p = p->next)
    if (!p->next)
      return &p->next;

  return NULL;
}

/**
 * pgp_getkeybyaddr - Find a PGP key by address
 * @param a           Email address to match
 * @param abilities   Abilities to match, see #KeyFlags
 * @param keyring     PGP keyring to use
 * @param oppenc_mode If true, use opportunistic encryption
 * @retval ptr Matching PGP key
 */
struct PgpKeyInfo *pgp_getkeybyaddr(struct Address *a, KeyFlags abilities,
                                    enum PgpRing keyring, bool oppenc_mode)
{
  if (!a)
    return NULL;

  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);

  bool multi = false;

  struct PgpKeyInfo *keys = NULL, *k = NULL, *kn = NULL;
  struct PgpKeyInfo *the_strong_valid_key = NULL;
  struct PgpKeyInfo *a_valid_addrmatch_key = NULL;
  struct PgpKeyInfo *matches = NULL;
  struct PgpKeyInfo **last = &matches;
  struct PgpUid *q = NULL;

  if (a->mailbox)
    pgp_add_string_to_hints(a->mailbox, &hints);
  if (a->personal)
    pgp_add_string_to_hints(a->personal, &hints);

  if (!oppenc_mode)
    mutt_message(_("Looking for keys matching \"%s\"..."), a->mailbox);
  keys = pgp_get_candidates(keyring, &hints);

  mutt_list_free(&hints);

  if (!keys)
    return NULL;

  mutt_debug(LL_DEBUG5, "looking for %s <%s>\n", a->personal, a->mailbox);

  for (k = keys; k; k = kn)
  {
    kn = k->next;

    mutt_debug(LL_DEBUG5, "  looking at key: %s\n", pgp_keyid(k));

    if (abilities && !(k->flags & abilities))
    {
      mutt_debug(LL_DEBUG3, "  insufficient abilities: Has %x, want %x\n", k->flags, abilities);
      continue;
    }

    bool match = false; /* any match */

    for (q = k->address; q; q = q->next)
    {
      struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
      mutt_addrlist_parse(&al, NONULL(q->addr));
      struct Address *qa = NULL;
      TAILQ_FOREACH(qa, &al, entries)
      {
        PgpKeyValidFlags validity = pgp_id_matches_addr(a, qa, q);

        if (validity & PGP_KV_MATCH) /* something matches */
          match = true;

        if ((validity & PGP_KV_VALID) && (validity & PGP_KV_ADDR))
        {
          if (validity & PGP_KV_STRONGID)
          {
            if (the_strong_valid_key && (the_strong_valid_key != k))
              multi = true;
            the_strong_valid_key = k;
          }
          else
          {
            a_valid_addrmatch_key = k;
          }
        }
      }

      mutt_addrlist_clear(&al);
    }

    if (match)
    {
      *last = pgp_principal_key(k);
      kn = pgp_remove_key(&keys, *last);
      last = pgp_get_lastp(k);
    }
  }

  pgp_key_free(&keys);

  if (matches)
  {
    if (oppenc_mode)
    {
      if (the_strong_valid_key)
      {
        pgp_remove_key(&matches, the_strong_valid_key);
        k = the_strong_valid_key;
      }
      else if (a_valid_addrmatch_key && !C_CryptOpportunisticEncryptStrongKeys)
      {
        pgp_remove_key(&matches, a_valid_addrmatch_key);
        k = a_valid_addrmatch_key;
      }
      else
        k = NULL;
    }
    else if (the_strong_valid_key && !multi)
    {
      /* There was precisely one strong match on a valid ID.
       * Proceed without asking the user.  */
      pgp_remove_key(&matches, the_strong_valid_key);
      k = the_strong_valid_key;
    }
    else
    {
      /* Else: Ask the user.  */
      k = pgp_select_key(matches, a, NULL);
      if (k)
        pgp_remove_key(&matches, k);
    }

    pgp_key_free(&matches);

    return k;
  }

  return NULL;
}

/**
 * pgp_getkeybystr - Find a PGP key by string
 * @param cp         String to match, can be empty but cannot be NULL
 * @param abilities  Abilities to match, see #KeyFlags
 * @param keyring    PGP keyring to use
 * @retval ptr Matching PGP key
 */
struct PgpKeyInfo *pgp_getkeybystr(const char *cp, KeyFlags abilities, enum PgpRing keyring)
{
  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);
  struct PgpKeyInfo *keys = NULL;
  struct PgpKeyInfo *matches = NULL;
  struct PgpKeyInfo **last = &matches;
  struct PgpKeyInfo *k = NULL, *kn = NULL;
  struct PgpUid *a = NULL;
  size_t l;
  const char *ps = NULL, *pl = NULL, *pfcopy = NULL, *phint = NULL;

  char *p = strdup(cp); // mutt_str_dup converts "" into NULL, see #1809
  l = mutt_str_len(p);
  if ((l > 0) && (p[l - 1] == '!'))
    p[l - 1] = 0;

  mutt_message(_("Looking for keys matching \"%s\"..."), p);

  pfcopy = crypt_get_fingerprint_or_id(p, &phint, &pl, &ps);
  pgp_add_string_to_hints(phint, &hints);
  keys = pgp_get_candidates(keyring, &hints);
  mutt_list_free(&hints);

  for (k = keys; k; k = kn)
  {
    kn = k->next;
    if (abilities && !(k->flags & abilities))
      continue;

    /* This shouldn't happen, but keys without any addresses aren't selectable
     * in pgp_select_key().  */
    if (!k->address)
      continue;

    bool match = false;

    mutt_debug(LL_DEBUG5, "matching \"%s\" against key %s:\n", p, pgp_long_keyid(k));

    if ((*p == '\0') || (pfcopy && mutt_istr_equal(pfcopy, k->fingerprint)) ||
        (pl && mutt_istr_equal(pl, pgp_long_keyid(k))) ||
        (ps && mutt_istr_equal(ps, pgp_short_keyid(k))))
    {
      mutt_debug(LL_DEBUG5, "\t\tmatch #1\n");
      match = true;
    }
    else
    {
      for (a = k->address; a; a = a->next)
      {
        mutt_debug(LL_DEBUG5, "matching \"%s\" against key %s, \"%s\":\n", p,
                   pgp_long_keyid(k), NONULL(a->addr));
        if (mutt_istr_find(a->addr, p))
        {
          mutt_debug(LL_DEBUG5, "\t\tmatch #2\n");
          match = true;
          break;
        }
      }
    }

    if (match)
    {
      *last = pgp_principal_key(k);
      kn = pgp_remove_key(&keys, *last);
      last = pgp_get_lastp(k);
    }
  }

  pgp_key_free(&keys);

  if (matches)
  {
    k = pgp_select_key(matches, NULL, p);
    if (k)
      pgp_remove_key(&matches, k);
    pgp_key_free(&matches);
  }
  else
  {
    k = NULL;
  }

  FREE(&pfcopy);
  FREE(&p);
  return k;
}
