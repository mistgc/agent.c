#ifndef CALL_LLM_H
#define CALL_LLM_H

#include <stddef.h>

#include <cjson/cJSON.h>

#define CHAT_MAX_TOOL_CALLS 8

typedef struct {
  char *id;
  char *name;
  char *arguments; /* 完整 JSON 字符串, 由流式分片拼接而成 */
} chat_tool_call_t;

typedef struct {
  char *content; /* malloc'd, 可能为 NULL */
  char finish_reason[32];
  chat_tool_call_t tool_calls[CHAT_MAX_TOOL_CALLS];
  int n_tool_calls;
} chat_result_t;

typedef void (*chat_delta_cb)(const char *text, size_t len);

/*
 * 流式调用 chat completions。
 *   messages: cJSON 数组, 调用方拥有。
 *   tools:    cJSON 数组或 NULL, 调用方拥有。
 *   on_delta: 每收到一段 content 增量就回调 (可为 NULL)。
 *   out:      累积结果, 调用方需先清零; 结束后用 chat_result_free 释放。
 * 返回 CURLE_OK(0) 表示成功。
 */
int chat_complete_stream(const cJSON *messages, const cJSON *tools,
                         chat_delta_cb on_delta, chat_result_t *out);

void chat_result_free(chat_result_t *r);

#endif /* CALL_LLM_H */
