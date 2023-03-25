#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 * @thx https://stackoverflow.com/a/23840699
 */
char* itoa(int value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

int main(void) {
  char line[16];
  char res[8];
  char* at;
  char op;
  int a, b;

  while (fgets(line, 16, stdin) != NULL) {
    at = strchr(line, ' ');
    op = at[1];

    at[0] = '\0';
    a = atoi(line);
    b = atoi(at+3);

    switch (op) {
      case '+': itoa(a+b, res, 10); break;
      case '-': itoa(a-b, res, 10); break;
      case '*': itoa(a*b, res, 10); break;
    }
    puts(res);
  }
}
