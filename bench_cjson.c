/* bench_cjson.c — Compare pln→json pipelines against cJSON */

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
        for (pln_value_t *c = v->child; c; c = c->next) {
            cJSON *cv = pln_to_cjson(c);
            if (cv) cJSON_AddItemToObject(o, c->key, cv);
        }
        return o;
    }
    case PLN_ARRAY: {
        cJSON *a = cJSON_CreateArray();
        for (pln_value_t *c = v->child; c; c = c->next) {
            cJSON *cv = pln_to_cjson(c);
            if (cv) cJSON_AddItemToArray(a, cv);
        }
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

/* Convert cJSON → old pln_value_t */
static pln_value_t *cjson_to_pln(cJSON *j) {
    if (!j) return NULL;
    switch (j->type & 0xFF) {
    case cJSON_Object: {
        pln_value_t *o = pln_value_new_object();
        for (cJSON *c = j->child; c; c = c->next) {
            pln_value_t *cv = cjson_to_pln(c);
            if (cv) pln_value_add_to_object(o, c->string, cv);
        }
        return o;
    }
    case cJSON_Array: {
        pln_value_t *a = pln_value_new_array();
        for (cJSON *c = j->child; c; c = c->next) {
            pln_value_t *cv = cjson_to_pln(c);
            if (cv) pln_value_add_to_array(a, cv);
        }
        return a;
    }
    case cJSON_String: return pln_value_new_string(j->valuestring);
    case cJSON_Number: {
        double d = j->valuedouble;
        long long i = (long long)d;
        return (d == (double)i) ? pln_value_new_int(i) : pln_value_new_float(d);
    }
    case cJSON_True:  return pln_value_new_bool(1);
    case cJSON_False: return pln_value_new_bool(0);
    case cJSON_NULL:  return pln_value_new_null();
    }
    return NULL;
}

int main(void) {
    long pln_len, json_len;
    char *pln_text = read_file("package.pln", &pln_len);
    char *json_text = read_file("package.json", &json_len);
    if (!pln_text || !json_text) { fprintf(stderr, "read failed\n"); return 1; }

    printf("package.pln: %ld bytes\n", pln_len);
    printf("package.json: %ld bytes\n\n", json_len);
    const int ITER = 5000;

    /* ═══════════════════════════════════════════════
     *              pln → JSON  路径
     * ═══════════════════════════════════════════════ */

    /* 1. SAX direct → JSON */
    double t0 = now_sec();
    for (int i = 0; i < ITER; i++) { char *s = sax_to_json(pln_text); free(s); }
    double t_sax = now_sec() - t0;
    printf("pln→JSON (SAX direct)  : %.4f s  (%.1f MB/s)\n",
           t_sax, (double)pln_len * ITER / t_sax / 1e6);

    /* 2. 旧 DOM → cJSON → cJSON_Print (当前 CLI) */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        pln_value_t *v = pln_loads(pln_text);
        cJSON *j = pln_to_cjson(v);
        char *s = cJSON_PrintUnformatted(j);
        pln_value_free(v); cJSON_Delete(j); free(s);
    }
    double t_old = now_sec() - t0;
    printf("pln→JSON (DOM→cJSON)   : %.4f s  (%.1f MB/s)\n",
           t_old, (double)pln_len * ITER / t_old / 1e6);

    /* ═══════════════════════════════════════════════
     *           json → pln  路径
     * ═══════════════════════════════════════════════ */

    /* 3. cJSON_Parse → cJSON_Print (JSON baseline) */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        cJSON *j = cJSON_Parse(json_text);
        char *s = cJSON_PrintUnformatted(j);
        cJSON_Delete(j); free(s);
    }
    double t_jj = now_sec() - t0;
    printf("json→json (cJSON native): %.4f s  (%.1f MB/s)\n",
           t_jj, (double)json_len * ITER / t_jj / 1e6);

    /* 4. cJSON → old DOM → pln_dumps (当前 CLI 反向) */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        cJSON *j = cJSON_Parse(json_text);
        pln_value_t *v = cjson_to_pln(j);
        char *s = pln_dumps(v);
        cJSON_Delete(j); pln_value_free(v); free(s);
    }
    double t_jp = now_sec() - t0;
    printf("json→pln (cJSON→DOM)   : %.4f s  (%.1f MB/s)\n",
           t_jp, (double)json_len * ITER / t_jp / 1e6);

    /* ── 验证输出一致性 ── */
    char *sax_out  = sax_to_json(pln_text);
    pln_value_t *vv = pln_loads(pln_text);
    cJSON *cj = pln_to_cjson(vv);
    char *cjson_out = cJSON_PrintUnformatted(cj);
    int match = ((int)strlen(sax_out) == (int)strlen(cjson_out) &&
                 memcmp(sax_out, cjson_out, strlen(sax_out)) == 0);
    printf("\npln→JSON output match: %s (SAX=%zu cJSON=%zu)\n",
           match ? "YES" : "NO", strlen(sax_out), strlen(cjson_out));
    free(sax_out); free(cjson_out); pln_value_free(vv); cJSON_Delete(cj);

    printf("\n── Speedup ──\n");
    printf("SAX pln→JSON vs DOM→cJSON: %.2fx\n", t_old / t_sax);

    free(pln_text); free(json_text);
    return 0;
}
