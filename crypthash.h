#ifndef _CRYPTHASH_H
# define _CRYPTHASH_H


# include <sys/types.h>
# if HAVE_INTTYPES_H
#  include <inttypes.h>
# else
#  if HAVE_STDINT_H
#   include <stdint.h>
#  endif
# endif

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

#endif
