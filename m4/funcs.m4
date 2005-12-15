dnl XIPH_ macros are GPL, from http://svn.xiph.org/icecast/trunk/m4
dnl
# XIPH_FUNC_VA_COPY
# Test for implementation of va_copy, or define appropriately if missing
AC_DEFUN([XIPH_FUNC_VA_COPY],
[dnl
AC_MSG_CHECKING([for va_copy])
AC_TRY_LINK([#include <stdarg.h>], [va_list ap1, ap2; va_copy(ap1, ap2);],
  AC_MSG_RESULT([va_copy]),
  [dnl
  AH_TEMPLATE([va_copy], [define if va_copy is not available])
  AC_TRY_LINK([#include <stdarg.h>], [va_list ap1, ap2; __va_copy(ap1, ap2);],
    [dnl
    AC_DEFINE([va_copy], [__va_copy])
    AC_MSG_RESULT([__va_copy])],
    [dnl
    AC_DEFINE([va_copy(dest,src)], [memcpy(&dest,&src,sizeof(va_list))])
    AC_MSG_RESULT([memcpy])
    ])
  ])
])
])dnl XIPH_FUNC_VA_COPY
