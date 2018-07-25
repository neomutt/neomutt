/**
 * @file
 * Dump all the config
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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
 * @page config-dump Dump all the config
 *
 * Dump all the config items in various formats.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "dump.h"
#include "set.h"
#include "types.h"

void mutt_pretty_mailbox(char *s, size_t buflen);

/**
 * escape_string - Write a string to a buffer, escaping special characters
 * @param buf Buffer to write to
 * @param src String to write
 * @retval num Bytes written to buffer
 */
size_t escape_string(struct Buffer *buf, const char *src)
{
  if (!buf || !src)
    return 0;

  size_t len = 0;
  for (; *src; src++)
  {
    switch (*src)
    {
      case '\n':
        len += mutt_buffer_addstr(buf, "\\n");
        break;
      case '\r':
        len += mutt_buffer_addstr(buf, "\\r");
        break;
      case '\t':
        len += mutt_buffer_addstr(buf, "\\t");
        break;
      default:
        if ((*src == '\\') || (*src == '"'))
          len += mutt_buffer_addch(buf, '\\');
        len += mutt_buffer_addch(buf, src[0]);
    }
  }
  return len;
}

/**
 * pretty_var - Escape and stringify a config item value
 * @param str    String to escape
 * @param buf    Buffer to write to
 * @retval num Number of bytes written to buffer
 */
size_t pretty_var(const char *str, struct Buffer *buf)
{
  if (!buf || !str)
    return 0;

  int len = 0;

  len += mutt_buffer_addch(buf, '"');
  len += escape_string(buf, str);
  len += mutt_buffer_addch(buf, '"');

  return len;
}

/**
 * elem_list_sort - Sort two HashElem pointers to config
 * @param a First HashElem
 * @param b Second HashElem
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int elem_list_sort(const void *a, const void *b)
{
  const struct HashElem *hea = *(struct HashElem **) a;
  const struct HashElem *heb = *(struct HashElem **) b;

  return mutt_str_strcasecmp(hea->key.strkey, heb->key.strkey);
}

/**
 * get_elem_list - Create a sorted list of all config items
 * @param cs ConfigSet to read
 * @retval ptr Null-terminated array of HashElem
 */
struct HashElem **get_elem_list(struct ConfigSet *cs)
{
  if (!cs)
    return NULL;

  struct HashElem **list = mutt_mem_calloc(1024, sizeof(struct HashElem *));
  size_t index = 0;

  struct HashWalkState walk;
  memset(&walk, 0, sizeof(walk));

  struct HashElem *he = NULL;
  while ((he = mutt_hash_walk(cs->hash, &walk)))
  {
    list[index++] = he;
    if (index == 1022)
    {
      mutt_debug(1, "Too many config items to sort\n");
      break;
    }
  }

  qsort(list, index, sizeof(struct HashElem *), elem_list_sort);

  return list;
}

/**
 * dump_config_mutt - Dump the config in the style of Mutt
 * @param cs      Config items
 * @param he      HashElem representing config item
 * @param value   Current value of the config item
 * @param initial Initial value of the config item
 * @param flags   Flags, e.g. #CS_DUMP_ONLY_CHANGED
 */
void dump_config_mutt(struct ConfigSet *cs, struct HashElem *he,
                      struct Buffer *value, struct Buffer *initial, int flags)
{
  const char *name = he->key.strkey;

  if (DTYPE(he->type) == DT_BOOL)
  {
    if ((value->data[0] == 'y') || ((value->data[0] == '"') && (value->data[1] == 'y')))
    {
      printf("%s is set\n", name);
    }
    else
    {
      printf("%s is unset\n", name);
    }
  }
  else
  {
    printf("%s=%s\n", name, value->data);
  }
}

/**
 * dump_config_neo - Dump the config in the style of NeoMutt
 * @param cs      Config items
 * @param he      HashElem representing config item
 * @param value   Current value of the config item
 * @param initial Initial value of the config item
 * @param flags   Flags, e.g. #CS_DUMP_ONLY_CHANGED
 */
void dump_config_neo(struct ConfigSet *cs, struct HashElem *he,
                     struct Buffer *value, struct Buffer *initial, int flags)
{
  const char *name = he->key.strkey;

