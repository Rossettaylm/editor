#include <asm-generic/errno-base.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

int main(int argc, char *argv[]) {
  enableRawMode();

  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
      die("read");
    }
    // test whether c is a control character
    if (iscntrl(c)) {
      printf("%d\r\n", c);  // (non-printable ASCII 0-31)
    } else {
      printf("%d ('%c')\r\n", c, c);  // (printable ASCII 32-126)
    }
    if (c == 'q') break;
  }
  return 0;
}
