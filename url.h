#ifndef _URL_H
# define _URL_H

# include "config.h"

typedef enum url_scheme
{
  U_FILE,
# ifdef USE_POP
  U_POP,
# endif
# ifdef USE_IMAP
  U_IMAP,
# endif
# ifdef USE_SSL
#  ifdef USE_IMAP
  U_IMAPS,
#  endif
#  ifdef USE_POP
  U_POPS,
#  endif
# endif
  U_MAILTO,
  U_UNKNOWN
}
url_scheme_t;

typedef struct ciss_url
{
  char *user;
  char *pass;
  char *host;
  short port;
  char  *path;
} 
ciss_url_t;

url_scheme_t url_check_scheme (const char *s);
int url_parse_file (char *d, const char *src, size_t dl);
int url_parse_ciss (ciss_url_t *ciss, const char *src);
int url_parse_mailto (ENVELOPE *e, char **body, const char *src);

#endif
