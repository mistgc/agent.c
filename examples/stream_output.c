#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <openai.h>

#include "../src/call_llm.h"
#include "../src/dotenv.h"

/*
 * SSE (Server-Sent Events) 解析器。
 *
 * OpenAI 兼容接口的流式响应是一串事件, 每个事件形如:
 *
 *     data: {"choices":[{"delta":{"content":"Hello"}}]}\n\n
 *
 * 每个事件以空行结尾, 内容在 "data: " 之后。curl 回调可能一次收到
 * 半行、多行甚至跨事件的任意字节切片, 所以这里先把原始字节累积到
 * 缓冲区, 按 '\n' 切出完整行处理, 残留在缓冲区头部的半行留到下次。
 */

static void handle_sse_line(const char *line, size_t len) {
    if (len == 0 || line[0] == ':')
        return;

    if (len < 5 || strncmp(line, "data:", 5) != 0)
        return;
    line += 5;
    len -= 5;
    while (len > 0 && (*line == ' ' || *line == '\t')) {
        line++;
        len--;
    }

    if (len == 6 && memcmp(line, "[DONE]", 6) == 0)
        return;

    cJSON *root = cJSON_ParseWithLength(line, len);
    if (!root)
        return;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    cJSON *first = choices ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *delta = first ? cJSON_GetObjectItem(first, "delta") : NULL;
    cJSON *content = delta ? cJSON_GetObjectItem(delta, "content") : NULL;

    if (cJSON_IsString(content) && content->valuestring && *content->valuestring) {
        fputs(content->valuestring, stdout);
        fflush(stdout); /* 立即刷出, 实现真正的逐字流式显示 */
    }

    cJSON_Delete(root);
}

int main() {
    if (load_dotenv(".env") != 0) {
        fprintf(stderr, "error: could not load .env\n");
        return 1;
    }

    const char *key = getenv("OPENAI_API_KEY");
    const char *url = getenv("OPENAI_BASE_URL");
    const char *model = getenv("OPENAI_MODEL_ID");
    const char *prompt = "Hello, who are you?";

    if (!key || !*key) {
        fprintf(stderr, "error: OPENAI_API_KEY is not set\n");
        return 1;
    }

    openai_init(key, url);

    int rc = chat_complete_stream(handle_sse_line, prompt, model);
    if (rc != CURLE_OK) {
        fprintf(stderr, "\nerror: stream failed: %s\n", curl_easy_strerror(rc));
        return 1;
    }

    fputc('\n', stdout);
    return 0;
}
