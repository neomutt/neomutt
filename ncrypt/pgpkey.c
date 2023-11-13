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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "pgpkey.h"
#include "lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "send/lib.h"
#include "crypt.h"
#include "globals.h"
#include "gnupgparse.h"
#include "mutt_logging.h"
#include "pgpinvoke.h"
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
  struct PgpCache *next; ///< Linked list
};

/// Cache of GPGME keys
static struct PgpCache *IdDefaults = NULL;

// clang-format off
typedef uint8_t PgpKeyValidFlags; ///< Flags for valid Pgp Key fields, e.g. #PGP_KV_VALID
#define PGP_KV_NO_FLAGS       0   ///< No flags are set
#define PGP_KV_VALID    (1 << 0)  ///< PGP Key ID is valid
#define PGP_KV_ADDR     (1 << 1)  ///< PGP Key address is valid
#define PGP_KV_STRING   (1 << 2)  ///< PGP Key name string is valid
#define PGP_KV_STRONGID (1 << 3)  ///< PGP Key is strong
// clang-format on

#define PGP_KV_MATCH (PGP_KV_ADDR | PGP_KV_STRING)

/**
 * pgp_principal_key - Get the main (parent) PGP key
 * @param key Key to start with
 * @retval ptr PGP Key
 */
struct PgpKeyInfo *pgp_principal_key(struct PgpKeyInfo *key)
{
  if (key->flags & KEYFLAG_SUBKEY && key->parent)
    return key->parent;
  return key;
}

/**
 * pgp_key_is_valid - Is a PGP key valid?
 * @param k Key to examine
 * @retval true Key is valid
 */
bool pgp_key_is_valid(struct PgpKeyInfo *k)
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
 * @retval true Key is strong
 */
bool pgp_id_is_strong(struct PgpUid *uid)
{
  if ((uid->trust & 3) < 3)
    return false;
  /* else */
  return true;
}

/**
 * pgp_id_is_valid - Is a PGP key valid
 * @param uid UID of a PGP key
 * @retval true Key is valid
 */
bool pgp_id_is_valid(struct PgpUid *uid)
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

  if (addr->mailbox && u_addr->mailbox && buf_istr_equal(addr->mailbox, u_addr->mailbox))
  {
    flags |= PGP_KV_ADDR;
  }

  if (addr->personal && u_addr->personal &&
      buf_istr_equal(addr->personal, u_addr->personal))
  {
    flags |= PGP_KV_STRING;
  }

  return flags;
}

/**
 * pgp_ask_for_key - Ask the user for a PGP key
 * @param tag       Prompt for the user
 * @param whatfor   Use for key, e.g. "signing"
 * @param abilities Abilities to match, see #KeyFlags
 * @param keyring   PGP keyring to use
 * @retval ptr Selected PGP key
 */
struct PgpKeyInfo *pgp_ask_for_key(char *tag, const char *whatfor,
                                   KeyFlags abilities, enum PgpRing keyring)
{
  struct PgpKeyInfo *key = NULL;
  struct PgpCache *l = NULL;
  struct Buffer *resp = buf_pool_get();

  mutt_clear_error();

  if (whatfor)
  {
    for (l = IdDefaults; l; l = l->next)
    {
      if (mutt_istr_equal(whatfor, l->what))
      {
        buf_strcpy(resp, l->dflt);
        break;
      }
    }
  }

  while (true)
  {
    buf_reset(resp);
    if (mw_get_field(tag, resp, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) != 0)
    {
      goto done;
    }

    if (whatfor)
    {
      if (l)
      {
        mutt_str_replace(&l->dflt, buf_string(resp));
      }
      else
      {
        l = mutt_mem_malloc(sizeof(struct PgpCache));
        l->next = IdDefaults;
        IdDefaults = l;
        l->what = mutt_str_dup(whatfor);
        l->dflt = buf_strdup(resp);
      }
    }

    key = pgp_getkeybystr(buf_string(resp), abilities, keyring);
    if (key)
      goto done;

    mutt_error(_("No matching keys found for \"%s\""), buf_string(resp));
  }

done:
  buf_pool_release(&resp);
  return key;
}

/**
 * pgp_class_make_key_attachment - Generate a public key attachment - Implements CryptModuleSpecs::pgp_make_key_attachment() - @ingroup crypto_pgp_make_key_attachment
 */
