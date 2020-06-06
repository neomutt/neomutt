#!/bin/sh

BASEVERSION=6
STRUCTURES="Address Body Buffer Email Envelope ListNode Parameter"

cleanstruct () {
  echo "$1" | sed -e 's/.* //'
}

cleanbody () {
  echo "$1" | sed -e 's/{ *//'
}

getstruct () {
  STRUCT=""
  BODY=''
  inbody=0
  case "$1" in
    *'{') inbody=1 ;;
    *';') return ;;
  esac

  STRUCT=`cleanstruct "$1"`

  while read line
  do
    if test $inbody -eq 0
    then
      case "$line" in
        '{'*) inbody=1 ;;
        *';') return ;;
      esac
    fi

    case "$line" in
      '};'*)
        break
      ;;
      '#'*) continue ;;
      *)
        if test $inbody -ne 0
        then
          BODY="$BODY $line"
        fi
      ;;
    esac
  done

  for s in ${STRUCTURES}; do
    if [ ${STRUCT} = $s ]; then
      BODY=`cleanbody "$BODY"`
      echo "$STRUCT: $BODY"
    fi
  done
  return
}

md5prog () {
  prog=""

  # Use OpenSSL if it's installed
  openssl=`which openssl`
  if [ $? = 0 ];then
    # Check that openssl supports the -r option (requires version 1.1.0)
    echo test | openssl md5 -r > /dev/null 2>&1
    if [ $? = 0 ]; then
      echo "$openssl md5 -r"
      return
    fi
  fi

  # Fallback to looking for a system-specific utility
  case "`uname`" in
    SunOS)
      # This matches most of the Solaris family
      prog="digest -a md5"
      ;;
    *BSD|Darwin)
      # Darwin, FreeBSD, NetBSD, and OpenBSD all have md5
      prog="md5"
      ;;
    *)
      # Assume anything else has binutils' md5sum
      prog="md5sum"
      ;;
  esac

  echo $prog
}

DEST="$1"
TMPD="$DEST.tmp"

TEXT="$BASEVERSION"

echo "/* base version: $BASEVERSION" > $TMPD
while read line
do
  case "$line" in
    'struct'*)
       STRUCT=`getstruct "$line"`
       if test -n "$STRUCT"
       then
         NAME=`echo $STRUCT | cut -d: -f1`
         BODY=`echo $STRUCT | cut -d' ' -f2-`
         echo " * $NAME:" $BODY >> $TMPD
         TEXT="$TEXT $NAME {$BODY}"
       fi
    ;;
  esac
done
echo " */" >> $TMPD

MD5PROG=$(md5prog)
MD5TEXT=`echo "$TEXT" | $MD5PROG | cut -c-8`
echo "#define HCACHEVER 0x$MD5TEXT" >> $TMPD

# TODO: validate we have all structs

mv $TMPD $DEST
