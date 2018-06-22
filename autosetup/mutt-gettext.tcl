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
# The 'gettext' module implements a subset of the functionality provided by the
# AM_GNU_GETTEXT macro. Namely, it supports the 'external' configuration where
# a libintl library is supposed to be present in the system, in contrast to it
# being built as part of the package.

use system cc

# @check-gettext
#
# Try to locate a usable gettext installation in the system.
#
# If found, returns 1 and sets 'ENABLE_NLS' to 1. Otherwise, returns 0.
proc check-gettext {prefix} {
    set src {
        #include <libintl.h>
        extern int _nl_msg_cat_cntr;
        extern int *_nl_domain_bindings;
        int main(void)
        {
            bindtextdomain("", "");
            return * gettext("");
        }
    }

    set g_msg_l       [list $prefix           libintl libc]
    set g_cflags_l    [list -I$prefix/include {}      {}  ]
    set g_ldflags_l   [list -L$prefix/lib     {}      {}  ]
    set g_libs_l      [list -lintl            -lintl  {}  ]

    foreach g_msg $g_msg_l g_cflags $g_cflags_l g_ldflags $g_ldflags_l g_libs $g_libs_l {
        msg-checking "Checking for GNU gettext in $g_msg..."
        if {[cctest -cflags $g_cflags -libs "$g_ldflags $g_libs" -link 1 \
                    -source $src]} {
            define-append CFLAGS $g_cflags
            define-append LDFLAGS $g_ldflags
            define-append LIBS $g_libs
            define INTLLIBS "$g_ldflags $g_libs"
            msg-result yes
            cc-check-functions bind_textdomain_codeset
            define ENABLE_NLS
            return 1
        }
        msg-result no
    }
    return 0
}
