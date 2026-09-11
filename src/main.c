#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "call_llm.h"
#include "config.h"
#include "dotenv.h"
#include "tools/tool_base.h"

#define MAX_TURNS 20

/* Assistant 标签在首个 delta 到来时惰性打印, 避免空气泡 */
static int _assistant_started = 0;

static void print_delta(const char *text, size_t len) {
  if (len == 0)
    return;
  if (!_assistant_started) {
    _assistant_started = 1;
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
      fprintf(stderr, "\nerror: stream failed: %s\n", curl_easy_strerror(rc));
      chat_result_free(&r);
      return;
    }
    if (r.content && r.content[0]) {
      fputc('\n', stdout);
      fflush(stdout);
    }

    if (r.n_tool_calls == 0) {
      cJSON *asst = cJSON_CreateObject();
      cJSON_AddStringToObject(asst, "role", "assistant");
      cJSON_AddStringToObject(asst, "content", r.content ? r.content : "");
      cJSON_AddItemToArray(messages, asst);
      chat_result_free(&r);
      return;
    }
    if (++turns >= MAX_TURNS) {
      chat_result_free(&r);
      return;
    }

    cJSON *asst = cJSON_CreateObject();
    cJSON_AddStringToObject(asst, "role", "assistant");
    if (r.content)
      cJSON_AddStringToObject(asst, "content", r.content);
    else
      cJSON_AddNullToObject(asst, "content");
    cJSON *calls = cJSON_AddArrayToObject(asst, "tool_calls");
    for (int i = 0; i < r.n_tool_calls; i++) {
      cJSON *call = cJSON_CreateObject();
      cJSON_AddStringToObject(call, "id", r.tool_calls[i].id);
      cJSON_AddStringToObject(call, "type", "function");
      cJSON *fn = cJSON_CreateObject();
      cJSON_AddStringToObject(fn, "name", r.tool_calls[i].name);
      cJSON_AddStringToObject(fn, "arguments", r.tool_calls[i].arguments);
      cJSON_AddItemToObject(call, "function", fn);
      cJSON_AddItemToArray(calls, call);
    }
    cJSON_AddItemToArray(messages, asst);

    for (int i = 0; i < r.n_tool_calls; i++) {
      fprintf(stderr, "Tool: %s(%s)\n", r.tool_calls[i].name,
              r.tool_calls[i].arguments);
      char *result = tool_invoke(r.tool_calls[i].name, r.tool_calls[i].arguments);
      cJSON *tm = cJSON_CreateObject();
      cJSON_AddStringToObject(tm, "role", "tool");
      cJSON_AddStringToObject(tm, "tool_call_id", r.tool_calls[i].id);
      cJSON_AddStringToObject(tm, "content", result);
      cJSON_AddItemToArray(messages, tm);
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
    fputs(cont ? "... " : "User: ", stdout);
    fflush(stdout);
    if (!fgets(line, sizeof line, stdin)) {
      if (!buf)
        return NULL; /* EOF 且无任何输入 */
      break;
    }
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

int main(int argc, char **argv) {
  if (load_dotenv(".env") != 0) {
    fprintf(stderr, "error: could not load .env\n");
    return 1;
  }

  if (config_init() != 0) {
    fprintf(stderr, "error: could not init config\n");
    return 1;
  }

  cJSON *messages = cJSON_CreateArray();
  cJSON *tools = tools_schema();

  /* 命令行首条 prompt (可选): ./agent "question" */
  const char *initial = argc > 1 ? argv[1] : NULL;

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

    cJSON *user = cJSON_CreateObject();
    cJSON_AddStringToObject(user, "role", "user");
    cJSON_AddStringToObject(user, "content", input);
    cJSON_AddItemToArray(messages, user);
    free(input);

    run_agent(messages, tools);
  }

  cJSON_Delete(tools);
  cJSON_Delete(messages);
  config_free();
  return 0;
}
