/**
 * @file
 * Gpgme functions
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
 * @page crypt_gpgme_functions Gpgme functions
 *
 * Gpgme functions
 */

#include "config.h"
#include <ctype.h>
#include <gpgme.h>
#include <langinfo.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "gpgme_functions.h"
#include "lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "question/lib.h"
#include "crypt_gpgme.h"
#include "globals.h"
#include "mutt_logging.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/// Number of padding spaces needed after each of the strings in #KeyInfoPrompts after translation
int KeyInfoPadding[KIP_MAX] = { 0 };

/// Names of header fields used in the pgp key display, e.g. Name:, Fingerprint:
static const char *const KeyInfoPrompts[] = {
  /* L10N: The following are the headers for the "verify key" output from the
     GPGME key selection menu (bound to "c" in the key selection menu).
     They will be automatically aligned. */
  N_("Name: "),      N_("aka: "),       N_("Valid From: "),  N_("Valid To: "),
  N_("Key Type: "),  N_("Key Usage: "), N_("Fingerprint: "), N_("Serial-No: "),
  N_("Issued By: "), N_("Subkey: ")
};

/**
 * struct DnArray - An X500 Distinguished Name
 */
struct DnArray
{
  char *key;   ///< Key
  char *value; ///< Value
};

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
  mutt_ch_convert_string(&tstr, "utf-8", cc_charset(), MUTT_ICONV_NO_FLAGS);
  fputs(tstr, fp);
  FREE(&tstr);
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
    if (mutt_str_equal(dn->key, key))
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
      if (mutt_str_equal(dn->key, stdpart[i]))
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
        {
          return NULL; /* invalid escape sequence */
        }
      }
      else if (*s == '\"')
      {
        return NULL; /* invalid encoding */
      }
      else if ((*s == ',') || (*s == '=') || (*s == '+') || (*s == '<') ||
               (*s == '>') || (*s == '#') || (*s == ';'))
      {
        break;
      }
      else
      {
        n++;
      }
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
        {
          *p++ = *s;
        }
      }
      else
      {
        *p++ = *s;
      }
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
  {
    fputs(_("[Can't display this user ID (unknown encoding)]"), fp);
  }
  else if (!isalnum(userid[0]))
  {
    fputs(_("[Can't display this user ID (invalid encoding)]"), fp);
  }
  else
  {
    struct DnArray *dn = parse_dn(userid);
    if (dn)
    {
      print_dn_parts(fp, dn);
      for (int i = 0; dn[i].key; i++)
      {
        FREE(&dn[i].key);
        FREE(&dn[i].value);
      }
      FREE(&dn);
    }
    else
    {
      fputs(_("[Can't display this user ID (invalid DN)]"), fp);
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
  char shortbuf[128] = { 0 };
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
    fprintf(fp, "%*s0x%s\n", KeyInfoPadding[KIP_SERIAL_NO],
            _(KeyInfoPrompts[KIP_SERIAL_NO]), s);
  }

  if (key->issuer_name)
  {
    s = key->issuer_name;
    fprintf(fp, "%*s", KeyInfoPadding[KIP_ISSUED_BY], _(KeyInfoPrompts[KIP_ISSUED_BY]));
    parse_and_print_user_id(fp, s);
    putc('\n', fp);
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

  struct Buffer tempfile = buf_make(PATH_MAX);
  buf_mktemp(&tempfile);
  FILE *fp = mutt_file_fopen(buf_string(&tempfile), "w");
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
  while ((s = k->chain_id) && k->subkeys && !mutt_str_equal(s, k->subkeys->fpr))
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
  char title[1024] = { 0 };
  snprintf(title, sizeof(title), _("Key ID: 0x%s"), crypt_keyid(key));

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(&tempfile);

  pview.banner = title;
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);

cleanup:
  buf_dealloc(&tempfile);
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

// -----------------------------------------------------------------------------

/**
 * op_exit - Exit this menu - Implements ::gpgme_function_t - @ingroup gpgme_function_api
 */
static int op_exit(struct GpgmeData *gd, int op)
{
  gd->done = true;
  return FR_SUCCESS;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::gpgme_function_t - @ingroup gpgme_function_api
 */
static int op_generic_select_entry(struct GpgmeData *gd, int op)
{
  const int index = menu_get_index(gd->menu);
  struct CryptKeyInfo *cur_key = gd->key_table[index];
  /* FIXME make error reporting more verbose - this should be
   * easy because GPGME provides more information */
  if (OptPgpCheckTrust)
  {
    if (!crypt_key_is_valid(cur_key))
    {
      mutt_error(_("This key can't be used: expired/disabled/revoked"));
      return FR_ERROR;
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

    if (query_yesorno(buf2, MUTT_NO) != MUTT_YES)
    {
      mutt_clear_error();
      return FR_NO_ACTION;
    }
  }

  gd->key = crypt_copy_key(cur_key);
  gd->done = true;
  return FR_SUCCESS;
}

/**
 * op_verify_key - Verify a PGP public key - Implements ::gpgme_function_t - @ingroup gpgme_function_api
 */
static int op_verify_key(struct GpgmeData *gd, int op)
{
  const int index = menu_get_index(gd->menu);
  struct CryptKeyInfo *cur_key = gd->key_table[index];
  verify_key(cur_key);
  menu_queue_redraw(gd->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_view_id - View the key's user id - Implements ::gpgme_function_t - @ingroup gpgme_function_api
 */
static int op_view_id(struct GpgmeData *gd, int op)
{
  const int index = menu_get_index(gd->menu);
  struct CryptKeyInfo *cur_key = gd->key_table[index];
  mutt_message("%s", cur_key->uid);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * GpgmeFunctions - All the NeoMutt functions that the Gpgme supports
 */
static const struct GpgmeFunction GpgmeFunctions[] = {
  // clang-format off
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { OP_VERIFY_KEY,             op_verify_key },
  { OP_VIEW_ID,                op_view_id },
  { 0, NULL },
  // clang-format on
};

/**
 * gpgme_function_dispatcher - Perform a Gpgme function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int gpgme_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return FR_ERROR;

  struct GpgmeData *gd = dlg->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; GpgmeFunctions[i].op != OP_NULL; i++)
  {
    const struct GpgmeFunction *fn = &GpgmeFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(gd, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