struct Body *pgp_class_make_key_attachment(void)
{
  struct Body *att = NULL;
  char buf[1024] = { 0 };
  char tmp[256] = { 0 };
  struct stat st = { 0 };
  pid_t pid;
  OptPgpCheckTrust = false;
  struct Buffer *tempf = NULL;

  struct PgpKeyInfo *key = pgp_ask_for_key(_("Please enter the key ID: "), NULL,
                                           KEYFLAG_NO_FLAGS, PGP_PUBRING);

  if (!key)
    return NULL;

  snprintf(tmp, sizeof(tmp), "0x%s", pgp_fpr_or_lkeyid(pgp_principal_key(key)));
  pgp_key_free(&key);

  tempf = buf_pool_get();
  buf_mktemp(tempf);
  FILE *fp_tmp = mutt_file_fopen(buf_string(tempf), "w");
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
    unlink(buf_string(tempf));
    goto cleanup;
  }

  mutt_message(_("Invoking PGP..."));

  pid = pgp_invoke_export(NULL, NULL, NULL, -1, fileno(fp_tmp), fileno(fp_null), tmp);
  if (pid == -1)
  {
    mutt_perror(_("Can't create filter"));
    unlink(buf_string(tempf));
    mutt_file_fclose(&fp_tmp);
    mutt_file_fclose(&fp_null);
    goto cleanup;
  }

  filter_wait(pid);

  mutt_file_fclose(&fp_tmp);
  mutt_file_fclose(&fp_null);

  att = mutt_body_new();
  att->filename = buf_strdup(tempf);
  att->unlink = true;
  att->use_disp = false;
  att->type = TYPE_APPLICATION;
  att->subtype = mutt_str_dup("pgp-keys");
  snprintf(buf, sizeof(buf), _("PGP Key %s"), tmp);
  att->description = mutt_str_dup(buf);
  mutt_update_encoding(att, NeoMutt->sub);

  stat(buf_string(tempf), &st);
  att->length = st.st_size;

cleanup:
  buf_pool_release(&tempf);
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

  for (char *t = strtok(scratch, " ,.:\"()<>\n"); t; t = strtok(NULL, " ,.:\"()<>\n"))
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
    mutt_list_insert_tail(&hints, buf_strdup(a->mailbox));
  if (a->personal)
    pgp_add_string_to_hints(buf_string(a->personal), &hints);

  if (!oppenc_mode)
    mutt_message(_("Looking for keys matching \"%s\"..."), buf_string(a->mailbox));
  keys = pgp_get_candidates(keyring, &hints);

  mutt_list_free(&hints);

  if (!keys)
    return NULL;

  mutt_debug(LL_DEBUG5, "looking for %s <%s>\n", buf_string(a->personal),
             buf_string(a->mailbox));

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
      const bool c_crypt_opportunistic_encrypt_strong_keys =
          cs_subset_bool(NeoMutt->sub, "crypt_opportunistic_encrypt_strong_keys");
      if (the_strong_valid_key)
      {
        pgp_remove_key(&matches, the_strong_valid_key);
        k = the_strong_valid_key;
      }
      else if (a_valid_addrmatch_key && !c_crypt_opportunistic_encrypt_strong_keys)
      {
        pgp_remove_key(&matches, a_valid_addrmatch_key);
        k = a_valid_addrmatch_key;
      }
      else
      {
        k = NULL;
      }
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
      k = dlg_pgp(matches, a, NULL);
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
     * in dlg_pgp().  */
    if (!k->address)
      continue;

    bool match = false;

    mutt_debug(LL_DEBUG5, "matching \"%s\" against key %s:\n", p, pgp_long_keyid(k));

    if ((*p == '\0') || (pfcopy && mutt_istr_equal(pfcopy, k->fingerprint)) ||
        (pl && mutt_istr_equal(pl, pgp_long_keyid(k))) ||
        (ps && mutt_istr_equal(ps, pgp_short_keyid(k))))
    {
      mutt_debug(LL_DEBUG5, "        match #1\n");
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
          mutt_debug(LL_DEBUG5, "        match #2\n");
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
    k = dlg_pgp(matches, NULL, p);
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
