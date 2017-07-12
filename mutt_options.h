/**
 * @file
 * NotMuch virtual mailbox type
 *
 * @authors
 * Copyright (C) 2016 Bernard Pratz <z+mutt+pub@m0g.net>
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

#ifndef _MUTT_OPTIONS_H
#define _MUTT_OPTIONS_H

#include <stddef.h>

struct Buffer;

#define DT_MASK      0x0f
#define DT_BOOL      1    /**< boolean option */
#define DT_NUM       2    /**< a number */
#define DT_STR       3    /**< a string */
#define DT_PATH      4    /**< a pathname */
#define DT_QUAD      5    /**< quad-option (yes/no/ask-yes/ask-no) */
#define DT_SORT      6    /**< sorting methods */
#define DT_RX        7    /**< regular expressions */
#define DT_MAGIC     8    /**< mailbox type */
#define DT_SYN       9    /**< synonym for another variable */
#define DT_ADDR      10   /**< e-mail address */
#define DT_MBCHARTBL 11   /**< multibyte char table */
#define DT_HCACHE    12   /**< header cache backend */

#define DTYPE(x) ((x) &DT_MASK)

/* subtypes */
#define DT_SUBTYPE_MASK 0xff0
#define DT_SORT_ALIAS   0x10
#define DT_SORT_BROWSER 0x20
#define DT_SORT_KEYS    0x40
#define DT_SORT_AUX     0x80
#define DT_SORT_SIDEBAR 0x100

/**
 * struct Option - Definition of a user-variable
 */
struct Option
{
  const char *option; /**< user-visible name */
  short type;         /**< varible type, e.g. *DT_STR */
  short flags;        /**< notification flags, e.g. R_PAGER */
  unsigned long data; /**< pointer to the global variable */
  unsigned long init; /**< initial value */
};

int mutt_option_to_string(const struct Option *opt, char *val, size_t len);

bool mutt_option_get(const char *s, struct Option *opt);
int mutt_option_set(const struct Option *val, struct Buffer *err);

#endif /* _MUTT_OPTIONS_H */
