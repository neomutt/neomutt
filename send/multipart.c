/**
 * @file
 * Manipulate multipart Emails
 *
 * @authors
 * Copyright (C) 1996-2002,2009-2012 Michael R. Elkins <me@mutt.org>
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
 * @page send_multipart Manipulate multipart Emails
 *
 * Manipulate multipart Emails
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "sendlib.h"

/**
 * get_toplevel_encoding - Find the most restrictive encoding type
 * @param a Body to examine
 * @retval num Encoding type, e.g. #ENC_7BIT
 */
static int get_toplevel_encoding(struct Body *a)
{
  int e = ENC_7BIT;

  for (; a; a = a->next)
  {
    if (a->encoding == ENC_BINARY)
      return ENC_BINARY;
    if (a->encoding == ENC_8BIT)
      e = ENC_8BIT;
  }

  return e;
}

/**
 * check_boundary - check for duplicate boundary
 * @param boundary Boundary to look for
 * @param b        Body parts to check
 * @retval true if duplicate found
 */
static bool check_boundary(const char *boundary, struct Body *b)
{
  char *p = NULL;

  if (b->parts && check_boundary(boundary, b->parts))
    return true;

  if (b->next && check_boundary(boundary, b->next))
    return true;

  p = mutt_param_get(&b->parameter, "boundary");
  if (p && mutt_str_equal(p, boundary))
  {
    return true;
  }
  return false;
}

/**
 * mutt_generate_boundary - Create a unique boundary id for a MIME part
 * @param pl MIME part
 */
void mutt_generate_boundary(struct ParameterList *pl)
{
  char rs[MUTT_RANDTAG_LEN + 1];

  mutt_rand_base32(rs, sizeof(rs) - 1);
  rs[MUTT_RANDTAG_LEN] = 0;
  mutt_param_set(pl, "boundary", rs);
}

/**
 * mutt_make_multipart - Create a multipart email
 * @param b Body of email to start
 * @retval ptr Newly allocated Body
 */
struct Body *mutt_make_multipart(struct Body *b)
{
  struct Body *new_body = mutt_body_new();
  new_body->type = TYPE_MULTIPART;
  new_body->subtype = mutt_str_dup("mixed");
  new_body->encoding = get_toplevel_encoding(b);
  do
  {
    mutt_generate_boundary(&new_body->parameter);
    if (check_boundary(mutt_param_get(&new_body->parameter, "boundary"), b))
      mutt_param_delete(&new_body->parameter, "boundary");
  } while (!mutt_param_get(&new_body->parameter, "boundary"));
  new_body->use_disp = false;
  new_body->disposition = DISP_INLINE;
  new_body->parts = b;

  return new_body;
}

/**
 * mutt_remove_multipart - Extract the multipart body if it exists
 * @param b Body to alter
 * @retval ptr The parts of the Body
 *
 * @note The original Body is freed
 */
struct Body *mutt_remove_multipart(struct Body *b)
{
  struct Body *t = NULL;

  if (b->parts)
  {
    t = b;
    b = b->parts;
    t->parts = NULL;
    mutt_body_free(&t);
  }
  return b;
}
