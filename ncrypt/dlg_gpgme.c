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
 * | Name                       | Type               | See Also               |
 * | :------------------------- | :----------------- | :--------------------- |
 * | GPGME Key Selection Dialog | WT_DLG_CRYPT_GPGME | dlg_select_gpgme_key() |
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
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
#include "crypt_gpgme.h"
#include "format_flags.h"
#include "keymap.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

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
 * struct DnArray - An X500 Distinguished Name
 */
struct DnArray
{
  char *key;   ///< Key
  char *value; ///< Value
};

static const char *const KeyInfoPrompts[] = {
  /* L10N: The following are the headers for the "verify key" output from the
     GPGME key selection menu (bound to "c" in the key selection menu).
     They will be automatically aligned. */
  N_("Name: "),      N_("aka: "),       N_("Valid From: "),  N_("Valid To: "),
  N_("Key Type: "),  N_("Key Usage: "), N_("Fingerprint: "), N_("Serial-No: "),
  N_("Issued By: "), N_("Subkey: ")
};

int KeyInfoPadding[KIP_MAX] = { 0 };

/**
 * print_utf8 - Write a UTF-8 string to a file
 * @param fp  File to write to
 * @param buf Buffer to read from
 * @param len Length to read
 *
 * Convert the character set.
 */
static void print_utf8(FILE *fp, const char *buf, size_t len)
{
  char *tstr = mutt_mem_malloc(len + 1);
  memcpy(tstr, buf, len);
  tstr[len] = 0;

  /* fromcode "utf-8" is sure, so we don't want
   * charset-hook corrections: flags must be 0.  */
  const char *const c_charset = cs_subset_string(NeoMutt->sub, "charset");
  mutt_ch_convert_string(&tstr, "utf-8", c_charset, MUTT_ICONV_NO_FLAGS);
  fputs(tstr, fp);
  FREE(&tstr);
}

/**
 * crypt_key_is_valid - Is the key valid
 * @param k Key to test
 * @retval true Key is valid
 */
static bool crypt_key_is_valid(struct CryptKeyInfo *k)
{
  if (k->flags & KEYFLAG_CANTUSE)
    return false;
  return true;
}

/**
 * crypt_compare_key_address - Compare Key addresses and IDs for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_key_address(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;

  int r = mutt_istr_cmp((*s)->uid, (*t)->uid);
  if (r != 0)
    return r > 0;
  return mutt_istr_cmp(crypt_fpr_or_lkeyid(*s), crypt_fpr_or_lkeyid(*t)) > 0;
}

/**
 * crypt_compare_address_qsort - Compare the addresses of two keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_address_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !crypt_compare_key_address(a, b) :
                                            crypt_compare_key_address(a, b);
}

/**
 * crypt_compare_keyid - Compare Key IDs and addresses for sorting
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_keyid(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;

  int r = mutt_istr_cmp(crypt_fpr_or_lkeyid(*s), crypt_fpr_or_lkeyid(*t));
  if (r != 0)
    return r > 0;
  return mutt_istr_cmp((*s)->uid, (*t)->uid) > 0;
}

/**
 * crypt_compare_keyid_qsort - Compare the IDs of two keys
 * @param a First key ID
 * @param b Second key ID
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_keyid_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !crypt_compare_keyid(a, b) :
                                            crypt_compare_keyid(a, b);
}

/**
 * crypt_compare_key_date - Compare Key creation dates and addresses for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_key_date(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;
  unsigned long ts = 0, tt = 0;

  if ((*s)->kobj->subkeys && ((*s)->kobj->subkeys->timestamp > 0))
    ts = (*s)->kobj->subkeys->timestamp;
  if ((*t)->kobj->subkeys && ((*t)->kobj->subkeys->timestamp > 0))
    tt = (*t)->kobj->subkeys->timestamp;

  if (ts > tt)
    return 1;
  if (ts < tt)
    return 0;

  return mutt_istr_cmp((*s)->uid, (*t)->uid) > 0;
}

/**
 * crypt_compare_date_qsort - Compare the dates of two keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_date_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !crypt_compare_key_date(a, b) :
                                            crypt_compare_key_date(a, b);
}

/**
 * crypt_compare_key_trust - Compare the trust of keys for sorting
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 *
 * Compare two trust values, the key length, the creation dates. the addresses
 * and the key IDs.
 */
