/**
 * @file
 * Shared debug code
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page debug_common Shared debug code
 *
 * Shared debug code
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "lib.h"

void add_flag(struct Buffer *buf, bool is_set, const char *name)
{
  if (!buf || !name)
    return;

  if (is_set)
  {
    if (!mutt_buffer_is_empty(buf))
      mutt_buffer_addch(buf, ',');
    mutt_buffer_addstr(buf, name);
  }
}

const char *get_content_encoding(enum ContentEncoding enc)
{
  switch (enc)
  {
    case ENC_OTHER:
      return "ENC_OTHER";
    case ENC_7BIT:
      return "ENC_7BIT";
    case ENC_8BIT:
      return "ENC_8BIT";
    case ENC_QUOTED_PRINTABLE:
      return "ENC_QUOTED_PRINTABLE";
    case ENC_BASE64:
      return "ENC_BASE64";
    case ENC_BINARY:
      return "ENC_BINARY";
    case ENC_UUENCODED:
      return "ENC_UUENCODED";
    default:
      return "UNKNOWN";
  }
}

const char *get_content_disposition(enum ContentDisposition disp)
{
  switch (disp)
  {
    case DISP_INLINE:
      return "DISP_INLINE";
    case DISP_ATTACH:
      return "DISP_ATTACH";
    case DISP_FORM_DATA:
      return "DISP_FORM_DATA";
    case DISP_NONE:
      return "DISP_NONE";
    default:
      return "UNKNOWN";
  }
}

const char *get_content_type(enum ContentType type)
{
  switch (type)
  {
    case TYPE_OTHER:
      return "TYPE_OTHER";
    case TYPE_AUDIO:
      return "TYPE_AUDIO";
    case TYPE_APPLICATION:
      return "TYPE_APPLICATION";
    case TYPE_IMAGE:
      return "TYPE_IMAGE";
    case TYPE_MESSAGE:
      return "TYPE_MESSAGE";
    case TYPE_MODEL:
      return "TYPE_MODEL";
    case TYPE_MULTIPART:
      return "TYPE_MULTIPART";
    case TYPE_TEXT:
      return "TYPE_TEXT";
    case TYPE_VIDEO:
      return "TYPE_VIDEO";
    case TYPE_ANY:
      return "TYPE_ANY";
    default:
      return "UNKNOWN";
  }
}
