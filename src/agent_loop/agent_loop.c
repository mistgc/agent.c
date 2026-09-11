#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "../call_llm.h"
#include "../chat/chat.h"
#include "../term/term.h"
#include "../tools/tool_base.h"

#define MAX_TURNS 20

/* Assistant 标签在首个 delta 到来时惰性打印, 避免空气泡 */
static int _assistant_started = 0;

static void print_delta(const char *text, size_t len) {
  if (len == 0)
    return;
  if (!_assistant_started) {
    _assistant_started = 1;
    fputs(term_color(TERM_ASSIST), stdout);
    fputs("Assistant: ", stdout);
  }
  fwrite(text, 1, len, stdout);
  fflush(stdout);
}

/* 跑一次用户回合的 agent loop: 模型请求工具就执行回填, 直到给出最终回答 */
static void run_agent(cJSON *messages, cJSON *tools) {
  int turns = 0;
  while (1) {
    chat_result_t r = {0};
    _assistant_started = 0;
    int rc = chat_complete_stream(messages, tools, print_delta, &r);
    if (rc != CURLE_OK) {
      fprintf(stderr, "%s\nerror: stream failed: %s\n", term_color(TERM_RESET),
              curl_easy_strerror(rc));
      chat_result_free(&r);
      return;
    }
    if (r.content && r.content[0]) {
      fputc('\n', stdout);
      fputs(term_color(TERM_RESET), stdout);
      fflush(stdout);
    }

    if (r.n_tool_calls == 0) {
      chat_append_assistant(messages, &r);
      chat_result_free(&r);
      return;
    }
    if (++turns >= MAX_TURNS) {
      chat_result_free(&r);
      return;
    }

    chat_append_assistant(messages, &r);

    for (int i = 0; i < r.n_tool_calls; i++) {
      fprintf(stderr, "%sTool: %s(%s)%s\n", term_color(TERM_TOOL),
              r.tool_calls[i].name, r.tool_calls[i].arguments,
              term_color(TERM_RESET));
      char *result = tool_invoke(r.tool_calls[i].name, r.tool_calls[i].arguments);
      chat_append_tool_result(messages, r.tool_calls[i].id, result);
      free(result);
    }

    chat_result_free(&r);
  }
}

/* 读取一条用户输入 (可多行): 行尾反斜杠 \ 表示续行, 否则提交。
 * 返回 malloc 的字符串 (已去续行符); 无输入即 EOF 时返回 NULL。 */
static char *read_input(void) {
  char *buf = NULL;
  size_t len = 0;
  char line[4096];
  int cont = 0;
  for (;;) {
    fputs(term_color(TERM_USER), stdout);
    fputs(cont ? "... " : "User: ", stdout);
    fflush(stdout);
    if (!fgets(line, sizeof line, stdin)) {
      fputs(term_color(TERM_RESET), stdout);
      fflush(stdout);
      if (!buf)
        return NULL; /* EOF 且无任何输入 */
      break;
    }
    fputs(term_color(TERM_RESET), stdout);
    fflush(stdout);
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = '\0';

    cont = 0;
    if (n > 0 && line[n - 1] == '\\') {
      line[--n] = '\0';
      cont = 1;
    }

    char *np = realloc(buf, len + n + 2);
    if (!np)
      break;
    buf = np;
    memcpy(buf + len, line, n);
    len += n;
    buf[len++] = '\n';
    buf[len] = '\0';

    if (!cont)
      break;
  }
  if (buf && len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = '\0';
  return buf;
}

void agent_loop_run(const char *initial) {
  term_enable_utf8();

  cJSON *messages = cJSON_CreateArray();
  cJSON *tools = tools_schema();

  while (1) {
    char *input = initial ? strdup(initial) : read_input();
    initial = NULL;
    if (!input)
      break; /* EOF (Ctrl-D) */

    if (input[0] == '\0') {
      free(input);
      continue;
    }
    if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
      free(input);
      break;
    }

    chat_append_user(messages, input);
    free(input);

    run_agent(messages, tools);
  }

  cJSON_Delete(tools);
  cJSON_Delete(messages);
}
