#!/bin/sh

cred=""
host=""
user=""

while [ $# -ne 0 ]; do
    case "$1" in
        "--credfile") shift; cred="$1";;
        "--hostname") shift; host="$1";;
        "--username") shift; user="$1";;
    esac
    shift
done

[ -z "$cred" ] || [ -z "$host" ] || [ -z "$user" ] && exit 1

query=$(printf '.host == "%s" and .user == "%s"' "$host" "$user")
gpg -qd "$cred" | jq -r ".[] | select($query) | .pass"
