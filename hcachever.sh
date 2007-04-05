#!/bin/sh

BASEVERSION=1

if which md5 > /dev/null
then
  MD5=md5
elif which md5sum > /dev/null
then
  MD5=md5sum
elif which openssl > /dev/null
then
  MD5="openssl md5 -hex"
else
  echo "ERROR: no MD5 tool found"
  exit 1
fi

cleanstruct () {
  STRUCT="$1"
  STRUCT=${STRUCT#\} }
  STRUCT=${STRUCT%\;}

  echo $STRUCT
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
      '} '*)
        STRUCT=`cleanstruct "$line"`
        break
      ;;
      '}')
        read line
        STRUCT=`cleanstruct "$line"`
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

  case $STRUCT in
    ADDRESS|LIST|BUFFER|PARAMETER|BODY|ENVELOPE)
      BODY=`cleanbody "$BODY"`
      echo "$STRUCT: $BODY"
    ;;
  esac
  return
}

DEST="$1"
TMPD="$DEST.tmp"

TEXT="$BASEVERSION"

echo "/* base version: $BASEVERSION" > $TMPD
while read line
do
  case "$line" in
    'typedef struct'*)
       STRUCT=`getstruct "$line"`
       if test -n "$STRUCT"
       then
         NAME=${STRUCT%%:*}
         BODY=${STRUCT#*:}
         echo " * $NAME:" $BODY >> $TMPD
         TEXT="$TEXT $NAME {$BODY}"
       fi
    ;;
  esac
done
echo " */" >> $TMPD

MD5TEXT=`echo $TEXT | $MD5`
echo "#define HCACHEVER 0x"${MD5TEXT:0:8} >> $TMPD

# TODO: validate we have all structs

mv $TMPD $DEST
