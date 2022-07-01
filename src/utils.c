#include "utils.h"

#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <termios.h>
#include <unistd.h>

/*** datas ***/
struct editorConfig {
  int screenRows;
  int screenCols;
  struct termios origin_termios;
};

struct editorConfig E;

/*** init ***/
void initEditor() {
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

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  return c;
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
  char c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // reposition cursor to top
    exit(0);
    break;
  }
}

/*** output ***/

/**
 * @brief refresh screen accordding to write a 4 bytes character
 * 第一个字节：\x1b 表示16进制的1b, 十进制的27, 'escape' character
 * 其余三个字节分别是：[2J
 * <esc>[1J 是escape sequence命令，用来执行终端文本格式化命令
 * <esc>[12;40H  表示对光标的重定位，12;40是位置
 */
void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
  write(STDOUT_FILENO, "\x1b[H", 3);  // reposition cursor to top
  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void editorDrawRows() {
  int y;
  // 在每行的开头绘制波浪号
  for (y = 0; y < E.screenRows; ++y) {
    write(STDOUT_FILENO, "~", 1);

    // 最后一行不需要进行回车换行
    if (y < E.screenRows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}