static int crypt_compare_key_trust(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;
  unsigned long ts = 0, tt = 0;

  int r = (((*s)->flags & KEYFLAG_RESTRICTIONS) - ((*t)->flags & KEYFLAG_RESTRICTIONS));
  if (r != 0)
    return r > 0;

  ts = (*s)->validity;
  tt = (*t)->validity;
  r = (tt - ts);
  if (r != 0)
    return r < 0;

  if ((*s)->kobj->subkeys)
    ts = (*s)->kobj->subkeys->length;
  if ((*t)->kobj->subkeys)
    tt = (*t)->kobj->subkeys->length;
  if (ts != tt)
    return ts > tt;

  if ((*s)->kobj->subkeys && ((*s)->kobj->subkeys->timestamp > 0))
    ts = (*s)->kobj->subkeys->timestamp;
  if ((*t)->kobj->subkeys && ((*t)->kobj->subkeys->timestamp > 0))
    tt = (*t)->kobj->subkeys->timestamp;
  if (ts > tt)
    return 1;
  if (ts < tt)
    return 0;

  r = mutt_istr_cmp((*s)->uid, (*t)->uid);
  if (r != 0)
    return r > 0;
  return mutt_istr_cmp(crypt_fpr_or_lkeyid((*s)), crypt_fpr_or_lkeyid((*t))) > 0;
}

/**
 * crypt_compare_trust_qsort - Compare the trust levels of two keys
 * @param a First key
 * @param b Second key
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int crypt_compare_trust_qsort(const void *a, const void *b)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_sort_keys");
  return (c_pgp_sort_keys & SORT_REVERSE) ? !crypt_compare_key_trust(a, b) :
                                            crypt_compare_key_trust(a, b);
}

/**
 * print_dn_part - Print the X.500 Distinguished Name
 * @param fp  File to write to
 * @param dn  Distinguished Name
 * @param key Key string
 * @retval true  Any DN keys match the given key string
 * @retval false Otherwise
 *
 * Print the X.500 Distinguished Name part KEY from the array of parts DN to FP.
 */
static bool print_dn_part(FILE *fp, struct DnArray *dn, const char *key)
{
  bool any = false;

  for (; dn->key; dn++)
  {
    if (strcmp(dn->key, key) == 0)
    {
      if (any)
        fputs(" + ", fp);
      print_utf8(fp, dn->value, strlen(dn->value));
      any = true;
    }
  }
  return any;
}

/**
 * print_dn_parts - Print all parts of a DN in a standard sequence
 * @param fp File to write to
 * @param dn Array of Distinguished Names
 */
static void print_dn_parts(FILE *fp, struct DnArray *dn)
{
  static const char *const stdpart[] = {
    "CN", "OU", "O", "STREET", "L", "ST", "C", NULL,
  };
  bool any = false;
  bool any2 = false;

  for (int i = 0; stdpart[i]; i++)
  {
    if (any)
      fputs(", ", fp);
    any = print_dn_part(fp, dn, stdpart[i]);
  }
  /* now print the rest without any specific ordering */
  for (; dn->key; dn++)
  {
    int i;
    for (i = 0; stdpart[i]; i++)
    {
      if (strcmp(dn->key, stdpart[i]) == 0)
        break;
    }
    if (!stdpart[i])
    {
      if (any)
        fputs(", ", fp);
      if (!any2)
        fputs("(", fp);
      any = print_dn_part(fp, dn, dn->key);
      any2 = true;
    }
  }
  if (any2)
    fputs(")", fp);
}

