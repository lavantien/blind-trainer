#include "ga/eval.h"

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "common/config.h"
#include "data/paths.h"
#include "ga/alloc.h"

#include <stdlib.h>
#include <string.h>

#ifdef BT_GA_EVAL_CUDA
#include "ga/cuda/ga_eval.h"
#endif

static bt_ga_eval_fault_hook bt_ga_eval_fault_active;
static void *bt_ga_eval_fault_user;

struct bt_ga_eval {
    bt_ga_params params;
    const bt_ga_env *env;
    size_t capacity;
#ifdef BT_GA_EVAL_CUDA
    bt_ga_cuda_eval *cuda;
    uint32_t *pack;
#endif
};

void bt_ga_eval_fault_install(bt_ga_eval_fault_hook hook, void *user) {
    BT_TRACE_ENTER("ga.eval_fault_install");
    bt_ga_eval_fault_active = hook;
    bt_ga_eval_fault_user = user;
    BT_TRACE_EXIT("ga.eval_fault_install");
}

void bt_ga_eval_fault_reset(void) {
    BT_TRACE_ENTER("ga.eval_fault_reset");
    bt_ga_eval_fault_install(NULL, NULL);
    BT_TRACE_EXIT("ga.eval_fault_reset");
}

static int bt_ga_eval_fault_hit(const char *site) {
    if (bt_ga_eval_fault_active == NULL) {
        return 0;
    }
    return bt_ga_eval_fault_active(site, bt_ga_eval_fault_user);
}

int bt_ga_eval_config_allow(const bt_json_value *config, int *allow, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.eval_config_allow");
    *allow = 0;
    const bt_json_value *value = bt_config_value(config, "cuda.enable");
    if (value == NULL) {
        BT_TRACE_EXIT("ga.eval_config_allow");
        return 1;
    }
    if (value->type != BT_JSON_BOOL) {
        snprintf(err, BT_GA_ERR_MAX, "config value has the wrong type: cuda.enable");
        BT_TRACE_FAIL("ga.eval_config_allow", "cuda.enable is not a boolean");
        return 0;
    }
    *allow = value->as.boolean ? 1 : 0;
    BT_TRACE_EXIT("ga.eval_config_allow");
    return 1;
}

#ifdef BT_GA_EVAL_CUDA
static int bt_ga_eval_cuda_allowed(void) {
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        BT_TRACE_FAIL("ga.eval.create", "config unreadable, cpu fallback");
        return 0;
    }
    int allow = 0;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_eval_config_allow(config, &allow, err)) {
        BT_TRACE_FAIL("ga.eval.create", "%s, cpu fallback", err);
        allow = 0;
    }
    bt_json_free(config);
    return allow;
}

static void bt_ga_eval_desc_of(const bt_ga_env *env, const bt_ga_params *params, uint32_t *pair_i,
                               uint32_t *pair_j, float *pair_w, bt_ga_cuda_desc *desc) {
    desc->conf_table = env->conf_table;
    desc->flow_table = env->flow_table;
    desc->lengths = env->lengths;
    desc->pool_size = env->pool_size;
    desc->cooc_i = pair_i;
    desc->cooc_j = pair_j;
    desc->cooc_w = pair_w;
    desc->cooc_count = env->cooc_count;
    desc->adj_i = pair_i + env->cooc_count;
    desc->adj_j = pair_j + env->cooc_count;
    desc->adj_w = pair_w + env->cooc_count;
    desc->adj_count = env->adj_count;
    desc->case_count = env->case_count;
    desc->w_conf = params->w_conf;
    desc->w_flow = params->w_flow;
    desc->w_cplx = params->w_cplx;
}
#endif

