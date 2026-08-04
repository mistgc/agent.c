#ifndef CALL_LLM_H
#define CALL_LLM_H

#include <stddef.h>

typedef void chat_complete_stream_sse_callback(const char* line, size_t len);

int chat_complete_stream(chat_complete_stream_sse_callback cb, const char *prompt,
                         const char *model_id);

#endif /* CALL_LLM_H */