/**
 * parse_dn_part - Parse an RDN
 * @param array Array for results
 * @param str   String to parse
 * @retval ptr First character after Distinguished Name
 *
 * This is a helper to parse_dn()
 */
static const char *parse_dn_part(struct DnArray *array, const char *str)
{
  const char *s = NULL, *s1 = NULL;
  size_t n;
  char *p = NULL;

  /* parse attribute type */
  for (s = str + 1; (s[0] != '\0') && (s[0] != '='); s++)
    ; // do nothing

  if (s[0] == '\0')
    return NULL; /* error */
  n = s - str;
  if (n == 0)
    return NULL; /* empty key */
  array->key = mutt_mem_malloc(n + 1);
  p = array->key;
  memcpy(p, str, n); /* fixme: trim trailing spaces */
  p[n] = 0;
  str = s + 1;

  if (*str == '#')
  { /* hexstring */
    str++;
    for (s = str; isxdigit(*s); s++)
      s++;
    n = s - str;
    if ((n == 0) || (n & 1))
      return NULL; /* empty or odd number of digits */
    n /= 2;
    p = mutt_mem_malloc(n + 1);
    array->value = (char *) p;
    for (s1 = str; n; s1 += 2, n--)
      sscanf(s1, "%2hhx", (unsigned char *) p++);
    *p = '\0';
  }
  else
  { /* regular v3 quoted string */
    for (n = 0, s = str; *s; s++)
    {
      if (*s == '\\')
      { /* pair */
        s++;
        if ((*s == ',') || (*s == '=') || (*s == '+') || (*s == '<') || (*s == '>') ||
            (*s == '#') || (*s == ';') || (*s == '\\') || (*s == '\"') || (*s == ' '))
        {
          n++;
        }
        else if (isxdigit(s[0]) && isxdigit(s[1]))
        {
          s++;
          n++;
        }
        else
          return NULL; /* invalid escape sequence */
      }
      else if (*s == '\"')
        return NULL; /* invalid encoding */
      else if ((*s == ',') || (*s == '=') || (*s == '+') || (*s == '<') ||
               (*s == '>') || (*s == '#') || (*s == ';'))
      {
        break;
      }
      else
        n++;
    }

    p = mutt_mem_malloc(n + 1);
    array->value = (char *) p;
    for (s = str; n; s++, n--)
    {
      if (*s == '\\')
      {
        s++;
        if (isxdigit(*s))
        {
          sscanf(s, "%2hhx", (unsigned char *) p++);
          s++;
        }
        else
          *p++ = *s;
      }
      else
        *p++ = *s;
    }
    *p = '\0';
  }
  return s;
}

/**
 * parse_dn - Parse a DN and return an array-ized one
 * @param str String to parse
 * @retval ptr Array of Distinguished Names
 *
 * This is not a validating parser and it does not support any old-stylish
 * syntax; GPGME is expected to return only rfc2253 compatible strings.
 */
static struct DnArray *parse_dn(const char *str)
{
  struct DnArray *array = NULL;
  size_t arrayidx, arraysize;

  arraysize = 7; /* C,ST,L,O,OU,CN,email */
  array = mutt_mem_malloc((arraysize + 1) * sizeof(*array));
  arrayidx = 0;
  while (*str)
  {
    while (str[0] == ' ')
      str++;
    if (str[0] == '\0')
      break; /* ready */
    if (arrayidx >= arraysize)
    {
      /* neomutt lacks a real mutt_mem_realloc - so we need to copy */
      arraysize += 5;
      struct DnArray *a2 = mutt_mem_malloc((arraysize + 1) * sizeof(*array));
      for (int i = 0; i < arrayidx; i++)
      {
        a2[i].key = array[i].key;
        a2[i].value = array[i].value;
      }
      FREE(&array);
      array = a2;
    }
    array[arrayidx].key = NULL;
    array[arrayidx].value = NULL;
    str = parse_dn_part(array + arrayidx, str);
    arrayidx++;
    if (!str)
      goto failure;
    while (str[0] == ' ')
      str++;
    if ((str[0] != '\0') && (str[0] != ',') && (str[0] != ';') && (str[0] != '+'))
      goto failure; /* invalid delimiter */
    if (str[0] != '\0')
      str++;
  }
  array[arrayidx].key = NULL;
  array[arrayidx].value = NULL;
  return array;

failure:
  for (int i = 0; i < arrayidx; i++)
  {
    FREE(&array[i].key);
    FREE(&array[i].value);
  }
  FREE(&array);
  return NULL;
}

