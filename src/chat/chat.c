#include "chat.h"

void chat_append_user(cJSON *messages, const char *content) {
  cJSON *user = cJSON_CreateObject();
  cJSON_AddStringToObject(user, "role", "user");
  cJSON_AddStringToObject(user, "content", content);
  cJSON_AddItemToArray(messages, user);
}

void chat_append_assistant(cJSON *messages, const chat_result_t *r) {
  cJSON *asst = cJSON_CreateObject();
  cJSON_AddStringToObject(asst, "role", "assistant");

  if (r->n_tool_calls == 0) {
    cJSON_AddStringToObject(asst, "content", r->content ? r->content : "");
    cJSON_AddItemToArray(messages, asst);
    return;
  }

  if (r->content)
    cJSON_AddStringToObject(asst, "content", r->content);
  else
    cJSON_AddNullToObject(asst, "content");

  cJSON *calls = cJSON_AddArrayToObject(asst, "tool_calls");
  for (int i = 0; i < r->n_tool_calls; i++) {
    cJSON *call = cJSON_CreateObject();
    cJSON_AddStringToObject(call, "id", r->tool_calls[i].id);
    cJSON_AddStringToObject(call, "type", "function");
    cJSON *fn = cJSON_CreateObject();
    cJSON_AddStringToObject(fn, "name", r->tool_calls[i].name);
    cJSON_AddStringToObject(fn, "arguments", r->tool_calls[i].arguments);
    cJSON_AddItemToObject(call, "function", fn);
    cJSON_AddItemToArray(calls, call);
  }
  cJSON_AddItemToArray(messages, asst);
}

void chat_append_tool_result(cJSON *messages, const char *tool_call_id,
                             const char *content) {
  cJSON *tm = cJSON_CreateObject();
  cJSON_AddStringToObject(tm, "role", "tool");
  cJSON_AddStringToObject(tm, "tool_call_id", tool_call_id);
  cJSON_AddStringToObject(tm, "content", content);
  cJSON_AddItemToArray(messages, tm);
}
