/**
 * @file
 * Render Expandos using Data
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_EXPANDO_RENDER_H
#define MUTT_EXPANDO_RENDER_H

#include <stdint.h>

struct Buffer;
struct ExpandoNode;

/**
 * enum MuttFormatFlag - Flags for expando_render(), e.g. #MUTT_FORMAT_FORCESUBJ
 */
enum MuttFormatFlag
{
  MUTT_FORMAT_NONE        =       0,  ///< No flags are set
  MUTT_FORMAT_FORCESUBJ   = 1U << 0,  ///< Print the subject even if unchanged
  MUTT_FORMAT_TREE        = 1U << 1,  ///< Draw the thread tree
  MUTT_FORMAT_STAT_FILE   = 1U << 2,  ///< Used by attach_format_str
  MUTT_FORMAT_ARROWCURSOR = 1U << 3,  ///< Reserve space for arrow_cursor
  MUTT_FORMAT_INDEX       = 1U << 4,  ///< This is a main index entry
  MUTT_FORMAT_PLAIN       = 1U << 5,  ///< Do not prepend DISP_TO, DISP_CC ...
};
typedef uint8_t MuttFormatFlags;

/**
 * @defgroup expando_get_string_api Expando Get String API
 * @ingroup expando_get_data_api
 *
 * get_string_t - Get some string data
 * @param[in]  node      ExpandoNode containing the callback
 * @param[in]  data      Private data
 * @param[in]  flags     Flags, see #MuttFormatFlags
 * @param[out] buf       Buffer in which to save string
 *
 * Get some string data to be formatted.
 */
typedef void (*get_string_t)(const struct ExpandoNode *node, void *data, MuttFormatFlags flags, struct Buffer *buf);

/**
 * @defgroup expando_get_number_api Expando Get Number API
 * @ingroup expando_get_data_api
 *
 * get_number_t - Get some numeric data
 * @param[in]  node      ExpandoNode containing the callback
 * @param[in]  data      Private data
 * @param[in]  flags     Flags, see #MuttFormatFlags
 * @retval num Data as a number
 *
 * Get some numeric data to be formatted.
 */
typedef long (*get_number_t)(const struct ExpandoNode *node, void *data, MuttFormatFlags flags);

/**
 * @defgroup expando_get_data_api Expando Get Data API
 *
 * Define callback functions to get data to be formatted.
 * Each function is associated with a Domain+UID pair.
 */
struct ExpandoRenderCallback
{
  int did;                  ///< Domain ID, #ExpandoDomain
  int uid;                  ///< Unique ID, e.g. #ExpandoDataAlias

  get_string_t get_string;  ///< Callback function to get a string
  get_number_t get_number;  ///< Callback function to get a number
};

int node_render(const struct ExpandoNode *node, const struct ExpandoRenderCallback *erc,
                struct Buffer *buf, int max_cols, void *data, MuttFormatFlags flags);

#endif /* MUTT_EXPANDO_RENDER_H */