/**
 * parse_and_print_user_id - Print a nice representation of the userid
 * @param fp     File to write to
 * @param userid String returned by GPGME key functions (utf-8 encoded)
 *
 * Make sure it is displayed in a proper way, which does mean to reorder some
 * parts for S/MIME's DNs.
 */
static void parse_and_print_user_id(FILE *fp, const char *userid)
{
  const char *s = NULL;

  if (*userid == '<')
  {
    s = strchr(userid + 1, '>');
    if (s)
      print_utf8(fp, userid + 1, s - userid - 1);
  }
  else if (*userid == '(')
    fputs(_("[Can't display this user ID (unknown encoding)]"), fp);
  else if (!isalnum(userid[0]))
    fputs(_("[Can't display this user ID (invalid encoding)]"), fp);
  else
  {
    struct DnArray *dn = parse_dn(userid);
    if (!dn)
      fputs(_("[Can't display this user ID (invalid DN)]"), fp);
    else
    {
      print_dn_parts(fp, dn);
      for (int i = 0; dn[i].key; i++)
      {
        FREE(&dn[i].key);
        FREE(&dn[i].value);
      }
      FREE(&dn);
    }
  }
}

/**
 * print_key_info - Verbose information about a key or certificate to a file
 * @param key Key to use
 * @param fp  File to write to
 */
