#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
