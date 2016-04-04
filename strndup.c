/*
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"

char *strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = (char *) malloc((len + 1) * sizeof(char));
	if (!new)
		return NULL;
	new[len] = '\0';
	return (char *) memcpy(new, s, len);
}
