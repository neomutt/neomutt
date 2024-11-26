/**
 * @file
 * Ncrypt Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page ncrypt_expando_pgp Ncrypt Expando definitions
 *
 * Ncrypt Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "expando_pgp.h"
#include "lib.h"
#include "expando/lib.h"
#include "pgp.h"
#include "pgpkey.h"
#include "pgplib.h"

/// Characters used to show the trust level for PGP keys
static const char TrustFlags[] = "?- +";

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
 * pgp_entry_pgp_date_num - PGP: Date of the key - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long pgp_entry_pgp_date_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  const struct PgpKeyInfo *key = uid->parent;

  return key->gen_time;
}

/**
 * pgp_entry_pgp_date - PGP: Date of the key - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_date(const struct ExpandoNode *node, void *data,
                               MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  const struct PgpKeyInfo *key = uid->parent;

  bool use_c_locale = false;
  const char *text = node->text;
  if (*text == '!')
  {
    use_c_locale = true;
    text++;
  }

  char tmp[128] = { 0 };
  if (use_c_locale)
  {
    mutt_date_localtime_format_locale(tmp, sizeof(tmp), text, key->gen_time,
                                      NeoMutt->time_c_locale);
  }
  else
  {
    mutt_date_localtime_format(tmp, sizeof(tmp), text, key->gen_time);
  }

  buf_strcpy(buf, tmp);
}

/**
 * pgp_entry_pgp_n_num - PGP: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long pgp_entry_pgp_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct PgpEntry *entry = data;
  return entry->num;
}

/**
 * pgp_entry_pgp_t - PGP: Trust/validity - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_t(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;

  buf_printf(buf, "%c", TrustFlags[uid->trust & 0x03]);
}

/**
 * pgp_entry_pgp_u - PGP: User id - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_u(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;

  const char *s = uid->addr;
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_a - PGP: Key Algorithm - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_a(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  const struct PgpKeyInfo *key = uid->parent;

  const char *s = key->algorithm;
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_A - PGP: Principal Key Algorithm - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_A(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;
  struct PgpKeyInfo *pkey = pgp_principal_key(key);

  const char *s = pkey->algorithm;
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_c - PGP: Key Capabilities - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_c(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  const struct PgpKeyInfo *key = uid->parent;

  const KeyFlags kflags = key->flags | uid->flags;

  const char *s = pgp_key_abilities(kflags);
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_C - PGP: Principal Key Capabilities - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_C(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;
  struct PgpKeyInfo *pkey = pgp_principal_key(key);

  const KeyFlags kflags = (pkey->flags & KEYFLAG_RESTRICTIONS) | uid->flags;

  const char *s = pgp_key_abilities(kflags);
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_f - PGP: Key Flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_f(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  const struct PgpKeyInfo *key = uid->parent;

  const KeyFlags kflags = key->flags | uid->flags;

  buf_printf(buf, "%c", pgp_flags(kflags));
}

/**
 * pgp_entry_pgp_F - PGP: Principal Key Flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_F(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;
  struct PgpKeyInfo *pkey = pgp_principal_key(key);

  const KeyFlags kflags = (pkey->flags & KEYFLAG_RESTRICTIONS) | uid->flags;

  buf_printf(buf, "%c", pgp_flags(kflags));
}

/**
 * pgp_entry_pgp_k - PGP: Key id - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_k(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;

  const char *s = pgp_this_keyid(key);
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_K - PGP: Principal Key id - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_pgp_K(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;
  struct PgpKeyInfo *pkey = pgp_principal_key(key);

  const char *s = pgp_this_keyid(pkey);
  buf_strcpy(buf, s);
}

/**
 * pgp_entry_pgp_l_num - PGP: Key length - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long pgp_entry_pgp_l_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  const struct PgpKeyInfo *key = uid->parent;

  return key->keylen;
}

/**
 * pgp_entry_pgp_L_num - PGP: Principal Key length - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long pgp_entry_pgp_L_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct PgpEntry *entry = data;
  const struct PgpUid *uid = entry->uid;
  struct PgpKeyInfo *key = uid->parent;
  struct PgpKeyInfo *pkey = pgp_principal_key(key);

  return pkey->keylen;
}

/**
 * pgp_entry_ignore - PGP: Field not supported - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_entry_ignore(const struct ExpandoNode *node, void *data,
                             MuttFormatFlags flags, struct Buffer *buf)
{
}

/**
 * PgpEntryRenderData- Callbacks for PGP Key Expandos
 *
 * @sa PgpEntryFormatDef, ExpandoDataGlobal, ExpandoDataPgp, ExpandoDataPgpKey
 */
const struct ExpandoRenderData PgpEntryRenderData[] = {
  // clang-format off
  { ED_PGP,     ED_PGP_NUMBER,            NULL,                pgp_entry_pgp_n_num },
  { ED_PGP,     ED_PGP_TRUST,             pgp_entry_pgp_t,     NULL },
  { ED_PGP,     ED_PGP_USER_ID,           pgp_entry_pgp_u,     NULL },
  { ED_PGP_KEY, ED_PGK_DATE,              pgp_entry_pgp_date,  pgp_entry_pgp_date_num },
  { ED_PGP_KEY, ED_PGK_KEY_ALGORITHM,     pgp_entry_pgp_a,     NULL },
  { ED_PGP_KEY, ED_PGK_KEY_CAPABILITIES,  pgp_entry_pgp_c,     NULL },
  { ED_PGP_KEY, ED_PGK_KEY_FINGERPRINT,   pgp_entry_ignore,    NULL },
  { ED_PGP_KEY, ED_PGK_KEY_FLAGS,         pgp_entry_pgp_f,     NULL },
  { ED_PGP_KEY, ED_PGK_KEY_ID,            pgp_entry_pgp_k,     NULL },
  { ED_PGP_KEY, ED_PGK_KEY_LENGTH,        NULL,                pgp_entry_pgp_l_num },
  { ED_PGP_KEY, ED_PGK_PKEY_ALGORITHM,    pgp_entry_pgp_A,     NULL },
  { ED_PGP_KEY, ED_PGK_PKEY_CAPABILITIES, pgp_entry_pgp_C,     NULL },
  { ED_PGP_KEY, ED_PGK_PKEY_FINGERPRINT,  pgp_entry_ignore,    NULL },
  { ED_PGP_KEY, ED_PGK_PKEY_FLAGS,        pgp_entry_pgp_F,     NULL },
  { ED_PGP_KEY, ED_PGK_PKEY_ID,           pgp_entry_pgp_K,     NULL },
  { ED_PGP_KEY, ED_PGK_PKEY_LENGTH,       NULL,                pgp_entry_pgp_L_num },
  { ED_PGP_KEY, ED_PGK_PROTOCOL,          pgp_entry_ignore,    NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
