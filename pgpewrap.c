/*
 * C version by Wessel Dankers <wsl@fruit.eu.org>
 *
 * This code is in the public domain.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
	char **opts, **opt, *pfx;
	int i;

	opts = malloc((2 * argc + 1) * sizeof (* opts));	/* __MEM_CHECKED__ */
	if(!opts) {
		perror(argv[0]);
		exit(2);
	}

	opt = opts;
	*opt++ = argv[1];
	pfx = NULL;

	for(i = 2; i < argc; ) {
		if(!strcmp(argv[i], "--")) {
			i += 2;
			if(i > argc) {
				fprintf(stderr, "Command line usage: %s [flags] -- prefix [recipients]\n", argv[0]);
				return 1;
			}
			pfx = argv[i-1];
		}
		if(pfx)
			*opt++ = pfx;
		*opt++ = argv[i++];
	}
	*opt = NULL;

	execvp(opts[0], opts);
	perror(argv[0]);
	return 2;
}
