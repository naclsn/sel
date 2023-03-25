#include <stdio.h>
#include <stdlib.h>

#define COUNT 1 << 22

int main(void) {
  char op;
  int a, b;
  size_t k;

  for (k = 0; k < COUNT; k++) {
    a = rand() & 0xff - 0x80;
    b = rand() & 0xff - 0x80;

    switch (rand() % 3) {
      case 0: op = '+'; break;
      case 1: op = '-'; break;
      case 2: op = '*'; break;
    }

    printf("%d %c %d\n", a, op, b);
  }
}
