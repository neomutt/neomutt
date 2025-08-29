#!/usr/bin/env bash

set -euo pipefail

EXIT_CODE=0
IGNORELIST=(
    # Used by macros
    "log_multiline_full"
    "mutt_socket_buffer_readln_d"

    # Used by tests
    "mutt_str_upper"
    "name_expando_domain"
    "name_expando_node_type"
    "name_expando_pad_type"
    "name_expando_uid"
    "name_format_justify"

    # Will be used by future code
    "progress_set_size"
)

while IFS= read -r line; do
    if ! [[ $line =~ "is unused" ]]; then
        continue
    fi
    read -r func def <<< "$(echo "$line" | sed -rn "s/^(.*:[[:digit:]]+):.*Function '(.*)'.*$/\2 \1/p")"
    for i in "${IGNORELIST[@]}"; do
        if [[ $i == "$func" ]]; then
            continue 2
        fi
    done
    printf '%s (%s)\n' "$func" "$def"
    EXIT_CODE=1
done

exit $EXIT_CODE
