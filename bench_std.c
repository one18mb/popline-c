/* bench_std.c — Standard benchmark: README-style parse/serialize comparison */

#include "pln_val.h"
#include "popline_sax.h"
#include "sax_formats.h"
#include "popline.h"
#include "../popline-cli/cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static char *read_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(sz + 1);
    if (fread(b, 1, sz, f) != (size_t)sz) { free(b); fclose(f); return NULL; }
    fclose(f); b[sz] = '\0'; *len = sz; return b;
}

/* Convert old pln_value_t → cJSON */
static cJSON *pln_to_cjson(pln_value_t *v) {
    if (!v) return NULL;
    switch (v->type) {
    case PLN_OBJECT: {
        cJSON *o = cJSON_CreateObject();
        for (pln_value_t *c = v->child; c; c = c->next)
            { cJSON *cv = pln_to_cjson(c); if (cv) cJSON_AddItemToObject(o, c->key, cv); }
        return o;
    }
    case PLN_ARRAY: {
        cJSON *a = cJSON_CreateArray();
        for (pln_value_t *c = v->child; c; c = c->next)
            { cJSON *cv = pln_to_cjson(c); if (cv) cJSON_AddItemToArray(a, cv); }
        return a;
    }
    case PLN_STRING: return cJSON_CreateString(v->data.string_val);
    case PLN_INT:    return cJSON_CreateNumber((double)v->data.int_val);
    case PLN_FLOAT:  return cJSON_CreateNumber(v->data.float_val);
    case PLN_BOOL:   return cJSON_CreateBool(v->data.bool_val);
    case PLN_NULL:   return cJSON_CreateNull();
    }
    return NULL;
}

int main(void) {
    long pln_len, json_len;
    char *pln_text = read_file("package.pln", &pln_len);
    char *json_text = read_file("package.json", &json_len);
    if (!pln_text || !json_text) { fprintf(stderr, "read failed\n"); return 1; }

    printf("测试数据: package.json (%ld B) → package.pln (%ld B, %.1f%%)\n",
           json_len, pln_len, 100.0 * pln_len / json_len);
    printf("统一 5000 次迭代\n\n", json_len);
    const int ITER = 5000;

    /* ── 解析 (文本 → DOM) ── */
    double t0;

    /* 旧 pln DOM 解析 */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { pln_value_t *v = pln_loads(pln_text); pln_value_free(v); }
    double t_pln_parse_old = now_sec() - t0;

    /* 新 pln DOM 解析 (SAX → builder) */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { pln_val_t *v = pln_val_parse(pln_text); pln_val_free(v); }
    double t_pln_parse_new = now_sec() - t0;

    /* JSON 解析 (cJSON) */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { cJSON *j = cJSON_Parse(json_text); cJSON_Delete(j); }
    double t_json_parse = now_sec() - t0;

    /* ── 序列化 (DOM → 文本) ── */
    pln_value_t *old_dom = pln_loads(pln_text);
    pln_val_t   *new_dom = pln_val_parse(pln_text);
    cJSON *cjson_dom = cJSON_Parse(json_text);

    /* 旧 pln 序列化 */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { char *s = pln_dumps(old_dom); free(s); }
    double t_pln_ser_old = now_sec() - t0;

    /* 新 pln 序列化 */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { char *s = pln_val_dumps(new_dom); free(s); }
    double t_pln_ser_new = now_sec() - t0;

    /* JSON 序列化 (cJSON) */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { char *s = cJSON_PrintUnformatted(cjson_dom); free(s); }
    double t_json_ser = now_sec() - t0;

    /* ── SAX direct (一次遍历, 无 DOM) ── */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) { char *s = sax_to_json(pln_text); free(s); }
    double t_sax_json = now_sec() - t0;

    pln_value_free(old_dom); pln_val_free(new_dom); cJSON_Delete(cjson_dom);

    /* ── 结果输出 ── */
    printf("| 平台 | 序列化 (vs JSON) | 解析 (vs JSON) |\n");
    printf("|------|-----------------|---------------|\n");
    printf("| **C (旧 DOM)** | **%.2fx** %s | **%.2fx** %s |\n",
           t_pln_ser_old / t_json_ser, t_pln_ser_old < t_json_ser ? "🟢" : "🔴",
           t_pln_parse_old / t_json_parse, t_pln_parse_old < t_json_parse ? "🟢" : "🔴");
    printf("| **C (新 DOM)** | **%.2fx** %s | **%.2fx** %s |\n",
           t_pln_ser_new / t_json_ser, t_pln_ser_new < t_json_ser ? "🟢" : "🔴",
           t_pln_parse_new / t_json_parse, t_pln_parse_new < t_json_parse ? "🟢" : "🔴");
    printf("| **SAX→JSON** | **--** | **--** |\n\n");

    printf("详细数据:\n\n");
    printf("解析 (文本 → DOM):\n");
    printf("  JSON (cJSON_Parse):        %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_json_parse, t_json_parse / ITER * 1e9, (double)json_len * ITER / t_json_parse / 1e6);
    printf("  PopLine (旧 pln_loads):    %.4f s  (%.0f ns/iter, %.1f MB/s)  ratio=%.2f\n",
           t_pln_parse_old, t_pln_parse_old / ITER * 1e9,
           (double)pln_len * ITER / t_pln_parse_old / 1e6,
           t_pln_parse_old / t_json_parse);
    printf("  PopLine (新 pln_val_parse): %.4f s  (%.0f ns/iter, %.1f MB/s)  ratio=%.2f\n",
           t_pln_parse_new, t_pln_parse_new / ITER * 1e9,
           (double)pln_len * ITER / t_pln_parse_new / 1e6,
           t_pln_parse_new / t_json_parse);
    printf("\n序列化 (DOM → 文本):\n");
    printf("  JSON (cJSON_PrintUnformatted): %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_json_ser, t_json_ser / ITER * 1e9, (double)json_len * ITER / t_json_ser / 1e6);
    printf("  PopLine (旧 pln_dumps):       %.4f s  (%.0f ns/iter, %.1f MB/s)  ratio=%.2f\n",
           t_pln_ser_old, t_pln_ser_old / ITER * 1e9,
           (double)pln_len * ITER / t_pln_ser_old / 1e6,
           t_pln_ser_old / t_json_ser);
    printf("  PopLine (新 pln_val_dumps):   %.4f s  (%.0f ns/iter, %.1f MB/s)  ratio=%.2f\n",
           t_pln_ser_new, t_pln_ser_new / ITER * 1e9,
           (double)pln_len * ITER / t_pln_ser_new / 1e6,
           t_pln_ser_new / t_json_ser);
    printf("\n一次遍历 (无 DOM):\n");
    printf("  SAX→JSON:                    %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_sax_json, t_sax_json / ITER * 1e9,
           (double)pln_len * ITER / t_sax_json / 1e6);

    free(pln_text); free(json_text);
    return 0;
}
