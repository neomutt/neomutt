#include <stdio.h>

#define per_line 12

void
txt2c(char *sym, FILE *fp)
{
	unsigned char buf[per_line];
	int i;
	int sz = 0;

	printf("unsigned char %s[] = {\n", sym);
	while (1) {
		sz = fread(buf, sizeof(unsigned char), per_line, fp);
		if (sz == 0)
			break;
		printf("\t");
		for (i = 0; i < sz; i++)
			printf("0x%02x, ", buf[i]);
		printf("\n");
	}

	printf("\t0x00\n};\n");
}


int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s symbol <textfile >textfile.c\n", argv[0]);
		return 2;
	}

	txt2c(argv[1], stdin);
	return 0;
}
