#include "utils.h"

#include <asm-generic/ioctls.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <termios.h>
#include <unistd.h>

/*** datas ***/

struct editorConfig E;

/*** init ***/
void initEditor() {
  // 初始化光标位置
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;

  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
    die("getWindowsSize");
  }
}

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
  write(STDOUT_FILENO, "\x1b[H", 3);  // reposition cursor to top
  perror(s); // 通过查看全局errno变量，打印错误信息
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.origin_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  // tcgetattr: 将terminal(zsh)属性读入到E.origin_termios中
  if (tcgetattr(STDIN_FILENO, &E.origin_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode); // 主程序退出时执行的动作，传递一个函数指针

  struct termios raw =
      E.origin_termios; // 创建一个termios结构体raw，并赋值origin的状态

  // Disable ctrl-s and ctrl-q | fix ctrl-m
  // c_iflag: input flags
  // IXON 其中XON表示resume transmission，XOFF表示pause transmission
  // ICRNL 禁止此标志位可以防止操作系统将(13, '\r')转为(10,
  // '\n')，造成ctrl-m键的错误
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);

  // Turn off all output processing
  // c_oflag: output flags
  // 禁止OPOST表示关闭输出的处理，防止"\n"被转换为"\r\n"(需要自己控制打印"\r\n")，因为通常terminal需要"\r\n"来开启新一行
  // OPOST = post-processing of output
  raw.c_oflag &= ~(OPOST);

  raw.c_cflag |= (CS8);

  // ECHO = 00000000000000000000000000001000 (32bit)
  // ~ECHO = 11111111111111111111111111110111
  // 使local flags的第4位置为0，其它保持不变
  // ICANON flag表示关闭canonical mode（经典模式），即用按字节读取替代按行读取
  // ISIG 表示关闭对ctrl-c, ctrl-z如SIGINT的响应，ctrl-c此时只表示3个字节
  // IEXTEN 控制关闭对ctrl-v的响应
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // local flags

  // 增加timeout, VMIN设置在read()函数能返回之前所需要的最小字节数
  // VTIME设置在read()函数返回前的最大等待时间, 单位为1/10秒
  // 下列表示，不需要输入字节且等待0.1秒后read函数直接返回
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // TCSAFLUSH表示等待所有的pending output
  // 被写到terminal后，应用对raw做的改变，同时遗弃未被读入的输入
  // tcsetattr: 对terminal应用修改后的raw属性
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  // 将上下左右的escape sequence映射到定义的editorKey enum结构中
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1':
              return HOME_KEY;
            case '3':
              return DEL_KEY;
            case '4':
              return END_KEY;
            case '5':
              return PAGE_UP; // <esc>[5~
            case '6':
              return PAGE_DOWN; // <esc>[6~
            case '7':
              return HOME_KEY;
            case '8':
              return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  // #include <sys/ioctl.h>
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // 如果ioctl函数查询失败，则通过跳转光标来进行查询窗口大小
    // 其中999C表示移动光标到右边999列, 999B表示向下移动光标999行
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/**
 * @brief Get the Cursor Position object
 * @return int
 * 其中，"\x1b[6n"表示查询光标的状态信息，输出到屏幕上
 */
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';
  // buf = "<esc>[30;100"

  // 判断是否是一个escape sequence
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;

  // 读取buf中的行和列到rows和cols地址中 30;100
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

/*** input ***/

/**
 * @brief mapping keys to editor functions at a much higher level.
 */
void editorProcessKeypress() {
  int c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
      write(STDOUT_FILENO, "\x1b[H", 3);  // reposition cursor to top
      exit(0);
      break;

    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      E.cx = E.screenCols - 1;
      break;

    case PAGE_UP:
    case PAGE_DOWN: {
      int times = E.screenRows;
      while (times--) {
        editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
    }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}

/*** output ***/

/**
 * @brief refresh screen
 * 第一个字节：\x1b 表示16进制的1b, 十进制的27, 'escape' character
 * 其余三个字节分别是：[2J
 * <esc>[1J 表示 clear screen
 * <esc>[12;40H  表示对光标的重定位，12;40是位置, [H表示左上角
 */
void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // 刷新缓冲区前立刻显示光标
  abAppend(&ab, "\x1b[H", 3); // move cursor to the left top

  editorDrawRows(&ab);

  // 每次刷新屏幕时，将光标的位置传入escape sequence来控制光标移动
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // 在刷新缓冲区前隐藏光标

  // 最后将缓冲区的字符串写入到标准输出
  write(STDOUT_FILENO, ab.b, ab.len);

  abFree(&ab);
}

void editorDrawRows(struct abuf *ab) {
  int y;
  // 在每行的开头绘制波浪号
  for (y = 0; y < E.screenRows; ++y) {
    int filerow = y + E.rowoff;
    // 还没有文本内容的区域
    if (filerow >= E.numrows) {
      // 如果是空白文本，在屏幕的三分之一高度处, 插入KILO_VERSION
      if (E.numrows == 0 && y == E.screenRows / 3) {
        char welcome[80];
        int welcomeLen = snprintf(welcome, sizeof(welcome),
                                  "kilo editor -- version %s", KILO_VERSION);
        // 防止越界
        if (welcomeLen > E.screenCols) {
          welcomeLen = E.screenCols;
        }
        int padding = (E.screenCols - welcomeLen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomeLen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      // 将已有的文本内容绘制到屏幕上
      int len = E.row[filerow].size - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screenCols) len = E.screenCols;
      abAppend(ab, &E.row[filerow].chars[E.coloff], len);
    }

    abAppend(ab, "\x1b[K", 3); // clear line when we redraw
    // 最后一行不需要进行回车换行
    if (y < E.screenRows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void abAppend(struct abuf *ab, const char *s, int len) {
  // 在原来abuf的地址上多分配len长度的空间
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

void editorMoveCursor(int key) {
  // 获取光标所在的当前行
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) { // 行首时向上移动一行，到上一行的末尾
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      // 最右只能移动到文本最右侧的字符
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) { // 行尾时向下移动一行，到下一行的开头
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0)
        E.cy--;
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows)
        E.cy++;
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

/*** file I/O ***/
void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--; // 得到一行文本的长度（不包括换行符）
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

void editorAppendRow(char *s, size_t len) {
  // 其中E.row是一个指针数组，数组中每个元素指向了一行文本数据的结构体erow
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/**
 * 更新E.rowoff和E.coloff的大小
 */
void editorScroll() {
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenRows) {
    E.rowoff = E.cy - E.screenRows + 1;
  }
  if (E.cx < E.coloff) {
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screenCols) {
    E.coloff = E.cx - E.screenCols + 1;
  }
}


