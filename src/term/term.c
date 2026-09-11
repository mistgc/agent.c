#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "term.h"

static struct termios _saved_termios;

static void restore_terminal(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &_saved_termios);
}

void term_enable_utf8(void) {
  if (!isatty(STDIN_FILENO))
    return;
  struct termios t;
  if (tcgetattr(STDIN_FILENO, &t) != 0)
    return;
  if (t.c_iflag & IUTF8)
    return; /* 已开启, 无需改动 */
  _saved_termios = t;
  t.c_iflag |= IUTF8;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0)
    atexit(restore_terminal);
}

const char *term_color(const char *code) {
  static int tty = -1;
  if (tty < 0)
    tty = isatty(STDOUT_FILENO);
  return tty ? code : "";
}
