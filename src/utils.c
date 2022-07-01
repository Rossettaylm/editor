#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "termios.h"
#include "unistd.h"

struct termios origin_termios;

void die(const char *s) {
  perror(s);  // 通过查看全部errno变量，打印错误信息
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &origin_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  // tcgetattr: 将terminal(zsh)属性读入到origin_termios中
  if (tcgetattr(STDIN_FILENO, &origin_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);  // 主程序退出时执行的动作，传递一个函数指针

  struct termios raw =
      origin_termios;  // 创建一个termios结构体raw，并赋值origin的状态

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
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);  // local flags

  // 增加timeout, VMIN设置在read()函数能返回之前所需要的最小字节数
  // VTIME设置在read()函数返回前的最大等待时间, 单位为1/10秒
  // 下列表示，不需要输入字节且等待1秒后read函数直接返回
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 10;

  // TCSAFLUSH表示等待所有的pending output
  // 被写到terminal后，应用对raw做的改变，同时遗弃未被读入的输入
  // tcsetattr: 对terminal应用修改后的raw属性
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}
