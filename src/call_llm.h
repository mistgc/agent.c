#ifndef CALL_LLM_H
#define CALL_LLM_H

#include <stddef.h>

typedef size_t chat_complete_stream_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

int chat_complete(char **msg, const char *prompt, const char *model_id);
int chat_complete_stream(chat_complete_stream_callback cb, const char *prompt,
                         const char *model_id, void *userdata);

#endif /* CALL_LLM_H */
