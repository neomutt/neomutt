/**
 * @file
 * Help system
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Floyd Anderson <f.a@31c0.net>
 * Copyright (C) 2019 Tran Manh Tu <xxlaguna93@gmail.com>
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

#ifndef MUTT_HELP_HELP_H
#define MUTT_HELP_HELP_H

#include <stddef.h>
#include <stdint.h>
#include "mx.h"

extern struct MxOps MxHelpOps;

typedef uint8_t HelpDocFlags;     ///< Types of Help Documents, e.g. #HELP_DOC_INDEX
#define HELP_DOC_NO_FLAGS      0  ///< No flags are set
#define HELP_DOC_UNKNOWN (1 << 0) ///< File isn't a help document
#define HELP_DOC_INDEX   (1 << 1) ///< Document is treated as help index (index.md)
#define HELP_DOC_ROOTDOC (1 << 2) ///< Document lives directly in root of #C_HelpDocDir
#define HELP_DOC_CHAPTER (1 << 3) ///< Document is treated as help chapter
#define HELP_DOC_SECTION (1 << 4) ///< Document is treated as help section

/**
 * struct HelpList - Generic list to hold several help elements
 */
struct HelpList
{
  size_t item_size; ///< Size of a single element
  size_t size;      ///< List length
  size_t capa;      ///< List capacity
  void **data;      ///< Internal list data pointers
};
#define HELPLIST_INIT_CAPACITY 10

/**
 * struct helpfile_header - Describes the header of a help file
 */
struct HelpFileHeader
{
  char *key;
  char *val;
};

/**
 * struct helpdoc_meta - Bundle additional information to a help document
 */
struct HelpDocMeta
{
  struct HelpList *fhdr; ///< File header lines (list of key/value pairs)
  char *name;            ///< Base file name
  HelpDocFlags type;     ///< Type of the help document
};

void help_doclist_free(void);
int help_doclist_init(void);

#endif /* MUTT_HELP_HELP_H */
