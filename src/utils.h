#ifndef __UTILS_H__
#define __UTILS_H__

/*** defines ***/
// 'q' = 1110001
//  ctrl_q = 10001
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#define CTRL_KEY(k) ((k)&0x1f) // 取变量k的低5位
#define KILO_VERSION "0.0.1"

/************ data **********************/
struct editorConfig {
  int cx, cy; // 表示光标当前位置
  int screenRows;
  int screenCols;
  struct termios origin_termios;
};

/*** append buffer ***/
// 定义一个缓冲区数据结构，用来代替重复的write()
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}; // 用来定义一个空的缓冲区

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
};

/************ terminal *******************/

/**
 * @brief Turn off echoing, Turn off canonical mode, Turn off ctrl-c and ctrl-z
 * signals
 * 1. 使标准输入的字符不显示在屏幕上，类似于输入密码
 * 2. 关闭经典输入模式，使按字节读入数据而不是按行读取
 * 3. 关闭对ctrl-c和ctrl-z信号的响应 interrupt, suspend
 * 4. 关闭对ctrl-s和ctrl-q的响应 pause / resume transmission
 * 5. 关闭对ctrl-v信号的响应
 * 6. fix ctrl-m 上述操作时候ctrl-m变成了10，等价于回车键
 * 7. 关闭对输出的处理，防止将'\n'转为'\r\n'
 * 8. 一些不是很显著的标志位
 *    - BRKINT 当开启时，break condition将同时发送一个SIGINT信号
 *    - INPCK 当开启时，允许奇偶校验
 *    - ISTRIP 当开启时，对8位的输入字节进行strip
 *    - CS8 是一个位掩码，通过位或|来设置
 * 9. 加上read()函数的时间限制timeout
 * 10. 错误处理
 */
void enableRawMode();

/**
 * @brief Disable raw mode at exit
 * 当程序退出时，恢复terminal原始的属性
 */
void disableRawMode();

void die(const char *s);

/**
 * @brief low level keypress reading
 * @return char
 */
int editorReadKey();

/**
 * @brief Get the Window Size
 */
int getWindowSize(int *rows, int *cols);

/**
 * @brief Get the Cursor Position
 */
int getCursorPosition(int *rows, int *cols);

/**
 * 向缓冲区写入长度为len的字符串
 * @param ab 缓冲区
 * @param s 待写入的字符串
 * @param len 字符串长度
 */
void abAppend(struct abuf *ab, const char *s, int len);

/**
 * 释放缓冲区分配的内存空间
 * @param ab
 */
void abFree(struct abuf *ab);

/************ input *******************/

void editorProcessKeypress();

void editorMoveCursor(int key);

/************ output *******************/

/**
 * @brief refresh the screen with escape sequence
 */
void editorRefreshScreen();

void editorDrawRows();

/************ init *********************/

void initEditor();

#endif // __UTILS_H__