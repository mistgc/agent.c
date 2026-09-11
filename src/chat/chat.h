#ifndef CHAT_H
#define CHAT_H

#include <cjson/cJSON.h>

#include "../call_llm.h"

/* 追加一条 user 消息 */
void chat_append_user(cJSON *messages, const char *content);

/*
 * 把一次助手回复追加为 assistant 消息。
 * 无工具调用: content 用空串。
 * 有工具调用: content 可为 null, 并附 tool_calls 数组。
 */
void chat_append_assistant(cJSON *messages, const chat_result_t *r);

/* 追加一条 tool 角色消息, 回填某次 tool_call 的执行结果 */
void chat_append_tool_result(cJSON *messages, const char *tool_call_id,
                             const char *content);

#endif /* CHAT_H */