bt_ga_eval *bt_ga_eval_create(const bt_ga_env *env, const bt_ga_params *params, size_t max_count) {
    BT_TRACE_ENTER("ga.eval.create");
    bt_ga_eval *eval = bt_ga_alloc(sizeof *eval, "ga.eval.session");
    if (eval == NULL) {
        BT_TRACE_FAIL("ga.eval.create", "allocation failed at ga.eval.session");
        bt_harness_fail("src/ga/eval.c", 0, "abort: ga eval: allocation failed at ga.eval.session");
        bt_harness_exit_failure();
    }
    eval->params = *params;
    eval->env = env;
    eval->capacity = max_count;
    int seam_block = bt_ga_eval_fault_hit("ga.eval.cuda");
    if (seam_block) {
        BT_TRACE_FAIL("ga.eval.create", "fault seam armed at ga.eval.cuda, cpu fallback");
    }
#ifdef BT_GA_EVAL_CUDA
    eval->cuda = NULL;
    eval->pack = NULL;
    int allowed = bt_ga_eval_cuda_allowed();
    if (allowed && !seam_block) {
        size_t pair_count = env->cooc_count + env->adj_count;
        uint32_t *pair_i = NULL;
        uint32_t *pair_j = NULL;
        float *pair_w = NULL;
        if (pair_count != 0) {
            pair_i = bt_ga_alloc(pair_count * sizeof *pair_i, "ga.eval.pairs");
            pair_j = bt_ga_alloc(pair_count * sizeof *pair_j, "ga.eval.pairs");
            pair_w = bt_ga_alloc(pair_count * sizeof *pair_w, "ga.eval.pairs");
        }
        if (pair_count != 0 && (pair_i == NULL || pair_j == NULL || pair_w == NULL)) {
            BT_TRACE_FAIL("ga.eval.create", "allocation failed at ga.eval.pairs, cpu fallback");
            bt_ga_free(pair_i);
            bt_ga_free(pair_j);
            bt_ga_free(pair_w);
        } else {
            for (size_t k = 0; k < env->cooc_count; k++) {
                pair_i[k] = env->cooc[k].i;
                pair_j[k] = env->cooc[k].j;
                pair_w[k] = env->cooc[k].w;
            }
            for (size_t k = 0; k < env->adj_count; k++) {
                size_t at = env->cooc_count + k;
                pair_i[at] = env->adj[k].i;
                pair_j[at] = env->adj[k].j;
                pair_w[at] = env->adj[k].w;
            }
            bt_ga_cuda_desc desc;
            bt_ga_eval_desc_of(env, params, pair_i, pair_j, pair_w, &desc);
            char err[BT_GA_CUDA_ERR_MAX];
            eval->cuda = bt_ga_cuda_eval_create(&desc, max_count, err);
            if (eval->cuda == NULL) {
                BT_TRACE_FAIL("ga.eval.create", "cuda context failed (%s), cpu fallback", err);
            }
            bt_ga_free(pair_i);
            bt_ga_free(pair_j);
            bt_ga_free(pair_w);
        }
    }
    if (eval->cuda != NULL && max_count != 0) {
        if (env->case_count != 0 && max_count > SIZE_MAX / env->case_count) {
            BT_TRACE_FAIL("ga.eval.create", "allocation size overflow at ga.eval.pack");
            bt_harness_fail("src/ga/eval.c", 0,
                            "abort: ga eval: allocation size overflow at ga.eval.pack");
            bt_harness_exit_failure();
        }
        eval->pack = bt_ga_alloc(max_count * env->case_count * sizeof *eval->pack, "ga.eval.pack");
        if (eval->pack == NULL) {
            BT_TRACE_FAIL("ga.eval.create", "allocation failed at ga.eval.pack");
            bt_harness_fail("src/ga/eval.c", 0,
                            "abort: ga eval: allocation failed at ga.eval.pack");
            bt_harness_exit_failure();
        }
    }
#endif
    BT_TRACE_EXIT("ga.eval.create");
    return eval;
}

void bt_ga_eval_destroy(bt_ga_eval *eval) {
    BT_TRACE_ENTER("ga.eval.destroy");
    if (eval == NULL) {
        BT_TRACE_EXIT("ga.eval.destroy");
        return;
    }
#ifdef BT_GA_EVAL_CUDA
    bt_ga_cuda_eval_destroy(eval->cuda);
    bt_ga_free(eval->pack);
#endif
    bt_ga_free(eval);
    BT_TRACE_EXIT("ga.eval.destroy");
}

void bt_ga_eval_population(bt_ga_eval *eval, uint32_t *const *population, size_t count,
                           float *scores) {
    BT_TRACE_ENTER("ga.eval.population");
    if (count > eval->capacity) {
        BT_TRACE_FAIL("ga.eval.population", "count %zu exceeds the eval capacity %zu", count,
                      eval->capacity);
        bt_harness_fail("src/ga/eval.c", 0,
                        "abort: ga eval: count %zu exceeds the eval capacity %zu", count,
                        eval->capacity);
        bt_harness_exit_failure();
    }
#ifdef BT_GA_EVAL_CUDA
    if (eval->cuda != NULL) {
        size_t case_count = eval->env->case_count;
        for (size_t i = 0; i < count; i++) {
            memcpy(eval->pack + i * case_count, population[i], case_count * sizeof *eval->pack);
        }
        char err[BT_GA_CUDA_ERR_MAX];
        if (bt_ga_cuda_eval_population(eval->cuda, eval->pack, count, scores, err) == 0) {
            BT_TRACE_EXIT("ga.eval.population");
            return;
        }
        BT_TRACE_FAIL("ga.eval.population", "cuda evaluation failed (%s)", err);
        bt_harness_fail("src/ga/eval.c", 0, "abort: ga eval: cuda evaluation failed (%s)", err);
        bt_harness_exit_failure();
    }
#endif
    for (size_t i = 0; i < count; i++) {
        scores[i] = bt_ga_fitness(&eval->params, eval->env, population[i]);
    }
    BT_TRACE_EXIT("ga.eval.population");
}

int bt_ga_eval_uses_cuda(const bt_ga_eval *eval) {
#ifdef BT_GA_EVAL_CUDA
    return eval != NULL && eval->cuda != NULL;
#else
    (void)eval;
    return 0;
#endif
}

int bt_ga_eval_cuda_compiled(void) {
#ifdef BT_GA_EVAL_CUDA
    return 1;
#else
    return 0;
#endif
}