static void print_key_info(gpgme_key_t key, FILE *fp)
{
  int idx;
  const char *s = NULL, *s2 = NULL;
  time_t tt = 0;
  char shortbuf[128];
  unsigned long aval = 0;
  const char *delim = NULL;
  gpgme_user_id_t uid = NULL;
  static int max_header_width = 0;

  if (max_header_width == 0)
  {
    for (int i = 0; i < KIP_MAX; i++)
    {
      KeyInfoPadding[i] = mutt_str_len(_(KeyInfoPrompts[i]));
      const int width = mutt_strwidth(_(KeyInfoPrompts[i]));
      if (max_header_width < width)
        max_header_width = width;
      KeyInfoPadding[i] -= width;
    }
    for (int i = 0; i < KIP_MAX; i++)
      KeyInfoPadding[i] += max_header_width;
  }

  bool is_pgp = (key->protocol == GPGME_PROTOCOL_OpenPGP);

  for (idx = 0, uid = key->uids; uid; idx++, uid = uid->next)
  {
    if (uid->revoked)
      continue;

    s = uid->uid;
    /* L10N: DOTFILL */

    if (idx == 0)
      fprintf(fp, "%*s", KeyInfoPadding[KIP_NAME], _(KeyInfoPrompts[KIP_NAME]));
    else
      fprintf(fp, "%*s", KeyInfoPadding[KIP_AKA], _(KeyInfoPrompts[KIP_AKA]));
    if (uid->invalid)
    {
      /* L10N: comes after the Name or aka if the key is invalid */
      fputs(_("[Invalid]"), fp);
      putc(' ', fp);
    }
    if (is_pgp)
      print_utf8(fp, s, strlen(s));
    else
      parse_and_print_user_id(fp, s);
    putc('\n', fp);
  }

  if (key->subkeys && (key->subkeys->timestamp > 0))
  {
    tt = key->subkeys->timestamp;

    mutt_date_localtime_format(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tt);
    fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_FROM],
            _(KeyInfoPrompts[KIP_VALID_FROM]), shortbuf);
  }

  if (key->subkeys && (key->subkeys->expires > 0))
  {
    tt = key->subkeys->expires;

    mutt_date_localtime_format(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tt);
    fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_TO],
            _(KeyInfoPrompts[KIP_VALID_TO]), shortbuf);
  }

  if (key->subkeys)
    s = gpgme_pubkey_algo_name(key->subkeys->pubkey_algo);
  else
    s = "?";

  s2 = is_pgp ? "PGP" : "X.509";

  if (key->subkeys)
    aval = key->subkeys->length;

  fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_TYPE], _(KeyInfoPrompts[KIP_KEY_TYPE]));
  /* L10N: This is printed after "Key Type: " and looks like this: PGP, 2048 bit RSA */
  fprintf(fp, ngettext("%s, %lu bit %s\n", "%s, %lu bit %s\n", aval), s2, aval, s);

  fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_USAGE], _(KeyInfoPrompts[KIP_KEY_USAGE]));
  delim = "";

  if (key_check_cap(key, KEY_CAP_CAN_ENCRYPT))
  {
    /* L10N: value in Key Usage: field */
    fprintf(fp, "%s%s", delim, _("encryption"));
    delim = _(", ");
  }
  if (key_check_cap(key, KEY_CAP_CAN_SIGN))
  {
    /* L10N: value in Key Usage: field */
    fprintf(fp, "%s%s", delim, _("signing"));
    delim = _(", ");
  }
  if (key_check_cap(key, KEY_CAP_CAN_CERTIFY))
  {
    /* L10N: value in Key Usage: field */
    fprintf(fp, "%s%s", delim, _("certification"));
  }
  putc('\n', fp);

  if (key->subkeys)
  {
    s = key->subkeys->fpr;
    fprintf(fp, "%*s", KeyInfoPadding[KIP_FINGERPRINT], _(KeyInfoPrompts[KIP_FINGERPRINT]));
    if (is_pgp && (strlen(s) == 40))
    {
      for (int i = 0; (s[0] != '\0') && (s[1] != '\0') && (s[2] != '\0') &&
                      (s[3] != '\0') && (s[4] != '\0');
           s += 4, i++)
      {
        putc(*s, fp);
        putc(s[1], fp);
        putc(s[2], fp);
        putc(s[3], fp);
        putc(' ', fp);
        if (i == 4)
          putc(' ', fp);
      }
    }
    else
    {
      for (int i = 0; (s[0] != '\0') && (s[1] != '\0') && (s[2] != '\0'); s += 2, i++)
      {
        putc(*s, fp);
        putc(s[1], fp);
        putc(is_pgp ? ' ' : ':', fp);
        if (is_pgp && (i == 7))
          putc(' ', fp);
      }
    }
    fprintf(fp, "%s\n", s);
  }

  if (key->issuer_serial)
  {
    s = key->issuer_serial;
    if (s)
    {
      fprintf(fp, "%*s0x%s\n", KeyInfoPadding[KIP_SERIAL_NO],
              _(KeyInfoPrompts[KIP_SERIAL_NO]), s);
    }
  }

  if (key->issuer_name)
  {
    s = key->issuer_name;
    if (s)
    {
      fprintf(fp, "%*s", KeyInfoPadding[KIP_ISSUED_BY], _(KeyInfoPrompts[KIP_ISSUED_BY]));
      parse_and_print_user_id(fp, s);
      putc('\n', fp);
    }
  }

  /* For PGP we list all subkeys. */
  if (is_pgp)
  {
    gpgme_subkey_t subkey = NULL;

    for (idx = 1, subkey = key->subkeys; subkey; idx++, subkey = subkey->next)
    {
      s = subkey->keyid;

      putc('\n', fp);
      if (strlen(s) == 16)
        s += 8; /* display only the short keyID */
      fprintf(fp, "%*s0x%s", KeyInfoPadding[KIP_SUBKEY], _(KeyInfoPrompts[KIP_SUBKEY]), s);
      if (subkey->revoked)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Revoked]"), fp);
      }
      if (subkey->invalid)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Invalid]"), fp);
      }
      if (subkey->expired)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Expired]"), fp);
      }
      if (subkey->disabled)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Disabled]"), fp);
      }
      putc('\n', fp);

      if (subkey->timestamp > 0)
      {
        tt = subkey->timestamp;

        mutt_date_localtime_format(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tt);
        fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_FROM],
                _(KeyInfoPrompts[KIP_VALID_FROM]), shortbuf);
      }

      if (subkey->expires > 0)
      {
        tt = subkey->expires;

        mutt_date_localtime_format(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tt);
        fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_TO],
                _(KeyInfoPrompts[KIP_VALID_TO]), shortbuf);
      }

      s = gpgme_pubkey_algo_name(subkey->pubkey_algo);

      aval = subkey->length;

      fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_TYPE], _(KeyInfoPrompts[KIP_KEY_TYPE]));
      /* L10N: This is printed after "Key Type: " and looks like this: PGP, 2048 bit RSA */
      fprintf(fp, ngettext("%s, %lu bit %s\n", "%s, %lu bit %s\n", aval), "PGP", aval, s);

      fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_USAGE], _(KeyInfoPrompts[KIP_KEY_USAGE]));
      delim = "";

      if (subkey->can_encrypt)
      {
        fprintf(fp, "%s%s", delim, _("encryption"));
        delim = _(", ");
      }
      if (subkey->can_sign)
      {
        fprintf(fp, "%s%s", delim, _("signing"));
        delim = _(", ");
      }
      if (subkey->can_certify)
      {
        fprintf(fp, "%s%s", delim, _("certification"));
      }
      putc('\n', fp);
    }
  }
}

