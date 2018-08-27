# Copyright (c) 2017 Pietro Cerutti <gahr@gahr.ch>. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

# @synopsis:
#
# The 'iconv' module tries to mimic the logic provided by the AM_ICONV macro,
# by checking for a working iconv() implementation first in libiconv under
# prefix, then in libiconv alone, and last in libc.
#

use system cc

# @check-iconv
#
# Try to locate a usable iconv() implementation, and check whether the second
# parameter to iconv() needs a const qualifier.
#
# If found, returns 1 and sets 'HAVE_ICONV' to 1 and ICONV_CONST to either the
# emppty string or 'const', depending on the signature of iconv()
# If not found, returns 0.
proc check-iconv {prefix} {
  set iconv_code {
    iconv_t cd = iconv_open("", "");
    iconv(cd, NULL, NULL, NULL, NULL);
    iconv_close(cd);
  }

  set iconv_const_code {
    size_t iconv (iconv_t   cd,
                  char    **inbuf,
                  size_t   *inbytesleft,
                  char    **outbuf,
                  size_t   *outbytesleft);
  }

  set iconv_inc {stdlib.h iconv.h}

  # Check to compile and link a simple iconv() call as follows:
  # 1. check with $prefix using -liconv
  # 2. check without prefix using -liconv
  # 3. check without prefix using only libc

  #                     prefix            noprefix  libc
  set i_msg_l     [list $prefix           libiconv  libc]
  set i_cflags_l  [list -I$prefix/include {}        {}  ]
  set i_ldflags_l [list -L$prefix/lib     {}        {}  ]
  set i_libs_l    [list -liconv           -liconv   {}  ]

  foreach i_msg $i_msg_l i_cflags $i_cflags_l i_ldflags $i_ldflags_l i_libs $i_libs_l {
    msg-checking "Checking for iconv() in $i_msg..."
    if {[cctest -cflags $i_cflags -libs "$i_ldflags $i_libs" -link 1 \
                -includes $iconv_inc -code $iconv_code]} {
      define-append CFLAGS $i_cflags
      define-append LDFLAGS $i_ldflags
      define-append LIBS $i_libs
      define-feature ICONV
      msg-result "yes"

      # ICONV_CONST
      msg-checking "Checking whether iconv() needs const..."
      if {[cctest -cflags $i_cflags -libs "$i_ldflags $i_libs" -link 1 \
                  -includes $iconv_inc -code $iconv_const_code]} {
        msg-result "no"
        define ICONV_CONST ""
      } else {
        msg-result "yes"
        define ICONV_CONST const
      }

      # we found it
      return 1
    }
    msg-result "no"
  }
  return 0
}
