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

# ifndef HAVE_UINT32_T
#  if SIZEOF_INT == 4
typedef unsigned int uint32_t;
#  elif SIZEOF_LONG == 4
typedef unsigned long uint32_t;
#  endif
# endif

#endif
