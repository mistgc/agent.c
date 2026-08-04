#ifndef CONFIG_H
#define CONFIG_H

typedef struct config {
  char *api_key;
  char *base_url;
  char *model_id;
} config_t;

int config_init();
void config_free();

config_t *config();

#endif /* CONFIG_H */
