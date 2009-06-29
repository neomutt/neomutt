#ifndef _URL_H
# define _URL_H

typedef enum url_scheme
{
  U_FILE,
  U_POP,
  U_POPS,
  U_IMAP,
  U_IMAPS,
  U_SMTP,
  U_SMTPS,
  U_MAILTO,
  U_UNKNOWN
}
url_scheme_t;

#define U_DECODE_PASSWD (1)
#define U_PATH          (1 << 1)

typedef struct ciss_url
{
  url_scheme_t scheme;
  char *user;
  char *pass;
  char *host;
  unsigned short port;
  char *path;
} 
ciss_url_t;

url_scheme_t url_check_scheme (const char *s);
int url_parse_file (char *d, const char *src, size_t dl);
int url_parse_ciss (ciss_url_t *ciss, char *src);
int url_ciss_tostring (ciss_url_t* ciss, char* dest, size_t len, int flags);
int url_parse_mailto (ENVELOPE *e, char **body, const char *src);

#endif
