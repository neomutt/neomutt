/**
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#define per_line 12

static void txt2c(char *sym, FILE *fp)
{
  unsigned char buf[per_line];
  int i;
  int sz = 0;

  printf("unsigned char %s[] = {\n", sym);
  while (1)
  {
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


int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s symbol <textfile >textfile.c\n", argv[0]);
    return 2;
  }

  txt2c(argv[1], stdin);
  return 0;
}