/**
 * verify_key - Show detailed information about the selected key
 * @param key Key to show
 */
static void verify_key(struct CryptKeyInfo *key)
{
  const char *s = NULL;
  gpgme_ctx_t listctx = NULL;
  gpgme_error_t err;
  gpgme_key_t k = NULL;
  int maxdepth = 100;

  struct Buffer tempfile = mutt_buffer_make(PATH_MAX);
  mutt_buffer_mktemp(&tempfile);
  FILE *fp = mutt_file_fopen(mutt_buffer_string(&tempfile), "w");
  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }
  mutt_message(_("Collecting data..."));

  print_key_info(key->kobj, fp);

  listctx = create_gpgme_context(key->flags & KEYFLAG_ISX509);

  k = key->kobj;
  gpgme_key_ref(k);
  while ((s = k->chain_id) && k->subkeys && (strcmp(s, k->subkeys->fpr) != 0))
  {
    putc('\n', fp);
    err = gpgme_op_keylist_start(listctx, s, 0);
    gpgme_key_unref(k);
    k = NULL;
    if (err == 0)
      err = gpgme_op_keylist_next(listctx, &k);
    if (err != 0)
    {
      fprintf(fp, _("Error finding issuer key: %s\n"), gpgme_strerror(err));
      goto leave;
    }
    gpgme_op_keylist_end(listctx);

    print_key_info(k, fp);
    if (!--maxdepth)
    {
      putc('\n', fp);
      fputs(_("Error: certification chain too long - stopping here\n"), fp);
      break;
    }
  }

leave:
  gpgme_key_unref(k);
  gpgme_release(listctx);
  mutt_file_fclose(&fp);
  mutt_clear_error();
  char title[1024];
  snprintf(title, sizeof(title), _("Key ID: 0x%s"), crypt_keyid(key));

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = mutt_buffer_string(&tempfile);

  pview.banner = title;
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);

cleanup:
  mutt_buffer_dealloc(&tempfile);
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
 * | \%k     | Key id
 * | \%l     | Length
 * |         |
 * | \%A     | Algorithm of the principal key
 * | \%C     | Capabilities of the principal key
 * | \%F     | Flags of the principal key
 * | \%K     | Key id of the principal key
 * | \%L     | Length of the principal key
 */
