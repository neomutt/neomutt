/*
 * Copyright (C) 1998 Thomas Roessler <roessler@guug.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define _GEN_CHARSETS

#include "../charset.h"

#if 0
#define DEBUG
#endif

static char *basedir = NULL;

typedef int MAP[256];

typedef struct alias
{
  char *charset;
  struct alias *next;
} ALIAS;

typedef struct
{
  char *charset;
  char escape_char;
  char comment_char;
  short is_valid;
  ALIAS *aliases;
  MAP map;
} CHARMAP;

static void *safe_malloc(size_t l)
{
  void *p = malloc(l);
  if(!p)
  {
    perror("malloc");
    exit(1);
  }
  return p;
}

static char *safe_strdup(const char *s)
{
  char *d;
  
  if(!s) 
    return NULL;
  
  if(!(d = strdup(s)))
  {
    perror("strdup");
    exit(1);
  }
  return d;
}

static void safe_free(void **p)
{
  if(!p || !*p) return;
  free(*p);
  *p = NULL;
}

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

static CHARMAP *charmap_new(void)
{
  int i;
  CHARMAP *m = safe_malloc(sizeof(CHARMAP));
  
  m->charset = NULL;
  m->escape_char = '\\';
  m->comment_char = '#';
  m->is_valid = 0;

  m->aliases = NULL;
  
  for(i = 0; i < 256; i++)
    m->map[i] = -1;
  
  return m;
}

static void charmap_free(CHARMAP **cp)
{
  ALIAS *p, *q;
  
  if(!cp || !*cp)
    return ;
  
  for(p = (*cp)->aliases; p; p = q)
  {
    q = p->next;
    safe_free((void **) &p->charset);
    safe_free((void **) &p);
  }
  
  safe_free((void **) &(*cp)->charset);
  safe_free((void **) cp);
  
  return;
}

static void add_alias(CHARMAP *m, const char *alias)
{
  ALIAS *aptr;

  aptr = safe_malloc(sizeof(ALIAS));
  aptr->charset = safe_strdup(alias);
  canonical_charset(aptr->charset, strlen(aptr->charset) + 1, aptr->charset);
  aptr->next = m->aliases;
  m->aliases = aptr;
}

static CHARMAP *parse_charmap_header(FILE *fp, const char *prefix)
{
  char buffer[1024];
  char *t, *u;
  CHARMAP *m = charmap_new();
  
  while(fgets(buffer, sizeof(buffer), fp))
  {
    if((t = strchr(buffer, '\n')))
      *t = '\0';
    else
    {
      fprintf(stderr, "%s: Line too long.", prefix);
      charmap_free(&m);
      return NULL;
    }

    if(!strncmp(buffer, "CHARMAP", 7))
      break;

    if(*buffer == m->comment_char)
    {
      if((t = strtok(buffer + 1, "\t ")) && !strcasecmp(t, "alias"))
      {
	while((t = strtok(NULL, "\t, ")))
	  add_alias(m, t);
      }
      continue;
    }
    
    if(!(t = strtok(buffer, "\t ")))
      continue;
    
    if(!(u = strtok(NULL, "\t ")))
    {
      fprintf(stderr, "%s: Syntax error.\n", prefix);
      charmap_free(&m);
      return NULL;
    }

    if(!strcmp(t, "<code_set_name>"))
    {
      safe_free((void **) &m->charset);
      canonical_charset(u, strlen(u) + 1, u);
      m->charset = safe_strdup(u);
#ifdef DEBUG
      fprintf(stderr, "code_set_name: `%s'\n", m->charset);
#endif
    }
    else if(!strcmp(t, "<comment_char>"))
    {
      m->comment_char = *u;
#ifdef DEBUG
      fprintf(stderr, "comment_char: `%c'\n", m->comment_char);
#endif
    }
    else if(!strcmp(t, "<escape_char>"))
    {
      m->escape_char = *u;
#ifdef DEBUG
      fprintf(stderr, "escape_char: `%c'\n", m->escape_char);
#endif
    }
  }
  
  return m;
}

static void parse_charmap_body(FILE *fp, CHARMAP *m, const char *prefix)
{
  char buffer[1024];
  char *ch, *t;
  int idx, value;
  
  while(fgets(buffer, sizeof(buffer), fp))
  {
    if((t = strchr(buffer, '\n')))
      *t = '\0';
    else
    {
      fprintf(stderr, "%s: Line too long.\n", prefix);
      return;
    }
    
    if(*buffer == m->comment_char)
      continue;
    
    if(!strncmp(buffer, "END CHARMAP", 11))
      break;
    
    if(!(ch = strtok(buffer, " \t")))
      continue;
    
    if(!(t = strtok(NULL, " \t")))
    {
      fprintf(stderr, "%s: Syntax error in definition of `%s'.\n", prefix, ch);
      continue;
    }
    
    /* parse the character encoding */
    if(*t++ != m->escape_char)
    {
      fprintf(stderr, "%s: Bad encoding for character `%s'.\n", prefix, ch);
      continue;
    }
    
    switch(*t++)
    {
      case 'x':
        idx = strtol(t, NULL, 16);
        break;
      case 'd':
        idx = strtol(t, NULL, 10);
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        idx = strtol(t, NULL, 8);
        break;
      default:
        fprintf(stderr, "%s: Bad encoding for character `%s'.\n", prefix, ch);
        continue;
    }
    
    if(!(t = strtok(NULL, "\t ")))
    {
      fprintf(stderr, "%s: No comment for `%s'.\n", prefix, ch);
      continue;
    }
    
    if(strncmp(t, "<U", 2))
    {
      fprintf(stderr, "%s: No unicode value for `%s'.\n", prefix, ch);
      continue;
    }
    
    value = strtol(t + 2, NULL, 16);
