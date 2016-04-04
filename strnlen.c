/*
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"

size_t strnlen(const char *s, size_t maxlen)
{
        int i;

        for (i = 0; i < maxlen; i++) {
                if (s[i] == '\0')
                        return i + 1;
        }
        return maxlen;
}
