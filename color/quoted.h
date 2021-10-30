/**
 * @file
 * Quoted-Email colours
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_QUOTED_H
#define MUTT_COLOR_QUOTED_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "core/lib.h"
#include "color.h"

struct Buffer;

/// Ten colours, quoted0..quoted9 (quoted and quoted0 are equivalent)
#define COLOR_QUOTES_MAX 10

extern struct AttrColor QuotedColors[];
extern int NumQuotedColors;

/**
 * struct QuoteStyle - Style of quoted text
 *
 * NeoMutt will store a tree of all the different email quoting levels it
 * detects in an Email.  If `$quote_regex` matches, say both "> " and "| ",
 * and the Email has three levels of indent, then the tree will contain two
 * siblings each with a child and grandchild.
 *
 * @dot
 * digraph QuoteStyle
 * {
 *   node [ shape="box" style="filled" fillcolor="#e0ffff" ]
 *   angle1 [ label=<"&gt;"> ]
 *   angle2 [ label=<"&gt; &gt;"> ]
 *   angle3 [ label=<"&gt; &gt; &gt;"> ]
 *   bar1 [ label=<"|"> ]
 *   bar2 [ label=<"| |"> ]
 *   bar3 [ label=<"| | |"> ]
 *   angle1 -> angle2 -> angle3
 *   bar1 -> bar2 -> bar3
 *   angle1 -> bar1
 *   { rank=same angle1 bar1 }
 * }
 * @enddot
 */
struct QuoteStyle
{
  int quote_n;                      ///< The quoteN colour index for this level
  struct AttrColor *attr_color;     ///< Colour and attribute of the text
  char *prefix;                     ///< Prefix string, e.g. "> "
  size_t prefix_len;                ///< Length of the prefix string
  struct QuoteStyle *prev, *next;   ///< Different quoting styles at the same level
  struct QuoteStyle *up, *down;     ///< Parent (less quoted) and child (more quoted) levels
};

void               quoted_colors_clear(void);
struct AttrColor * quoted_colors_get(int q);
int                quoted_colors_num_used(void);

bool               quoted_colors_parse_color  (enum ColorId cid, uint32_t fg, uint32_t bg, int attrs, int q_level, int *rc, struct Buffer *err);
enum CommandResult quoted_colors_parse_uncolor(enum ColorId cid, int q_level, struct Buffer *err);

struct QuoteStyle *qstyle_classify (struct QuoteStyle **quote_list, const char *qptr, size_t length, bool *force_redraw, int *q_level);
void               qstyle_free_tree(struct QuoteStyle **quote_list);
void               qstyle_recolour (struct QuoteStyle *quote_list);

#endif /* MUTT_COLOR_QUOTED_H */
