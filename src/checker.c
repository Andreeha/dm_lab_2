#include <stdio.h>
#include <stdlib.h>

int main () {
  FILE* f = fopen("file.bin", "rb+");
  fseek(f, 1084, SEEK_SET);
  char* buf = malloc(sizeof(char) * 74);
  fread(buf, 74, 1, f);

  for (int i = 0; i < 74; i += 2) {
    printf("%02x%02x ", (unsigned char)buf[i], (unsigned char)buf[i+1]);
    if (i % 16 == 14) printf("\n");
  }
  printf("\n");
  return 0;
}
