/**
 * @file
 * Help system
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Floyd Anderson <f.a@31c0.net>
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

#include "mx.h"

extern struct MxOps MxHelpOps;

/**
 * enum dirent_type - Constants for d_type field values of the dirent structure
 *                    and used for bitwise filter mask matching, even the macro
 *                    _DIRENT_HAVE_D_TYPE for the d_type field is not defined
 */
typedef enum dirent_type
{
  DET_UNKNOWN = (1 << 0), /* flag for DT_UNKNOWN field value (0) */
  DET_FIFO    = (1 << 1), /* flag for DT_FIFO field value (1) */
  DET_CHR     = (1 << 2), /* flag for DT_CHR field value (2) */
  DET_DIR     = (1 << 3), /* flag for DT_DIR field value (4) */
  DET_BLK     = (1 << 4), /* flag for DT_BLK field value (6) */
  DET_REG     = (1 << 5), /* flag for DT_REG field value (8) */
  DET_LNK     = (1 << 6), /* flag for DT_LNK field value (10) */
  DET_SOCK    = (1 << 7), /* flag for DT_SOCK field value (12) */
  DET_WHT     = (1 << 8)  /* flag for DT_WHT (dummy, whiteout inode) field value (14) */
} DEType;
#define DT2DET(type) (((type) ? 2 : 1) << ((type) >> 1))
typedef unsigned int DETMask;

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
