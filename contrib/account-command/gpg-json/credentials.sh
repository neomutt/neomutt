#!/bin/sh

host=""
user=""

while [ $# -ne 0 ]; do
    case "$1" in
        "--hostname") shift; host="$1";;
        "--username") shift; user="$1";;
    esac
    shift
done

[ "$host" = "" ] || [ "$user" = "" ] && exit 1

query=$(printf '.host == "%s" and .user == "%s"' "$host" "$user")
gpg -qd credentials.json.gpg | jq -r ".[] | select($query) | .pass"
