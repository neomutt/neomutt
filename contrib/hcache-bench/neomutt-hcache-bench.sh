#!/bin/sh
#
# Copyright 2017 Pietro Cerutti <gahr@gahr.ch>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

usage()
{
    echo "Usage: $(basename "$0") -e <neomutt> -m <mdir> -t <times> -b <backends>"
    echo ""
    echo "   -e Path to the neomutt executable"
    echo "   -m Path to a maildir directory"
    echo "   -t Number of times to repeat the test"
    echo "   -b List of backends to test"
    echo ""
}

while getopts e:m:t:b: OPT; do
    case "$OPT" in
        e)
            NEOMUTT="$OPTARG"
            ;;
        m)
            MAILDIR="$OPTARG"
            ;;
        t)
            TIMES="$OPTARG"
            ;;
        b)
            BACKENDS="$OPTARG"
            ;;
        *)
            usage
            exit 1
    esac
done

if [ -z "$MAILDIR" ] || [ -z "$NEOMUTT" ] || [ -z "$TIMES" ] || [ -z "$BACKENDS" ]; then
    usage
    exit 1
fi

CWD=$(dirname $(realpath $0))
TMPDIR=$(mktemp -d)

echo "Running in $TMPDIR"

exe()
{
    export my_backend=$1
    export my_maildir=$MAILDIR
    export my_tmpdir=$TMPDIR
    t=$(time -p $NEOMUTT -F "$CWD"/muttrc 2>&1 > /dev/null)
    echo "$t" | xargs
}

extract()
{
    grep "$2" "$TMPDIR/result-$1.txt" | awk "{print \$$3}" | xargs
}

avg()
{
    echo "3 k 0 $* $(printf "%*s" "$TIMES" "" | tr ' ' '+') $TIMES / p" | dc
}

width=${#TIMES}

# generate
for i in $(seq "$TIMES"); do
    for b in $BACKENDS; do
        rm -f "$TMPDIR"/hcache*
        # do it twice - the first will populate the cache, the second will reload it
        printf "%${width}d - populating - $b\n" "$i"
        t1=$(exe "$b")
        printf "%${width}d - reloading  - $b\n" "$i"
        t2=$(exe "$b")
        s=$(du -k "$TMPDIR/hcache-$b" | awk '{print $1}')
        echo "$b $s $t1" >> "$TMPDIR"/result-populate.txt
        echo "$b $s $t2" >> "$TMPDIR"/result-reload.txt
    done
done

for f in populate reload; do
    echo ""
    echo "*** $f"
    for b in $BACKENDS; do
        real=$(avg "$(extract "$f" "$b" 4)")
        user=$(avg "$(extract "$f" "$b" 6)")
        sys=$(avg "$(extract "$f" "$b" 8)")
        printf "%-15s" "$b"
        echo "$real real $user user $sys sys"
    done
done
