/**
 * @file
 * Colour Domains
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page color_domain Colour Domains
 *
 * Colour Domains
 */

#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "domain.h"
#include "color.h"
#include "regex4.h"

struct ColorDomainArray ColorDomains;
struct UserColorArray UserColors;

/**
 * user_color_sort - Compare two Colours by their Domain and Colour Ids - Implements ::sort_t - @ingroup sort_api
 */
static int user_color_sort(const void *a, const void *b, void *sdata)
{
  const struct UserColor *uc_a = a;
  const struct UserColor *uc_b = b;

  if (uc_a->did == uc_b->did)
    return mutt_numeric_cmp(uc_a->cdef->cid, uc_b->cdef->cid);

  return mutt_numeric_cmp(uc_a->did, uc_b->did);
}

/**
 * color_register_domain - Register a Colour Domain
 * @param name Name of the domain
 * @param did  Domain ID
 * @retval true Success
 */
bool color_register_domain(const char *name, enum ColorDomainId did)
{
  struct ColorDomain cd = { name, did };

  ARRAY_ADD(&ColorDomains, cd);
  return true;
}

/**
 * color_clear_domains - Clean up the Colour Domains
 */
void color_clear_domains(void)
{
  ARRAY_FREE(&ColorDomains);
}

/**
 * color_register_colors - Register a set of Colours in a Domain
 * @param did  Colour Domain
 * @param cdef Colour Definition
 * @retval true Success
 */
bool color_register_colors(enum ColorDomainId did, const struct ColorDefinition *cdef)
{
  for (; cdef->name; cdef++)
  {
    struct UserColor uc = { 0 };
    uc.did = did;
    uc.cdef = cdef;

    switch (cdef->type)
    {
      case CDT_SIMPLE:
        uc.type = CDT_SIMPLE;
        uc.cdata = attr_color_new();
        break;

      case CDT_PATTERN:
        uc.type = CDT_PATTERN;
        uc.cdata = pattern_color_list_new();
        break;

      case CDT_REGEX:
        uc.type = CDT_REGEX;
        uc.cdata = regex_color_list_new();
        break;
    }

    ARRAY_ADD(&UserColors, uc);
  }

  ARRAY_SORT(&UserColors, user_color_sort, NULL);

  return true;
}

/**
 * color_clear_colors - Clean up the registered Colours
 */
void color_clear_colors(void)
{
  struct UserColor *uc = NULL;

  ARRAY_FOREACH(uc, &UserColors)
  {
    switch (uc->type)
    {
      case CDT_SIMPLE:
        attr_color_free((struct AttrColor **) &uc->cdata);
        break;

      case CDT_PATTERN:
        pattern_color_free(NULL, (struct PatternColor **) &uc->cdata);
        break;

      case CDT_REGEX:
        regex_color_free((struct RegexColor **) &uc->cdata);
        break;
    }
  }

  ARRAY_FREE(&UserColors);
}

/**
 * domain_get_name - Get the name of a Colour Domain
 * @param did Domain ID
 * @retval ptr Name of Domain
 */
const char *domain_get_name(enum ColorDomainId did)
{
  struct ColorDomain *cd = NULL;

  ARRAY_FOREACH(cd, &ColorDomains)
  {
    if (cd->did == did)
      return cd->name;
  }

  return "UNKNOWN";
}

/**
 * domain_get_type - Convert the Colour Type into a string
 * @param type Colour Type
 * @retval ptr Colour Type
 */
const char *domain_get_type(enum ColorDefType type)
{
  switch (type)
  {
    case CDT_SIMPLE:
      return "simple";

    case CDT_PATTERN:
      return "pattern";

    case CDT_REGEX:
      return "regex";

    default:
      return "UNKNOWN";
  }
}

/**
 * domain_get_flags - Convert Colour Flags into a string
 * @param flags Colour flags
 * @param buf   Buffer for the result
 */
void domain_get_flags(ColorDefFlags flags, char *buf)
{
  int num = 0;

  if (flags & CDF_BACK_REF)
  {
    if (num > 0)
      num += sprintf(buf + num, ",");

    num += sprintf(buf + num, "back-ref");
  }

  if (flags & CDF_SYNONYM)
  {
    if (num > 0)
      num += sprintf(buf + num, ",");

    num += sprintf(buf + num, "synonym");
  }
}

/**
 * color_find_by_name - Lookup a Colour by its name
 * @param name Colour name
 * @retval ptr UserColor
 */
struct UserColor *color_find_by_name(const char *name)
{
  struct UserColor *uc = NULL;
  ARRAY_FOREACH(uc, &UserColors)
  {
    if (mutt_istr_equal(uc->cdef->name, name))
      return uc;
  }

  return NULL;
}

/**
 * color_find_by_id - Lookup a Colour by Domain and ID
 * @param did Colour Domain
 * @param cid Colour ID
 */
struct UserColor *color_find_by_id(enum ColorDomainId did, int cid)
{
  struct UserColor *uc = NULL;
  ARRAY_FOREACH(uc, &UserColors)
  {
    if ((uc->did == did) && (uc->cdef->cid == cid))
      return uc;
  }

  return NULL;
}

/**
 * color_get_simple - Lookup a Simple Colour by Domain and ID
 * @param did Colour Domain
 * @param cid Colour ID
 * @retval ptr UserColor
 */
struct AttrColor *color_get_simple(enum ColorDomainId did, int cid)
{
  struct UserColor *uc = color_find_by_id(did, cid);
  if (!uc)
    return NULL;

  if (uc->cdef->type != CDT_SIMPLE)
    return NULL;

  return uc->cdata;
}

/**
 * color_get_pattern - Lookup a Pattern Colour by Domain and ID
 * @param did Colour Domain
 * @param cid Colour ID
 * @retval ptr UserColor
 */
struct PatternColorList *color_get_pattern(enum ColorDomainId did, int cid)
{
  struct UserColor *uc = color_find_by_id(did, cid);
  if (!uc)
    return NULL;

  if (uc->cdef->type != CDT_PATTERN)
    return NULL;

  return uc->cdata;
}

/**
 * color_get_regex - Lookup a Regex Colour by Domain and ID
 * @param did Colour Domain
 * @param cid Colour ID
 * @retval ptr UserColor
 */
struct RegexColorList *color_get_regex(enum ColorDomainId did, int cid)
{
  struct UserColor *uc = color_find_by_id(did, cid);
  if (!uc)
    return NULL;

  if (uc->cdef->type != CDT_REGEX)
    return NULL;

  return uc->cdata;
}
