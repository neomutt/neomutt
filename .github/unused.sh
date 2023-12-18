#!/usr/bin/env bash

set -euo pipefail

EXIT_CODE=0
IGNORELIST=(
  "log_multiline_full"
  "mutt_socket_buffer_readln_d"
  "mutt_str_upper"
  "progress_set_size"
)

while IFS= read -r line; do
    if ! [[ $line =~ "is unused" ]]; then
        continue
    fi
    read -r func def <<<$(echo $line | sed -rn "s/^(.*:[[:digit:]]+):.*Function '(.*)'.*$/\2 \1/p")
    for i in "${IGNORELIST[@]}"; do
        if [[ $i == "$func" ]]; then
            continue 2
        fi
    done
    printf '%s (%s)\n' "$func" "$def"
    EXIT_CODE=1
done

exit $EXIT_CODE
