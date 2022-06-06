/**
 * @file
 * Enter buffer
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page enter_enter Enter buffer
 *
 * Enter buffer
 */

#include "config.h"
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "enter.h"
#include "state.h"

/// combining mark / non-spacing character
#define COMB_CHAR(wc) (IsWPrint(wc) && (wcwidth(wc) == 0))

/**
 * editor_backspace - Delete the char in front of the cursor
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Character deleted
 * @retval FR_ERROR   Failed, cursor was at the start of the buffer
 */
int editor_backspace(struct EnterState *es)
{
  if (!es || (es->curpos == 0))
    return FR_ERROR;

  size_t i = es->curpos;
  while ((i > 0) && COMB_CHAR(es->wbuf[i - 1]))
    i--;
  if (i > 0)
    i--;
  memmove(es->wbuf + i, es->wbuf + es->curpos,
          (es->lastchar - es->curpos) * sizeof(wchar_t));
  es->lastchar -= es->curpos - i;
  es->curpos = i;

  return FR_SUCCESS;
}

/**
 * editor_backward_char - Move the cursor one character to the left
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Cursor moved
 * @retval FR_ERROR   Failed, cursor was at the start of the buffer
 */
int editor_backward_char(struct EnterState *es)
{
  if (!es || (es->curpos == 0))
    return FR_ERROR;

  while (es->curpos && COMB_CHAR(es->wbuf[es->curpos - 1]))
    es->curpos--;
  if (es->curpos)
    es->curpos--;

  return FR_SUCCESS;
}

/**
 * editor_backward_word - Move the cursor to the beginning of the word
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Cursor moved
 * @retval FR_ERROR   Failed, cursor was at the start of the buffer
 */
int editor_backward_word(struct EnterState *es)
{
  if (!es || (es->curpos == 0))
    return FR_ERROR;

  while (es->curpos && iswspace(es->wbuf[es->curpos - 1]))
    es->curpos--;
  while (es->curpos && !iswspace(es->wbuf[es->curpos - 1]))
    es->curpos--;

  return FR_SUCCESS;
}

/**
 * editor_bol - Jump to the beginning of the line
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Cursor moved
 * @retval FR_ERROR   Error
 */
int editor_bol(struct EnterState *es)
{
  if (!es)
    return FR_ERROR;

  es->curpos = 0;
  return FR_SUCCESS;
}

/**
 * editor_case_word - Change the case of the word
 * @param es State of the Enter buffer
 * @param ec Case change to make, e.g. #EC_UPCASE
 * @retval FR_SUCCESS Case changed
 * @retval FR_ERROR   Error
 */
int editor_case_word(struct EnterState *es, enum EnterCase ec)
{
  if (!es || (es->curpos == es->lastchar))
    return FR_ERROR;

  while ((es->curpos < es->lastchar) && iswspace(es->wbuf[es->curpos]))
  {
    es->curpos++;
  }
  while ((es->curpos < es->lastchar) && !iswspace(es->wbuf[es->curpos]))
  {
    if (ec == EC_DOWNCASE)
    {
      es->wbuf[es->curpos] = towlower(es->wbuf[es->curpos]);
    }
    else
    {
      es->wbuf[es->curpos] = towupper(es->wbuf[es->curpos]);
      if (ec == EC_CAPITALIZE)
        ec = EC_DOWNCASE;
    }
    es->curpos++;
  }
  return FR_SUCCESS;
}

/**
 * editor_delete_char - Delete the char under the cursor
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Character deleted
 * @retval FR_ERROR   Failed, cursor was at the end of the buffer
 */
int editor_delete_char(struct EnterState *es)
{
  if (!es || (es->curpos == es->lastchar))
    return FR_ERROR;

  size_t i = es->curpos;
  if (i < es->lastchar)
    i++;
  while ((i < es->lastchar) && COMB_CHAR(es->wbuf[i]))
    i++;
  memmove(es->wbuf + es->curpos, es->wbuf + i, (es->lastchar - i) * sizeof(wchar_t));
  es->lastchar -= i - es->curpos;

  return FR_SUCCESS;
}

/**
 * editor_eol - Jump to the end of the line
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Cursor moved
 * @retval FR_ERROR   Error
 */
int editor_eol(struct EnterState *es)
{
  if (!es)
    return FR_ERROR;

  es->curpos = es->lastchar;
  return FR_SUCCESS;
}

/**
 * editor_forward_char - Move the cursor one character to the right
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Cursor moved
 * @retval FR_ERROR   Failed, cursor was at the end of the buffer
 */
int editor_forward_char(struct EnterState *es)
{
  if (!es || (es->curpos == es->lastchar))
    return FR_ERROR;

  es->curpos++;
  while ((es->curpos < es->lastchar) && COMB_CHAR(es->wbuf[es->curpos]))
  {
    es->curpos++;
  }

  return FR_SUCCESS;
}

/**
 * editor_forward_word - Move the cursor to the end of the word
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Cursor moved
 * @retval FR_ERROR   Failed, cursor was at the end of the buffer
 */
