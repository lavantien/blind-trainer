#ifndef BT_GA_CUDA_GA_EVAL_H
#define BT_GA_CUDA_GA_EVAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BT_GA_CUDA_ERR_MAX 160

typedef struct {
    const float *conf_table;
    const float *flow_table;
    const float *lengths;
    size_t pool_size;
    const uint32_t *cooc_i;
    const uint32_t *cooc_j;
    const float *cooc_w;
    size_t cooc_count;
    const uint32_t *adj_i;
    const uint32_t *adj_j;
    const float *adj_w;
    size_t adj_count;
    size_t case_count;
    float w_conf;
    float w_flow;
    float w_cplx;
} bt_ga_cuda_desc;

typedef struct bt_ga_cuda_eval bt_ga_cuda_eval;

bt_ga_cuda_eval *bt_ga_cuda_eval_create(const bt_ga_cuda_desc *desc, size_t max_count,
                                        char err[BT_GA_CUDA_ERR_MAX]);
void bt_ga_cuda_eval_destroy(bt_ga_cuda_eval *eval);
int bt_ga_cuda_eval_population(bt_ga_cuda_eval *eval, const uint32_t *flat, size_t count,
                               float *scores, char err[BT_GA_CUDA_ERR_MAX]);

#ifdef __cplusplus
}
#endif

#endif