static const char *crypt_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    intptr_t data, MuttFormatFlags flags)
{
  char fmt[128];
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  struct CryptEntry *entry = (struct CryptEntry *) data;
  struct CryptKeyInfo *key = entry->key;

  /*    if (isupper ((unsigned char) op)) */
  /*      key = pkey; */

  KeyFlags kflags = (key->flags /* | (pkey->flags & KEYFLAG_RESTRICTIONS)
                                 | uid->flags */);

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
        optional = false;
      break;

    case 'f':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, crypt_flags(kflags));
      }
      else if (!(kflags & KEYFLAG_RESTRICTIONS))
        optional = false;
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
        s = "x";
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
      bool do_locales = true;
      struct tm tm = { 0 };

      char *p = buf;

      const char *cp = src;
      if (*cp == '!')
      {
        do_locales = false;
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

      if (key->kobj->subkeys && (key->kobj->subkeys->timestamp > 0))
        tm = mutt_date_localtime(key->kobj->subkeys->timestamp);
      else
        tm = mutt_date_localtime(0); // Default to 1970-01-01

      if (!do_locales)
        setlocale(LC_TIME, "C");
      strftime(buf2, sizeof(buf2), buf, &tm);
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
 * crypt_make_entry - Format a menu item for the key selection list - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $pgp_entry_format, crypt_format_str()
 */
static void crypt_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct CryptKeyInfo **key_table = menu->mdata;
  struct CryptEntry entry;

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
 * gpgme_key_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int gpgme_key_window_observer(struct NotifyCallback *nc)
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

  notify_observer_remove(NeoMutt->notify, gpgme_key_config_observer, menu);
  notify_observer_remove(win_menu->notify, gpgme_key_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_select_gpgme_key - Get the user to select a key
 * @param[in]  keys         List of keys to select from
 * @param[in]  p            Address to match
 * @param[in]  s            Real name to display
 * @param[in]  app          Flags, e.g. #APPLICATION_PGP
 * @param[out] forced_valid Set to true if user overrode key's validity
 * @retval ptr Key selected by user
 *
 * Display a menu to select a key from the array of keys.
 */
struct CryptKeyInfo *dlg_select_gpgme_key(struct CryptKeyInfo *keys,
                                          struct Address *p, const char *s,
                                          unsigned int app, int *forced_valid)
{
  int keymax;
  int i;
  struct CryptKeyInfo *k = NULL;
  int (*f)(const void *, const void *);
  enum MenuType menu_to_use = MENU_GENERIC;
  bool unusable = false;

  *forced_valid = 0;

  /* build the key table */
  keymax = 0;
  i = 0;
  struct CryptKeyInfo **key_table = NULL;
  for (k = keys; k; k = k->next)
  {
    const bool c_pgp_show_unusable = cs_subset_bool(NeoMutt->sub, "pgp_show_unusable");
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
      f = crypt_compare_address_qsort;
      break;
    case SORT_DATE:
      f = crypt_compare_date_qsort;
      break;
    case SORT_KEYID:
      f = crypt_compare_keyid_qsort;
      break;
    case SORT_TRUST:
    default:
      f = crypt_compare_trust_qsort;
      break;
  }
  qsort(key_table, i, sizeof(struct CryptKeyInfo *), f);

  if (app & APPLICATION_PGP)
    menu_to_use = MENU_KEY_SELECT_PGP;
  else if (app & APPLICATION_SMIME)
    menu_to_use = MENU_KEY_SELECT_SMIME;

  struct MuttWindow *dlg = simple_dialog_new(menu_to_use, WT_DLG_CRYPT_GPGME, GpgmeHelp);

  struct Menu *menu = dlg->wdata;
  menu->max = i;
  menu->make_entry = crypt_make_entry;
  menu->mdata = key_table;
  menu->mdata_free = gpgme_key_table_free;

  struct MuttWindow *win_menu = menu->win;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->notify, NT_CONFIG, gpgme_key_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, gpgme_key_window_observer, win_menu);

  {
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
         e.g. "S/MIME keys matching <me@mutt.org>" */
      snprintf(buf, sizeof(buf), _("%s <%s>"), ts, p->mailbox);
    }
    else
    {
      /* L10N: e.g. 'S/MIME keys matching "Michael Elkins".' */
      snprintf(buf, sizeof(buf), _("%s \"%s\""), ts, s);
    }

    struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
    sbar_set_title(sbar, buf);
  }

  mutt_clear_error();
  k = NULL;

  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  int rc;
  do
  {
    rc = FR_UNKNOWN;
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(menu_to_use);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(menu_to_use);
      continue;
    }
    mutt_clear_error();

    *forced_valid = 0;
    switch (op)
    {
      case OP_VERIFY_KEY:
      {
        const int index = menu_get_index(menu);
        struct CryptKeyInfo *cur_key = key_table[index];
        verify_key(cur_key);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        continue;
      }

      case OP_VIEW_ID:
      {
        const int index = menu_get_index(menu);
        struct CryptKeyInfo *cur_key = key_table[index];
        mutt_message("%s", cur_key->uid);
        continue;
      }

      case OP_GENERIC_SELECT_ENTRY:
      {
        const int index = menu_get_index(menu);
        struct CryptKeyInfo *cur_key = key_table[index];
        /* FIXME make error reporting more verbose - this should be
         * easy because GPGME provides more information */
        if (OptPgpCheckTrust)
        {
          if (!crypt_key_is_valid(cur_key))
          {
            mutt_error(_("This key can't be used: expired/disabled/revoked"));
            continue;
          }
        }

        if (OptPgpCheckTrust && (!crypt_id_is_valid(cur_key) || !crypt_id_is_strong(cur_key)))
        {
          const char *warn_s = NULL;
          char buf2[1024];

          if (cur_key->flags & KEYFLAG_CANTUSE)
          {
            warn_s = _("ID is expired/disabled/revoked. Do you really want to use the key?");
          }
          else
          {
            warn_s = "??";
            switch (cur_key->validity)
            {
              case GPGME_VALIDITY_NEVER:
                warn_s = _("ID is not valid. Do you really want to use the key?");
                break;
              case GPGME_VALIDITY_MARGINAL:
                warn_s = _("ID is only marginally valid. Do you really want to use the key?");
                break;
              case GPGME_VALIDITY_FULL:
              case GPGME_VALIDITY_ULTIMATE:
                break;
              case GPGME_VALIDITY_UNKNOWN:
              case GPGME_VALIDITY_UNDEFINED:
                warn_s = _("ID has undefined validity. Do you really want to use the key?");
                break;
            }
          }

          snprintf(buf2, sizeof(buf2), "%s", warn_s);

          if (mutt_yesorno(buf2, MUTT_NO) != MUTT_YES)
          {
            mutt_clear_error();
            continue;
          }

          /* A '!' is appended to a key in find_keys() when forced_valid is
           * set.  Prior to GPGME 1.11.0, encrypt_gpgme_object() called
           * create_recipient_set() which interpreted the '!' to mean set
           * GPGME_VALIDITY_FULL for the key.
           *
           * Starting in GPGME 1.11.0, we now use a '\n' delimited recipient
           * string, which is passed directly to the gpgme_op_encrypt_ext()
           * function.  This allows to use the original meaning of '!' to
           * force a subkey use. */
#if (GPGME_VERSION_NUMBER < 0x010b00) /* GPGME < 1.11.0 */
          *forced_valid = 1;
#endif
        }

        k = crypt_copy_key(cur_key);
        rc = FR_DONE;
        break;
      }

      case OP_EXIT:
        k = NULL;
        rc = FR_DONE;
        break;
    }

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (rc != FR_DONE);
  // ---------------------------------------------------------------------------

  simple_dialog_free(&dlg);
  return k;
}
