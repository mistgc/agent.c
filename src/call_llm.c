#include "call_llm.h"
#include <openai.h>
#include <string.h>

int chat_complete(char **msg, const char *prompt, const char *model_id) {
    *msg = openai_chat_with_model(prompt, model_id);
    return *msg ? (int)strlen(*msg) : -1;
}
