/*
 * Copyright (C) 1998 Ruslan Ermilov <ru@ucb.crimea.ua>,
 *                    Thomas Roessler <roessler@guug.de>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "mutt.h"
#include "charset.h"

#include <string.h>
#include <ctype.h>

static HASH *Translations = NULL;
static HASH *Charsets = NULL;

static CHARSET *mutt_new_charset(void)
{
  CHARSET *chs;
  
  chs       = safe_malloc(sizeof(CHARSET));
  chs->name = NULL;
  chs->map  = NULL;
  
  return chs;
}

#if 0

static void mutt_free_charset(CHARSET **chsp)
{
  CHARSET *chs = *chsp;
  
  safe_free((void **) &chs->name);
  safe_free((void **) &chs->map);
  safe_free((void **) chsp);
}

#endif

static void canonical_charset(char *dest, size_t dlen, const char *name)
{
  int i;
  
  if(!strncasecmp(name, "x-", 2))
    name = name + 2;
  
  for(i = 0; name[i] && i < dlen - 1; i++)
  {
    if(strchr("_/. ", name[i]))
      dest[i] = '-';
    else
      dest[i] = tolower(name[i]);
  }
  
  dest[i] = '\0';
}

static CHARSET *load_charset(const char *name)
{
  char path[_POSIX_PATH_MAX];
  char buffer[SHORT_STRING];
  CHARSET *chs;
  FILE *fp = NULL;
  int i;

  chs = mutt_new_charset();
  chs->name = safe_strdup(name);
  
  snprintf(path, sizeof(path), "%s/charsets/%s", SHAREDIR, name);
  if((fp = fopen(path, "r")) == NULL)
    goto bail;
  
  if(fgets(buffer, sizeof(buffer), fp) == NULL)
    goto bail;
  
  if(strcmp(buffer, CHARSET_MAGIC) != 0)
    goto bail;

  chs->map  = safe_malloc(sizeof(UNICODE_MAP));
  
  for(i = 0; i < 256; i++)
  {
    if(fscanf(fp, "%i", &(*chs->map)[i]) != 1)
    {
      safe_free((void **) &chs->map);
      break;
    }
  }

  bail:
  
  if(fp) fclose(fp);
  return chs;
}

static void init_charsets()
{
  if(Charsets) return;

  Charsets     = hash_create(128);
  Translations = hash_create(256);
}

CHARSET *mutt_get_charset(const char *name)
{
  CHARSET *charset;
  char buffer[SHORT_STRING];
  
  init_charsets();
  canonical_charset(buffer, sizeof(buffer), name);
  if(!(charset = hash_find(Charsets, buffer)))
  {
    charset = load_charset(buffer);
    hash_insert(Charsets, buffer, charset, 0);
  }
  return charset;
}

static int translate_char(UNICODE_MAP *to, int ch)
{
  int i;

  if (ch == -1) return '?';
  for (i = 0; i < 256; i++)
  {
    if ((*to)[i] == ch) 
      return i;
  }
  return '?';
}

UNICODE_MAP *build_translation(UNICODE_MAP *from, UNICODE_MAP *to)
{
  int i;
  UNICODE_MAP *map = safe_malloc(sizeof(UNICODE_MAP));

  for(i = 0; i < 256; i++)
    (*map)[i] = translate_char(to, (*from)[i]);

  return map;
}

UNICODE_MAP *mutt_get_translation(const char *_from, const char *_to)
{
  char from[SHORT_STRING];
  char to[SHORT_STRING];
  char key[SHORT_STRING];
  CHARSET *from_cs, *to_cs;
  UNICODE_MAP *map;

  if(!_from || !_to)
    return NULL;
  
  init_charsets();

  canonical_charset(from, sizeof(from), _from);
  canonical_charset(to, sizeof(to), _to);
  
  /* quick check for the identity mapping */
  if((*from == *to) && !strcmp(from, to))
    return NULL;
  
  snprintf(key, sizeof(key), "%s %s", from, to);
  if((map = hash_find(Translations, key)) == NULL)
  {
    from_cs = mutt_get_charset(from);
    to_cs   = mutt_get_charset(to);

    if(!from_cs->map || !to_cs->map)
      return NULL;
    
    map = build_translation(from_cs->map, to_cs->map);
    hash_insert(Translations, key, map, 0);
  }
  return map;
}

int mutt_display_char(int ch, UNICODE_MAP *map)
{
  if (!map || (ch < 0) || (ch > 255))
    return ch;

  return (*map)[ch];
}

int mutt_display_string(char *str, UNICODE_MAP *map)
{
  if(!map)
    return -1;

  while ((*str = mutt_display_char((unsigned char)*str, map)))
    str++;

  return 0;
}