  if ((flags & CS_DUMP_ONLY_CHANGED) && (mutt_str_strcmp(value->data, initial->data) == 0))
    return;

  if (he->type == DT_SYNONYM)
  {
    const struct ConfigDef *cdef = he->data;
    const char *syn = (const char *) cdef->initial;
    printf("# synonym: %s -> %s\n", name, syn);
    return;
  }

  bool show_name = !(flags & CS_DUMP_HIDE_NAME);
  bool show_value = !(flags & CS_DUMP_HIDE_VALUE);

  if (show_name && show_value)
    printf("set ");
  if (show_name)
    printf("%s", name);
  if (show_name && show_value)
    printf(" = ");
  if (show_value)
    printf("%s", value->data);
  if (show_name || show_value)
    printf("\n");

  if (flags & CS_DUMP_SHOW_DEFAULTS)
  {
    const struct ConfigSetType *cst = cs_get_type_def(cs, he->type);
    if (cst)
      printf("# %s %s %s\n", cst->name, name, value->data);
  }
}

/**
 * dump_config - Write all the config to stdout
 * @param cs    ConfigSet to dump
 * @param style Output style, e.g. #CS_DUMP_STYLE_MUTT
 * @param flags Display flags, e.g. #CS_DUMP_ONLY_CHANGED
 */
bool dump_config(struct ConfigSet *cs, int style, int flags)
{
  if (!cs)
    return false;

  struct HashElem *he = NULL;

  struct HashElem **list = get_elem_list(cs);
  if (!list)
    return false;

  bool result = true;

  struct Buffer *value = mutt_buffer_alloc(1024);
  struct Buffer *initial = mutt_buffer_alloc(1024);
  struct Buffer *tmp = mutt_buffer_alloc(1024);

  for (size_t i = 0; list[i]; i++)
  {
    mutt_buffer_reset(value);
    mutt_buffer_reset(initial);
    he = list[i];
    const int type = DTYPE(he->type);

    if ((type == DT_SYNONYM) && !(flags & CS_DUMP_SHOW_SYNONYMS))
      continue;

    // if ((type == DT_DISABLED) && !(flags & CS_DUMP_SHOW_DISABLED))
    //   continue;

    if (type != DT_SYNONYM)
    {
      /* If necessary, get the current value */
      if ((flags & CS_DUMP_ONLY_CHANGED) || !(flags & CS_DUMP_HIDE_VALUE) ||
          (flags & CS_DUMP_SHOW_DEFAULTS))
      {
        int rc = cs_he_string_get(cs, he, value);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
        {
          result = false;
          break;
        }

        const struct ConfigDef *cdef = he->data;
        if (IS_SENSITIVE(*cdef) && (flags & CS_DUMP_HIDE_SENSITIVE) &&
            !mutt_buffer_is_empty(value))
        {
          mutt_buffer_reset(value);
          mutt_buffer_addstr(value, "***");
        }

        // if (type == DT_PATH)
        //   mutt_pretty_mailbox(value->data, value->dsize);

        if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_QUAD) &&
            !(flags & CS_DUMP_NO_ESCAPING))
        {
          mutt_buffer_reset(tmp);
          size_t len = pretty_var(value->data, tmp);
          mutt_str_strfcpy(value->data, tmp->data, len + 1);
        }
      }

      /* If necessary, get the default value */
      if (flags & (CS_DUMP_ONLY_CHANGED | CS_DUMP_SHOW_DEFAULTS))
      {
        int rc = cs_he_initial_get(cs, he, initial);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
        {
          result = false;
          break;
        }

        // if (type == DT_PATH)
        //   mutt_pretty_mailbox(value->data, value->dsize);

        if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_QUAD) &&
            !(flags & CS_DUMP_NO_ESCAPING))
        {
          mutt_buffer_reset(tmp);
          size_t len = pretty_var(initial->data, tmp);
          mutt_str_strfcpy(value->data, tmp->data, len + 1);
        }
      }
    }

    if (style == CS_DUMP_STYLE_MUTT)
      dump_config_mutt(cs, he, value, initial, flags);
    else
      dump_config_neo(cs, he, value, initial, flags);
  }

  FREE(&list);
  mutt_buffer_free(&value);
  mutt_buffer_free(&initial);
  mutt_buffer_free(&tmp);

  return result;
}
