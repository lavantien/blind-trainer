#include "ga/cuda/ga_eval.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BT_GA_CUDA_BLOCK 256

__global__ void bt_ga_cuda_fitness_kernel(bt_ga_cuda_desc d, const uint32_t *chroms, size_t count,
                                          float *scores) {
    size_t i = (size_t)blockIdx.x * (size_t)blockDim.x + (size_t)threadIdx.x;
    if (i >= count) {
        return;
    }
    const uint32_t *chrom = chroms + i * d.case_count;
    float conf = 0.0f;
    for (size_t k = 0; k < d.cooc_count; k++) {
        conf += d.cooc_w[k] *
                d.conf_table[(size_t)chrom[d.cooc_i[k]] * d.pool_size + chrom[d.cooc_j[k]]];
    }
    float flow = 0.0f;
    for (size_t k = 0; k < d.adj_count; k++) {
        flow +=
            d.adj_w[k] * d.flow_table[(size_t)chrom[d.adj_i[k]] * d.pool_size + chrom[d.adj_j[k]]];
    }
    float cplx = 0.0f;
    for (size_t g = 0; g < d.case_count; g++) {
        cplx += d.lengths[chrom[g]];
    }
    cplx /= (float)d.case_count;
    scores[i] = d.w_conf * conf + d.w_flow * flow + d.w_cplx * cplx;
}

struct bt_ga_cuda_eval {
    bt_ga_cuda_desc device;
    uint32_t *chroms;
    float *scores;
    size_t max_count;
};

static void bt_ga_cuda_note(char err[BT_GA_CUDA_ERR_MAX], const char *site, const char *what) {
    snprintf(err, BT_GA_CUDA_ERR_MAX, "%s: %s", site, what);
}

static void *bt_ga_cuda_malloc(size_t bytes, const char *site, char err[BT_GA_CUDA_ERR_MAX]) {
    void *block = NULL;
    cudaError_t code = cudaMalloc(&block, bytes);
    if (code != cudaSuccess) {
        bt_ga_cuda_note(err, site, cudaGetErrorString(code));
        return NULL;
    }
    return block;
}

static int bt_ga_cuda_copy_in(void *dst, const void *src, size_t bytes, const char *site,
                              char err[BT_GA_CUDA_ERR_MAX]) {
    cudaError_t code = cudaMemcpy(dst, src, bytes, cudaMemcpyHostToDevice);
    if (code != cudaSuccess) {
        bt_ga_cuda_note(err, site, cudaGetErrorString(code));
        return -1;
    }
    return 0;
}

