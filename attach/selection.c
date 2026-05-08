/**
 * @file
 * Build a selection of Attachments for an action
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page attach_selection Build a selection of Attachments for an action
 *
 * Build a selection of Attachments for an action
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "menu/lib.h"
#include "attach.h"
#include "recvattach.h"

/**
 * aa_contains_attach - Does a working set already include an Attachment?
 * @param aa Selected attachments
 * @param ap Candidate attachment
 * @retval true Attachment is selected
 */
static bool aa_contains_attach(struct AttachPtrArray *aa, struct AttachPtr *ap)
{
  if (!aa || !ap)
    return false;

  struct AttachPtr **app = NULL;
  ARRAY_FOREACH(app, aa)
  {
    if (*app == ap)
      return true;
  }

  return false;
}

/**
 * aa_add_attach - Add an Attachment to a working set
 * @param aa Selected attachments
 * @param ap Attachment to add
 */
static void aa_add_attach(struct AttachPtrArray *aa, struct AttachPtr *ap)
{
  if (!aa_contains_attach(aa, ap))
    ARRAY_ADD(aa, ap);
}

/**
 * aa_add_folded - Add an Attachment, expanding collapsed children
 * @param aa     Selected attachments
 * @param actx   List of Attachments
 * @param rindex Real attachment index
 */
static void aa_add_folded(struct AttachPtrArray *aa, struct AttachCtx *actx, int rindex)
{
  if (!aa || !actx || (rindex < 0) || (rindex >= actx->idxlen))
    return;

  struct AttachPtr *ap = actx->idx[rindex];
  aa_add_attach(aa, ap);

  if (!ap->collapsed)
    return;

  const int level = ap->level;
  for (int i = rindex + 1; (i < actx->idxlen) && (actx->idx[i]->level > level); i++)
  {
    aa_add_attach(aa, actx->idx[i]);
  }
}

/**
 * aa_add_tagged - Get an array of tagged Attachments
 * @param aa   Empty AttachPtrArray to populate
 * @param actx List of Attachments
 * @retval num Number of selected Attachments
 * @retval -1  Error
 */
static int aa_add_tagged(struct AttachPtrArray *aa, struct AttachCtx *actx)
{
  if (!aa || !actx)
    return -1;

  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->body->tagged)
      aa_add_folded(aa, actx, i);
  }

  return ARRAY_SIZE(aa);
}

/**
 * aa_add_selection - Build a working set of Attachments for an action
 * @param aa         Empty AttachPtrArray to populate
 * @param actx       List of Attachments
 * @param menu       Menu
 * @param use_tagged Use tagged attachments
 * @param count      Repeat-count (0 or 1 == current selection)
 * @retval num Number of selected Attachments
 * @retval -1  Error
 */
int aa_add_selection(struct AttachPtrArray *aa, struct AttachCtx *actx,
                     struct Menu *menu, bool use_tagged, int count)
{
  if (!aa || !actx || !menu)
    return -1;

  if (use_tagged)
    return aa_add_tagged(aa, actx);

  const int index = menu_get_index(menu);
  if ((index < 0) || (index >= actx->vcount))
    return -1;

  int n = (count > 1) ? count : 1;
  if ((index + n) > actx->vcount)
    n = actx->vcount - index;

  for (int i = 0; i < n; i++)
  {
    const int rindex = actx->v2r[index + i];
    if ((rindex < 0) || (rindex >= actx->idxlen))
      return -1;

    aa_add_folded(aa, actx, rindex);
  }

  return ARRAY_SIZE(aa);
}

/**
 * ba_add_selection - Build a working set of attachment bodies for an action
 * @param ba         Empty BodyArray to populate
 * @param actx       List of Attachments
 * @param menu       Menu
 * @param use_tagged Use tagged attachments
 * @param count      Repeat-count (0 or 1 == current selection)
 * @retval num Number of selected Attachments
 * @retval -1  Error
 */
int ba_add_selection(struct BodyArray *ba, struct AttachCtx *actx,
                     struct Menu *menu, bool use_tagged, int count)
{
  if (!ba)
    return -1;

  struct AttachPtrArray aa = ARRAY_HEAD_INITIALIZER;
  if (aa_add_selection(&aa, actx, menu, use_tagged, count) < 0)
  {
    ARRAY_FREE(&aa);
    return -1;
  }

  struct AttachPtr **app = NULL;
  ARRAY_FOREACH(app, &aa)
  {
    ARRAY_ADD(ba, (*app)->body);
  }

  const int rc = ARRAY_SIZE(ba);
  ARRAY_FREE(&aa);
  return rc;
}
