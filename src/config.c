#include <stdio.h>
#include <stdlib.h>

#include "config.h"

static config_t *_config = NULL;

int config_init() {
    if (_config == NULL) {
        _config = malloc(sizeof(config_t));

        _config->api_key = getenv("OPENAI_API_KEY");
        if (_config->api_key == NULL) {
            fprintf(stderr, "error: The OPENAI_API_KEY is empty.");
            return -1;
        }

        _config->base_url = getenv("OPENAI_BASE_URL");
        if (_config->base_url == NULL) {
            _config->base_url = "https://api.deepseek.com/chat/completions";
        }

        _config->model_id = getenv("OPENAI_MODEL_ID");
        if (_config->model_id == NULL) {
            _config->model_id = "deepseek-v4-flash";
        }

        _config->tavily_api_key = getenv("TAVILY_API_KEY");
        return 0;
    } else {
        fprintf(stderr, "error: The config has been inited.");
        return -1;
    }
}

void config_free() {
    if (_config != NULL) {
        free(_config);
        _config = NULL;
    }
}

config_t *config() {
    return _config;
}
