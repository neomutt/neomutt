# gssapi.m4: Find GSSAPI libraries in either Heimdal or MIT implementations
# Brendan Cully <brendan@kublai.com> 20010529

dnl MUTT_AM_PATH_GSSAPI(PREFIX)
dnl Search for a GSSAPI implementation in the standard locations plus PREFIX,
dnl if it is set and not "yes".
dnl Defines GSSAPI_CFLAGS and GSSAPI_LIBS if found.
dnl Defines GSSAPI_IMPL to "Heimdal", "MIT", or "OldMIT", or "none" if not found
AC_DEFUN([MUTT_AM_PATH_GSSAPI],
[
  GSSAPI_PREFIX=[$]$1
  GSSAPI_IMPL="none"
  saved_CPPFLAGS="$CPPFLAGS"
  saved_LDFLAGS="$LDFLAGS"
  saved_LIBS="$LIBS"
  dnl First try krb5-config
  if test "$GSSAPI_PREFIX" != "yes"
  then
    krb5_path="$GSSAPI_PREFIX/bin"
  else
    krb5_path="$PATH"
  fi
  AC_PATH_PROG(KRB5CFGPATH, krb5-config, none, $krb5_path)
  if test "$KRB5CFGPATH" != "none"
  then
    GSSAPI_CFLAGS="$CPPFLAGS `$KRB5CFGPATH --cflags gssapi`"
    GSSAPI_LIBS="$MUTTLIBS `$KRB5CFGPATH --libs gssapi`"
    case "`$KRB5CFGPATH --version`" in
      "Kerberos 5 "*)	GSSAPI_IMPL="MIT";;
      ?eimdal*)		GSSAPI_IMPL="Heimdal";;
      *)		GSSAPI_IMPL="Unknown";;
   esac
  else
    dnl No krb5-config, run the old code
    if test "$GSSAPI_PREFIX" != "yes"
    then
      GSSAPI_CFLAGS="-I$GSSAPI_PREFIX/include"
      GSSAPI_LDFLAGS="-L$GSSAPI_PREFIX/lib"
      CPPFLAGS="$CPPFLAGS $GSSAPI_CFLAGS"
      LDFLAGS="$LDFLAGS $GSSAPI_LDFLAGS"
    fi

    dnl New MIT kerberos V support
    AC_CHECK_LIB(gssapi_krb5, gss_init_sec_context, [
      GSSAPI_IMPL="MIT",
      GSSAPI_LIBS="$GSSAPI_LDFLAGS -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err"
      ],, -lkrb5 -lk5crypto -lcom_err)

    dnl Heimdal kerberos V support
    if test "$GSSAPI_IMPL" = "none"
    then
      AC_CHECK_LIB(gssapi, gss_init_sec_context, [
          GSSAPI_IMPL="Heimdal"
          GSSAPI_LIBS="$GSSAPI_LDFLAGS -lgssapi -lkrb5 -ldes -lasn1 -lroken"
          GSSAPI_LIBS="$GSSAPI_LIBS -lcrypt -lcom_err"
          ],, -lkrb5 -ldes -lasn1 -lroken -lcrypt -lcom_err)
    fi

    dnl Old MIT Kerberos V
    dnl Note: older krb5 distributions use -lcrypto instead of
    dnl -lk5crypto, which collides with OpenSSL.  One way of dealing
    dnl with that is to extract all objects from krb5's libcrypto
    dnl and from openssl's libcrypto into the same directory, then
    dnl to create a new libcrypto from these.
    if test "$GSSAPI_IMPL" = "none"
    then
      AC_CHECK_LIB(gssapi_krb5, g_order_init, [
        GSSAPI_IMPL="OldMIT",
        GSSAPI_LIBS="$GSSAPI_LDFLAGS -lgssapi_krb5 -lkrb5 -lcrypto -lcom_err"
        ],, -lkrb5 -lcrypto -lcom_err)
    fi
  fi

  CPPFLAGS="$saved_CPPFLAGS"
  LDFLAGS="$saved_LDFLAGS"
  LIBS="$saved_LIBS"
])
