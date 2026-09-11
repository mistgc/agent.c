#include <stdio.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "../src/call_llm.h"
#include "../src/config.h"
#include "../src/dotenv.h"

/*
 * 最小流式示例: 发一条 user 消息, 逐字打印 content 增量, 最后打印结束原因。
 * 不带 tools —— 工具调用的完整用法见 src/main.c。
 */

static void on_delta(const char *text, size_t len) {
  fwrite(text, 1, len, stdout);
  fflush(stdout);
}

int main(void) {
  if (load_dotenv(".env") != 0) {
    fprintf(stderr, "error: could not load .env\n");
    return 1;
  }

  if (config_init() != 0) {
    fprintf(stderr, "error: could not init config\n");
    return 1;
  }

  cJSON *messages = cJSON_CreateArray();
  cJSON *user = cJSON_CreateObject();
  cJSON_AddStringToObject(user, "role", "user");
  cJSON_AddStringToObject(user, "content", "Hello, who are you?");
  cJSON_AddItemToArray(messages, user);

  chat_result_t r = {0};
  int rc = chat_complete_stream(messages, NULL, on_delta, &r);
  if (rc != CURLE_OK) {
    fprintf(stderr, "\nerror: stream failed: %s\n", curl_easy_strerror(rc));
    cJSON_Delete(messages);
    config_free();
    return 1;
  }

  cJSON_Delete(messages);
  config_free();

  fputc('\n', stdout);
  fprintf(stderr, "finish_reason=%s\n",
          r.finish_reason[0] ? r.finish_reason : "(none)");
  chat_result_free(&r);
  return 0;
}
