static char rcsid[]="$Id$";
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "mutt.h"

#ifndef STDC_HEADERS
int fclose ();
#endif

/* poor man's version of getdomainname() for systems where it does not return
 * return the DNS domain, but the NIS domain.
 */

int getdnsdomainname (char *s, size_t l)
{
  FILE *f;
  char tmp[1024];
  char *p = NULL;

  if ((f = fopen ("/etc/resolv.conf", "r")) == NULL) return (-1);

  tmp[sizeof (tmp) - 1] = 0;

  l--; /* save room for the terminal \0 */

  while (fgets (tmp, sizeof (tmp) - 1, f) != NULL)
  {
    p = tmp;
    while (ISSPACE (*p)) p++;
    if (strncmp ("domain", p, 6) == 0 || strncmp ("search", p, 6) == 0)
    {
      p += 6;
      while (ISSPACE (*p)) p++;

      if (*p)
      {
	while (*p && !ISSPACE (*p) && l > 0)
	{
	  *s++ = *p++;
	  l--;
	}
	if (*(s-1) == '.') s--;
	*s = 0;

	fclose (f);
	return (0);
      }
    }
  }

  fclose (f);
  return (-1);
}
