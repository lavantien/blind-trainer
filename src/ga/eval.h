#ifndef BT_GA_EVAL_H
#define BT_GA_EVAL_H

#include "ga/ga.h"

typedef struct bt_ga_eval bt_ga_eval;

typedef int (*bt_ga_eval_fault_hook)(const char *site, void *user);

bt_ga_eval *bt_ga_eval_create(const bt_ga_env *env, const bt_ga_params *params, size_t max_count);
void bt_ga_eval_destroy(bt_ga_eval *eval);
void bt_ga_eval_population(bt_ga_eval *eval, uint32_t *const *population, size_t count,
                           float *scores);
int bt_ga_eval_uses_cuda(const bt_ga_eval *eval);
int bt_ga_eval_cuda_compiled(void);
int bt_ga_eval_config_allow(const bt_json_value *config, int *allow, char err[BT_GA_ERR_MAX]);
void bt_ga_eval_fault_install(bt_ga_eval_fault_hook hook, void *user);
void bt_ga_eval_fault_reset(void);

#endif
