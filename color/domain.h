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

#ifndef MUTT_COLOR_DOMAIN_H
#define MUTT_COLOR_DOMAIN_H

#include <stdbool.h>
#include "color.h"

/**
 * enum ColorDomain - Colour Domains
 *
 * Each domain has CID associated with it.
 * Their prefixes are shown.
 */
enum ColorDomainId
{
  CD_CORE = 1,     ///< Core       CD_COR_ #ColorCore
  CD_COMPOSE,      ///< Compose    CD_COM_ #ColorCompose
  CD_INDEX,        ///< Index      CD_IND_ #ColorIndex
  CD_PAGER,        ///< Pager      CD_PAG_ #ColorPager
  CD_QUOTED,       ///< Quoted     CD_QUO_ #ColorQuoted
  CD_SIDEBAR,      ///< Sidebar    CD_SID_ #ColorSidebar
};

/**
 * enum ColorCore - Core NeoMutt Colours
 *
 * @sa CD_CORE, ColorDomain
 */
enum ColorCore
{
  CD_COR_BOLD = 1,                 ///< Bold text
  CD_COR_ERROR,                    ///< Error message
  CD_COR_INDICATOR,                ///< Selected item in list
  CD_COR_ITALIC,                   ///< Italic text
  CD_COR_MESSAGE,                  ///< Informational message
  CD_COR_NORMAL,                   ///< Plain text
  CD_COR_OPTIONS,                  ///< Options in prompt
  CD_COR_PROGRESS,                 ///< Progress bar
  CD_COR_PROMPT,                   ///< Question/user input
  CD_COR_STATUS,                   ///< Status bar (takes a pattern)
  CD_COR_STRIPE_EVEN,              ///< Stripes: even lines of the Help Page
  CD_COR_STRIPE_ODD,               ///< Stripes: odd lines of the Help Page
  CD_COR_TREE,                     ///< Tree-drawing characters (Index, Attach)
  CD_COR_UNDERLINE,                ///< Underlined text
  CD_COR_WARNING,                  ///< Warning messages
};

/**
 * struct ColorDomain - The owner of a set of colours
 */
struct ColorDomain
{
  const char        *name;         ///< Name of set of Colours
  enum ColorDomainId did;          ///< Colour Domain ID
};
ARRAY_HEAD(ColorDomainArray, struct ColorDomain);

/**
 * struct UserColor - User settable colour
 */
struct UserColor
{
  enum ColorDomainId did;                ///< Colour Domain ID
  const struct ColorDefinition *cdef;    ///< Definition of Colour
  int type;                              ///< Colour type e.g. #CDT_REGEX
  void *cdata;                           ///< Colour data, (AttrColor, PatternColorList or RegexColorList)
};
ARRAY_HEAD(UserColorArray, struct UserColor);

extern struct UserColorArray UserColors;
extern struct ColorDomainArray ColorDomains;

bool color_register_domain(const char *name, enum ColorDomainId did);
bool color_register_colors(enum ColorDomainId did, const struct ColorDefinition *cdef);

void color_clear_domains(void);
void color_clear_colors(void);

void dump_colors(void);

const char *domain_get_name (enum ColorDomainId did);
void        domain_get_flags(ColorDefFlags flags, char *buf);
const char *domain_get_type (enum ColorDefType type);

struct UserColor *color_find_by_name(const char *name);

#endif /* MUTT_COLOR_DOMAIN_H */