bt_ga_cuda_eval *bt_ga_cuda_eval_create(const bt_ga_cuda_desc *desc, size_t max_count,
                                        char err[BT_GA_CUDA_ERR_MAX]) {
    err[0] = '\0';
    bt_ga_cuda_eval *eval = (bt_ga_cuda_eval *)calloc(1, sizeof *eval);
    if (eval == NULL) {
        bt_ga_cuda_note(err, "cuda.eval", "host allocation failed");
        return NULL;
    }
    eval->max_count = max_count;
    eval->device = *desc;
    size_t pool_bytes = desc->pool_size * desc->pool_size * sizeof(float);
    eval->device.conf_table = (const float *)bt_ga_cuda_malloc(pool_bytes, "cuda.conf_table", err);
    if (eval->device.conf_table == NULL) {
        free(eval);
        return NULL;
    }
    eval->device.flow_table = (const float *)bt_ga_cuda_malloc(pool_bytes, "cuda.flow_table", err);
    if (eval->device.flow_table == NULL) {
        cudaFree((void *)eval->device.conf_table);
        free(eval);
        return NULL;
    }
    size_t length_bytes = desc->pool_size * sizeof(float);
    eval->device.lengths = (const float *)bt_ga_cuda_malloc(length_bytes, "cuda.lengths", err);
    if (eval->device.lengths == NULL) {
        cudaFree((void *)eval->device.flow_table);
        cudaFree((void *)eval->device.conf_table);
        free(eval);
        return NULL;
    }
    if (bt_ga_cuda_copy_in((void *)eval->device.conf_table, desc->conf_table, pool_bytes,
                           "cuda.conf_table", err) != 0 ||
        bt_ga_cuda_copy_in((void *)eval->device.flow_table, desc->flow_table, pool_bytes,
                           "cuda.flow_table", err) != 0 ||
        bt_ga_cuda_copy_in((void *)eval->device.lengths, desc->lengths, length_bytes,
                           "cuda.lengths", err) != 0) {
        bt_ga_cuda_eval_destroy(eval);
        return NULL;
    }
    if (desc->cooc_count > 0) {
        size_t pair_bytes = desc->cooc_count * sizeof(uint32_t);
        size_t weight_bytes = desc->cooc_count * sizeof(float);
        eval->device.cooc_i = (const uint32_t *)bt_ga_cuda_malloc(pair_bytes, "cuda.cooc", err);
        eval->device.cooc_j = (const uint32_t *)bt_ga_cuda_malloc(pair_bytes, "cuda.cooc", err);
        eval->device.cooc_w = (const float *)bt_ga_cuda_malloc(weight_bytes, "cuda.cooc", err);
        if (eval->device.cooc_i == NULL || eval->device.cooc_j == NULL ||
            eval->device.cooc_w == NULL ||
            bt_ga_cuda_copy_in((void *)eval->device.cooc_i, desc->cooc_i, pair_bytes, "cuda.cooc",
                               err) != 0 ||
            bt_ga_cuda_copy_in((void *)eval->device.cooc_j, desc->cooc_j, pair_bytes, "cuda.cooc",
                               err) != 0 ||
            bt_ga_cuda_copy_in((void *)eval->device.cooc_w, desc->cooc_w, weight_bytes, "cuda.cooc",
                               err) != 0) {
            bt_ga_cuda_eval_destroy(eval);
            return NULL;
        }
    }
    if (desc->adj_count > 0) {
        size_t pair_bytes = desc->adj_count * sizeof(uint32_t);
        size_t weight_bytes = desc->adj_count * sizeof(float);
        eval->device.adj_i = (const uint32_t *)bt_ga_cuda_malloc(pair_bytes, "cuda.adj", err);
        eval->device.adj_j = (const uint32_t *)bt_ga_cuda_malloc(pair_bytes, "cuda.adj", err);
        eval->device.adj_w = (const float *)bt_ga_cuda_malloc(weight_bytes, "cuda.adj", err);
        if (eval->device.adj_i == NULL || eval->device.adj_j == NULL ||
            eval->device.adj_w == NULL ||
            bt_ga_cuda_copy_in((void *)eval->device.adj_i, desc->adj_i, pair_bytes, "cuda.adj",
                               err) != 0 ||
            bt_ga_cuda_copy_in((void *)eval->device.adj_j, desc->adj_j, pair_bytes, "cuda.adj",
                               err) != 0 ||
            bt_ga_cuda_copy_in((void *)eval->device.adj_w, desc->adj_w, weight_bytes, "cuda.adj",
                               err) != 0) {
            bt_ga_cuda_eval_destroy(eval);
            return NULL;
        }
    }
    if (max_count > 0) {
        size_t chrom_bytes = max_count * desc->case_count * sizeof(uint32_t);
        size_t score_bytes = max_count * sizeof(float);
        eval->chroms = (uint32_t *)bt_ga_cuda_malloc(chrom_bytes, "cuda.chroms", err);
        eval->scores = (float *)bt_ga_cuda_malloc(score_bytes, "cuda.scores", err);
        if (eval->chroms == NULL || eval->scores == NULL) {
            bt_ga_cuda_eval_destroy(eval);
            return NULL;
        }
    }
    return eval;
}

void bt_ga_cuda_eval_destroy(bt_ga_cuda_eval *eval) {
    if (eval == NULL) {
        return;
    }
    cudaFree((void *)eval->device.conf_table);
    cudaFree((void *)eval->device.flow_table);
    cudaFree((void *)eval->device.lengths);
    cudaFree((void *)eval->device.cooc_i);
    cudaFree((void *)eval->device.cooc_j);
    cudaFree((void *)eval->device.cooc_w);
    cudaFree((void *)eval->device.adj_i);
    cudaFree((void *)eval->device.adj_j);
    cudaFree((void *)eval->device.adj_w);
    cudaFree(eval->chroms);
    cudaFree(eval->scores);
    free(eval);
}

int bt_ga_cuda_eval_population(bt_ga_cuda_eval *eval, const uint32_t *flat, size_t count,
                               float *scores, char err[BT_GA_CUDA_ERR_MAX]) {
    err[0] = '\0';
    if (count == 0) {
        return 0;
    }
    if (count > eval->max_count) {
        bt_ga_cuda_note(err, "cuda.population", "count exceeds the allocated device buffers");
        return -1;
    }
    size_t chrom_bytes = count * eval->device.case_count * sizeof(uint32_t);
    cudaError_t code = cudaMemcpy(eval->chroms, flat, chrom_bytes, cudaMemcpyHostToDevice);
    if (code != cudaSuccess) {
        bt_ga_cuda_note(err, "cuda.population", cudaGetErrorString(code));
        return -1;
    }
    size_t blocks = (count + BT_GA_CUDA_BLOCK - 1) / BT_GA_CUDA_BLOCK;
    bt_ga_cuda_fitness_kernel<<<(unsigned)blocks, BT_GA_CUDA_BLOCK>>>(eval->device, eval->chroms,
                                                                      count, eval->scores);
    code = cudaGetLastError();
    if (code != cudaSuccess) {
        bt_ga_cuda_note(err, "cuda.launch", cudaGetErrorString(code));
        return -1;
    }
    code = cudaDeviceSynchronize();
    if (code != cudaSuccess) {
        bt_ga_cuda_note(err, "cuda.sync", cudaGetErrorString(code));
        return -1;
    }
    code = cudaMemcpy(scores, eval->scores, count * sizeof(float), cudaMemcpyDeviceToHost);
    if (code != cudaSuccess) {
        bt_ga_cuda_note(err, "cuda.scores", cudaGetErrorString(code));
        return -1;
    }
    return 0;
}