int editor_forward_word(struct EnterState *es)
{
  if (!es || (es->curpos == es->lastchar))
    return FR_ERROR;

  while ((es->curpos < es->lastchar) && iswspace(es->wbuf[es->curpos]))
  {
    es->curpos++;
  }
  while ((es->curpos < es->lastchar) && !iswspace(es->wbuf[es->curpos]))
  {
    es->curpos++;
  }

  return FR_SUCCESS;
}

/**
 * editor_kill_eol - Delete chars from cursor to end of line
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Characters deleted
 * @retval FR_ERROR   Error
 */
int editor_kill_eol(struct EnterState *es)
{
  if (!es)
    return FR_ERROR;

  es->lastchar = es->curpos;
  return FR_SUCCESS;
}

/**
 * editor_kill_eow - Delete chars from the cursor to the end of the word
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Characters deleted
 * @retval FR_ERROR   Error
 */
int editor_kill_eow(struct EnterState *es)
{
  if (!es)
    return FR_ERROR;

  /* first skip over whitespace */
  size_t i;
  for (i = es->curpos; (i < es->lastchar) && iswspace(es->wbuf[i]); i++)
  {
    // do nothing
  }

  /* if there are any characters left.. */
  if (i < es->lastchar)
  {
    /* if the current character is alphanumeric.. */
    if (iswalnum(es->wbuf[i]))
    {
      /* skip over the rest of the word consistent of only alphanumerics */
      for (; (i < es->lastchar) && iswalnum(es->wbuf[i]); i++)
        ; // do nothing
    }
    else
    {
      i++; // skip over one non-alphanumeric character
    }
  }

  memmove(es->wbuf + es->curpos, es->wbuf + i, (es->lastchar - i) * sizeof(wchar_t));
  es->lastchar += es->curpos - i;
  return FR_SUCCESS;
}

/**
 * editor_kill_line - Delete chars from cursor to beginning the line
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Characters deleted
 * @retval FR_ERROR   Error
 */
int editor_kill_line(struct EnterState *es)
{
  if (!es)
    return FR_ERROR;

  size_t len = es->lastchar - es->curpos;

  memmove(es->wbuf, es->wbuf + es->curpos, len * sizeof(wchar_t));

  es->lastchar = len;
  es->curpos = 0;

  return FR_SUCCESS;
}

/**
 * editor_kill_whole_line - Delete all chars on the line
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Characters deleted
 * @retval FR_ERROR   Error
 */
int editor_kill_whole_line(struct EnterState *es)
{
  if (!es)
    return FR_ERROR;

  es->lastchar = 0;
  es->curpos = 0;

  return FR_SUCCESS;
}

/**
 * editor_kill_word - Delete the word in front of the cursor
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Characters deleted
 * @retval FR_ERROR   Failed, cursor was at the start of the buffer
 */
int editor_kill_word(struct EnterState *es)
{
  if (!es || (es->curpos == 0))
    return FR_ERROR;

  size_t i = es->curpos;
  while (i && iswspace(es->wbuf[i - 1]))
    i--;
  if (i > 0)
  {
    if (iswalnum(es->wbuf[i - 1]))
    {
      for (--i; (i > 0) && iswalnum(es->wbuf[i - 1]); i--)
        ; // do nothing
    }
    else
    {
      i--;
    }
  }
  memmove(es->wbuf + i, es->wbuf + es->curpos,
          (es->lastchar - es->curpos) * sizeof(wchar_t));
  es->lastchar += i - es->curpos;
  es->curpos = i;

  return FR_SUCCESS;
}

/**
 * editor_transpose_chars - Transpose character under cursor with previous
 * @param es State of the Enter buffer
 * @retval FR_SUCCESS Characters switched
 * @retval FR_ERROR   Failed, too few characters
 */
int editor_transpose_chars(struct EnterState *es)
{
  if (!es || (es->lastchar < 2))
    return FR_ERROR;

  if (es->curpos == 0)
    es->curpos = 2;
  else if (es->curpos < es->lastchar)
    es->curpos++;

  wchar_t wc = es->wbuf[es->curpos - 2];
  es->wbuf[es->curpos - 2] = es->wbuf[es->curpos - 1];
  es->wbuf[es->curpos - 1] = wc;

  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * editor_buffer_is_empty - Is the Enter buffer empty?
 * @param es State of the Enter buffer
 * @retval true If buffer is empty
 */
bool editor_buffer_is_empty(struct EnterState *es)
{
  if (!es)
    return true;

  return (es->lastchar == 0);
}

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
 * @param pos New postition for the cursor
 */
void editor_buffer_set_cursor(struct EnterState *es, size_t pos)
{
  if (!es)
    return;

  if (pos >= es->lastchar)
    return;

  es->curpos = pos;
}

/**
 * editor_buffer_set - Set the string in the buffer
 * @param es  State of the Enter buffer
 * @param str String to set
 * @retval num Length of string
 */
int editor_buffer_set(struct EnterState *es, const char *str)

{
  es->wbuflen = 0;
  es->lastchar = mutt_mb_mbstowcs(&es->wbuf, &es->wbuflen, 0, str);
  es->curpos = es->lastchar;
  return es->lastchar;
}
