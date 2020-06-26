/**
 * @file
 * Misc PGP helper routines
 *
 * @authors
 * Copyright (C) 1997-2002 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page crypt_pgplib Misc PGP helper routines
 *
 * Misc PGP helper routines
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgplib.h"
#endif

/**
 * pgp_pkalgbytype - Get the name of the algorithm from its ID
 * @param type Algorithm ID
 * @retval ptr Algorithm name
 */
const char *pgp_pkalgbytype(unsigned char type)
{
  switch (type)
  {
    case 1:
      return "RSA";
    case 2:
      return "RSA";
    case 3:
      return "RSA";
    case 16:
      return "ElG";
    case 17:
      return "DSA";
    case 20:
      return "ElG";
    default:
      return "unk";
  }
}

/**
 * pgp_canencrypt - Does this algorithm ID support encryption?
 * @param type Algorithm ID
 * @retval true If it does
 */
bool pgp_canencrypt(unsigned char type)
{
  switch (type)
  {
    case 1:
    case 2:
    case 16:
    case 20:
      return true;
    default:
      return false;
  }
}

/**
 * pgp_cansign - Does this algorithm ID support signing?
 * @param type Algorithm ID
 * @retval true If it does
 */
bool pgp_cansign(unsigned char type)
{
  switch (type)
  {
    case 1:
    case 3:
    case 17:
    case 20:
      return true;
    default:
      return false;
  }
}

/**
 * pgp_get_abilities - Get the capabilities of an algorithm
 * @param type Algorithm ID
 * @retval num Capabilities
 *
 * The abilities are OR'd together
 * - 1 If signing is possible
 * - 2 If encryption is possible
 */
short pgp_get_abilities(unsigned char type)
{
  return (pgp_canencrypt(type) << 1) | pgp_cansign(type);
}

/**
 * pgp_uid_free - Free a PGP UID
 * @param[out] upp PGP UID to free
 */
static void pgp_uid_free(struct PgpUid **upp)
{
  struct PgpUid *up = NULL, *q = NULL;

  if (!upp || !*upp)
    return;
  for (up = *upp; up; up = q)
  {
    q = up->next;
    FREE(&up->addr);
    FREE(&up);
  }

  *upp = NULL;
}

/**
 * pgp_copy_uids - Copy a list of PGP UIDs
 * @param up     List of PGP UIDs
 * @param parent Parent PGP key
 * @retval ptr New list of PGP UIDs
 */
struct PgpUid *pgp_copy_uids(struct PgpUid *up, struct PgpKeyInfo *parent)
{
  struct PgpUid *l = NULL;
  struct PgpUid **lp = &l;

  for (; up; up = up->next)
  {
    *lp = mutt_mem_calloc(1, sizeof(struct PgpUid));
    (*lp)->trust = up->trust;
    (*lp)->flags = up->flags;
    (*lp)->addr = mutt_str_dup(up->addr);
    (*lp)->parent = parent;
    lp = &(*lp)->next;
  }

  return l;
}

/**
 * key_free - Free a PGP Key info
 * @param[out] kpp PGP Key info to free
 */
static void key_free(struct PgpKeyInfo **kpp)
{
  if (!kpp || !*kpp)
    return;

  struct PgpKeyInfo *kp = *kpp;

  pgp_uid_free(&kp->address);
  FREE(&kp->keyid);
  FREE(&kp->fingerprint);
  FREE(kpp);
}

/**
 * pgp_remove_key - Remove a PGP key from a list
 * @param[out] klist List of PGP Keys
 * @param[in]  key   Key to remove
 * @retval ptr Updated list of PGP Keys
 */
struct PgpKeyInfo *pgp_remove_key(struct PgpKeyInfo **klist, struct PgpKeyInfo *key)
{
  if (!klist || !*klist || !key)
    return NULL;

  struct PgpKeyInfo **last = NULL;
  struct PgpKeyInfo *p = NULL, *q = NULL, *r = NULL;

  if (key->parent && (key->parent != key))
    key = key->parent;

  last = klist;
  for (p = *klist; p && p != key; p = p->next)
    last = &p->next;

  if (!p)
    return NULL;

  for (q = p->next, r = p; q && q->parent == p; q = q->next)
    r = q;

  if (r)
    r->next = NULL;

  *last = q;
  return q;
}

/**
 * pgp_key_free - Free a PGP key info
 * @param[out] kpp PGP key info to free
 */
void pgp_key_free(struct PgpKeyInfo **kpp)
{
  if (!kpp || !*kpp)
    return;

  struct PgpKeyInfo *p = NULL, *q = NULL, *r = NULL;

  if ((*kpp)->parent && ((*kpp)->parent != *kpp))
    *kpp = (*kpp)->parent;

  /* Order is important here:
   *
   * - First free all children.
   * - If we are an orphan (i.e., our parent was not in the key list),
   *   free our parent.
   * - free ourselves.  */

  for (p = *kpp; p; p = q)
  {
    for (q = p->next; q && q->parent == p; q = r)
    {
      r = q->next;
      key_free(&q);
    }

    key_free(&p->parent);
    key_free(&p);
  }

  *kpp = NULL;
}

/**
 * pgp_keyinfo_new - Create a new PgpKeyInfo
 * @retval ptr New PgpKeyInfo
 */
struct PgpKeyInfo *pgp_keyinfo_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct PgpKeyInfo));
}
