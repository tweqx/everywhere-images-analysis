#include <stdio.h>
#include <stdlib.h>

short do_quantization(int dct, int divisor) {
  if (dct < 0) {
    dct = -dct;

    dct += divisor >> 1;

    if (dct >= divisor)
      dct /= divisor;
    else
      dct = 0;

    dct = -dct;
  }
  else {
    dct += divisor >> 1;

    if (dct >= divisor)
      dct /= divisor;
    else
      dct = 0;
  }

  return dct;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: %s divisor\n", argv[0]);
    return 1;
  }

  int divisor = atoi(argv[1]);

  for (int dct = -20; dct <= 20; dct++) {
    short coef = do_quantization(dct, divisor);

    printf("%d -> %d\n", dct, coef);
  }
}
