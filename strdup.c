/* ultrix doesn't have strdup */

#include <string.h>
#include <stdlib.h>

char *strdup (const char *s)	/* __MEM_CHECKED__ */
{
  char *d;
  
  if (s == NULL)
    return NULL;
  
  if ((d = malloc (strlen (s) + 1)) == NULL)	/* __MEM_CHECKED__ */
    return NULL;

  memcpy (d, s, strlen (s) + 1);
  return d;
}

