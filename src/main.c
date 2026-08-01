#include <stdio.h>
#include <stdlib.h>

#include <openai.h>

#include "call_llm.h"
#include "dotenv.h"


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

    char* msg = NULL;

    openai_init(key, url);
    int n = chat_complete(&msg, prompt, model);

    if (n < 0 || msg == NULL) {
        fprintf(stderr, "error: chat completion failed (model=%s)\n",
                model ? model : "(unset)");
        openai_cleanup();
        return 1;
    }

    printf("Response: %s\n", msg);
    free(msg);

    openai_cleanup();

    return 0;
}
