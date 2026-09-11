#ifndef TERM_H
#define TERM_H

/* 终端着色: 非 tty (管道/重定向) 时 term_color 返回空串, 避免转义码污染输出 */
#define TERM_USER "\033[36m"   /* 青: 用户输入 */
#define TERM_ASSIST "\033[32m" /* 绿: Agent 输出 */
#define TERM_TOOL "\033[33m"   /* 黄: 工具执行 */
#define TERM_RESET "\033[0m"

/*
 * 开启 IUTF8: 让内核行规程按 UTF-8 字符 (而非字节) 处理 backspace。
 * 否则中文等 3 字节字符按一次删除键只掉 1 字节, 剩下的乱码像是删不干净。
 * 非 tty 时空操作。
 */
void term_enable_utf8(void);

/* stdout 为 tty 时返回 code, 否则返回空串。tty 检测结果缓存。 */
const char *term_color(const char *code);

#endif /* TERM_H */
