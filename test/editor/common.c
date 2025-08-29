/**
 * @file
 * Shared Editor Buffer Testing Code
 *
 * @authors
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "common.h"
#include "editor/lib.h"

/**
 * editor_buffer_get_lastchar - Get the position of the last character
 * @param es State of the Enter buffer
 * @retval num Position of last character
 */
size_t editor_buffer_get_lastchar(struct EnterState *es)
{
  if (!es)
    return 0;

  return es->lastchar;
}

/**
 * editor_buffer_get_cursor - Get the position of the cursor
 * @param es State of the Enter buffer
 * @retval num Position of cursor
 */
size_t editor_buffer_get_cursor(struct EnterState *es)
{
  if (!es)
    return 0;

  return es->curpos;
}

/**
 * editor_buffer_set_cursor - Set the position of the cursor
 * @param es  State of the Enter buffer
 * @param pos New position for the cursor
 * @retval num Position of cursor
 *
 * @note If the cursor is positioned beyond the end of the buffer,
 *       it will be set to the last character
 */
size_t editor_buffer_set_cursor(struct EnterState *es, size_t pos)
{
  if (!es)
    return 0;

  if (pos >= es->lastchar)
    pos = es->lastchar;

  es->curpos = pos;
  return pos;
}

/**
 * editor_buffer_set - Set the string in the buffer
 * @param es  State of the Enter buffer
 * @param str String to set
 * @retval num Length of string
 */
int editor_buffer_set(struct EnterState *es, const char *str)

{
  if (!es)
    return 0;

  es->wbuflen = 0;
  es->lastchar = mutt_mb_mbstowcs(&es->wbuf, &es->wbuflen, 0, str);
  es->curpos = es->lastchar;
  return es->lastchar;
}
