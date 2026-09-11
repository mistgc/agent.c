#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "call_llm.h"
#include "config.h"
#include "dotenv.h"
#include "tools/tool_base.h"

static void print_delta(const char *text, size_t len) {
  fwrite(text, 1, len, stdout);
  fflush(stdout);
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

  const char *prompt =
      argc > 1 ? argv[1] : "What is the latest news about OpenAI?";

  cJSON *messages = cJSON_CreateArray();
  cJSON *user = cJSON_CreateObject();
  cJSON_AddStringToObject(user, "role", "user");
  cJSON_AddStringToObject(user, "content", prompt);
  cJSON_AddItemToArray(messages, user);

  cJSON *tools = tools_schema();

  for (int turn = 0; turn < 5; turn++) {
    chat_result_t r = {0};
    int rc = chat_complete_stream(messages, tools, print_delta, &r);
    if (rc != CURLE_OK) {
      fprintf(stderr, "\nerror: stream failed: %s\n", curl_easy_strerror(rc));
      chat_result_free(&r);
      break;
    }
    if (r.content)
      fputc('\n', stdout);

    if (r.n_tool_calls == 0) {
      chat_result_free(&r);
      break;
    }

    /* 回填 assistant 的 tool_calls, 再逐个执行并追加 tool 结果 */
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
      fprintf(stderr, "[tool] %s(%s)\n", r.tool_calls[i].name,
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

  cJSON_Delete(tools);
  cJSON_Delete(messages);
  config_free();
  return 0;
}
