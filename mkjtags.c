/* 
 * convert multi-file etags files to something
 * which can be used by jed.
 * 
 * Thomas Roessler <roessler@guug.de>
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void doit (const char *fname, const char *prefix, int crlf_pending);

  
int main (int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf (stderr, "usage: %s filename\n", argv[0]);
    exit (1);
  }
  
  doit (argv[1], NULL, 0);
  return 0;
}

void doit (const char *fname, const char *prefix, int crlf_pending)
{
  char buffer[2048];
  char tmpf[2048];
  FILE *fp;
  char *cp;
  
  size_t l;
  
  if (!(fp = fopen (fname, "r")))
  {
    perror (fname);
    exit (1);
  }
  
  while (fgets (buffer, sizeof (buffer), fp))
  {
    l = strlen (buffer);
    if (*buffer == '\f')
    {
      if (!crlf_pending) 
	fputs (buffer, stdout);
    }
    else if (crlf_pending && l > 9 && !strcmp (buffer + l - 9, ",include\n"))
    {
      if ((cp = strrchr (buffer, ',')))
	*cp = 0;
      strcpy (tmpf, buffer);
      
      if ((cp = strrchr (buffer, '/')))
	*cp = 0;
	
      doit (tmpf, buffer, crlf_pending);
    }
    else if (crlf_pending && prefix)
      printf ("%s/%s", prefix, buffer);
    else
      fputs (buffer, stdout);

    crlf_pending = (*buffer == '\f');
  }
  
  fclose (fp);
}

  
