static const char rcsid[]="$Id$";
#include <ctype.h>
#include <sys/types.h>

/* UnixWare doesn't have these functions in its standard C library */

int mutt_strncasecmp (char *s1, char *s2, size_t n)
{
    register int c1, c2, l = 0;

    while (*s1 && *s2 && l < n)
    {
      c1 = tolower (*s1);
      c2 = tolower (*s2);
      if (c1 != c2)
	return (c1 - c2);
      s1++;
      s2++;
      l++;
    }
    return (int) (0);
}

int mutt_strcasecmp (char *s1, char *s2)
{
    register int c1, c2;

    while (*s1 && *s2)
    {
      c1 = tolower (*s1);
      c2 = tolower (*s2);
      if (c1 != c2)
	return (c1 - c2);
      s1++;
      s2++;
    }                                                                           
    return (int) (*s1 - *s2);
}