#if 0
    if(value == LONG_MIN || value == LONG_MAX)
    {
      fprintf(stderr, "%s: Bad unicode value for `%s'.\n", prefix, ch);
      continue;
    }
#endif
    if(0 < idx && idx < 256)
    {
      m->map[idx] = value;
      m->is_valid = 1;
    }
  }
}

static void write_charmap(FILE *fp, CHARMAP *m)
{
  int i;

  fputs(CHARSET_MAGIC, fp);
  
  for(i = 0; i < 256; i++)
    fprintf(fp, "%d\n", m->map[i]);
}

int main(int argc, const char *argv[])
{
  FILE *fp;
  CHARMAP *m;
  ALIAS *aptr;
  int i;
  char buffer[1024];
  
  basedir = safe_strdup(argv[1]);
  
  for(i = 2; i < argc; i++)
  {
    if(!strcmp(argv[i], "-"))
      fp = stdin;
    else if(!(fp = fopen(argv[i], "r")))
    {
      perror(argv[i]);
      continue;
    }
    
    if((m = parse_charmap_header(fp, argv[i])))
      parse_charmap_body(fp, m, argv[i]);
    
    fclose(fp);
    
    if(m && m->charset && m->is_valid 
       && (basedir ? 0 : strlen(basedir) + 1) 
          + strlen(m->charset) + 1 < sizeof(buffer))
    {
      sprintf(buffer, "%s%s%s", basedir ? basedir : "", basedir ? "/" : "",
	       m->charset);
      
      if((fp = fopen(buffer, "w")))
      {
	write_charmap(fp, m);
	
	printf("charset %s\n", m->charset);
	for(aptr = m->aliases; aptr; aptr = aptr->next)
	{
	  if(strcmp(aptr->charset, m->charset))
	    printf("alias %s %s\n", aptr->charset, m->charset);
	}
	
	fclose(fp);
      }
      else
      {
	perror(buffer);
      }
    }
    charmap_free(&m);
  }
  return 0;
}
