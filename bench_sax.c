/* bench_sax.c — SAX parser benchmark: JSON serializer + PopLine comparison */

#include "popline_sax.h"
#include "popline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════
 *                      JSON Serializer
 * ═══════════════════════════════════════════════════════════════ */

#define JS_MAX 256

typedef struct {
    char  *buf;
    int    len, cap;
    int    depth;
    int    comma[JS_MAX];
    char   type[JS_MAX];     /* 'o' / 'a' per container level */
} jctx_t;

static void jgrow(jctx_t *j, int n) {
    while (j->len + n + 1 > j->cap) { j->cap = j->cap ? j->cap * 2 : 4096; j->buf = (char *)realloc(j->buf, j->cap); }
}
static void jw(jctx_t *j, const char *s, int n) { jgrow(j, n); memcpy(j->buf + j->len, s, n); j->len += n; }
static void jc(jctx_t *j, char c)               { jgrow(j, 1); j->buf[j->len++] = c; }

static void jstr(jctx_t *j, const char *s, int len) {
    jgrow(j, len * 2 + 4);
    j->buf[j->len++] = '"';
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':  j->buf[j->len++] = '\\'; j->buf[j->len++] = '"';  break;
        case '\\': j->buf[j->len++] = '\\'; j->buf[j->len++] = '\\'; break;
        case '\n': j->buf[j->len++] = '\\'; j->buf[j->len++] = 'n';  break;
        case '\t': j->buf[j->len++] = '\\'; j->buf[j->len++] = 't';  break;
        default:
            if (c < 0x20) {
                j->buf[j->len++] = '\\'; j->buf[j->len++] = 'u'; j->buf[j->len++] = '0'; j->buf[j->len++] = '0';
                j->buf[j->len++] = "0123456789abcdef"[c >> 4];
                j->buf[j->len++] = "0123456789abcdef"[c & 0xf];
            } else { j->buf[j->len++] = c; }
            break;
        }
    }
    j->buf[j->len++] = '"';
}

static void jclose_n(jctx_t *j, int n) {
    for (int i = 0; i < n; i++) {
        char t = j->type[j->depth];
        j->depth--;
        j->comma[j->depth] = 1;
        jc(j, t == 'o' ? '}' : ']');
    }
}

static int json_cb(const pln_sax_ev_t *ev, void *user) {
    jctx_t *j = (jctx_t *)user;
    switch (ev->type) {
    case PLN_SAX_OBJ_BEGIN:
        if (j->depth > 0 && j->comma[j->depth]) jc(j, ',');
        jc(j, '{');
        j->depth++;
        j->type[j->depth] = 'o';
        j->comma[j->depth] = 0;
        break;
    case PLN_SAX_ARR_BEGIN:
        if (j->depth > 0 && j->comma[j->depth]) jc(j, ',');
        jc(j, '[');
        j->depth++;
        j->type[j->depth] = 'a';
        j->comma[j->depth] = 0;
        break;
    case PLN_SAX_OBJ_END:
        j->depth--;
        j->comma[j->depth] = 1;
        jc(j, '}');
        break;
    case PLN_SAX_ARR_END:
        j->depth--;
        j->comma[j->depth] = 1;
        jc(j, ']');
        break;
    case PLN_SAX_KEY:
        if (j->comma[j->depth]) jc(j, ',');
        j->comma[j->depth] = 0;
        jstr(j, ev->data, ev->len);
        jc(j, ':');
        break;
    case PLN_SAX_STR:
        if (j->comma[j->depth]) jc(j, ',');
        j->comma[j->depth] = 1;
        jstr(j, ev->data, ev->len);
        if (ev->pop) jclose_n(j, ev->pop);
        break;
    case PLN_SAX_INT: {
        if (j->comma[j->depth]) jc(j, ',');
        j->comma[j->depth] = 1;
        char tmp[32]; int n = snprintf(tmp, sizeof(tmp), "%lld", ev->int_val);
        jw(j, tmp, n);
        if (ev->pop) jclose_n(j, ev->pop);
        break;
    }
    case PLN_SAX_FLOAT: {
        if (j->comma[j->depth]) jc(j, ',');
        j->comma[j->depth] = 1;
        char tmp[64]; int n = snprintf(tmp, sizeof(tmp), "%.15g", ev->float_val);
        jw(j, tmp, n);
        if (ev->pop) jclose_n(j, ev->pop);
        break;
    }
    case PLN_SAX_BOOL:
        if (j->comma[j->depth]) jc(j, ',');
        j->comma[j->depth] = 1;
        jw(j, ev->bool_val ? "true" : "false", ev->bool_val ? 4 : 5);
        if (ev->pop) jclose_n(j, ev->pop);
        break;
    case PLN_SAX_NULL:
        if (j->comma[j->depth]) jc(j, ',');
        j->comma[j->depth] = 1;
        jw(j, "null", 4);
        if (ev->pop) jclose_n(j, ev->pop);
        break;
    case PLN_SAX_DONE:
        jc(j, '\0');
        break;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *                   PopLine Serializer (via SAX events)
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    char  *buf;
    int    len, cap;
    int    pending_pop;      /* EOF flush pop count */
} pctx_t;

static void pgrow(pctx_t *p, int n) {
    while (p->len + n + 1 > p->cap) { p->cap = p->cap ? p->cap * 2 : 4096; p->buf = (char *)realloc(p->buf, p->cap); }
}
static void pw(pctx_t *p, const char *s, int n) { pgrow(p, n); memcpy(p->buf + p->len, s, n); p->len += n; }
static void pc(pctx_t *p, char c)               { pgrow(p, 1); p->buf[p->len++] = c; }

static void ppop(pctx_t *p, int n) {
    if (n <= 0) return;
    char tmp[16]; int i = 0;
    int pp = n;
    if (pp >= 100) { tmp[i++] = '0' + pp / 100; pp %= 100; }
    if (pp >= 10)  { tmp[i++] = '0' + pp / 10;  pp %= 10; }
    tmp[i++] = '0' + pp;
    pgrow(p, i + 2);
    p->buf[p->len++] = ' ';
    memcpy(p->buf + p->len, tmp, i);
    p->len += i;
    p->buf[p->len++] = '\n';
}

/* Insert " N" before the last newline for EOF flush */
static void ppop_eof(pctx_t *p, int n) {
    if (n <= 0 || p->len <= 0) return;
    char tmp[16]; int i = 0;
    int pp = n;
    if (pp >= 100) { tmp[i++] = '0' + pp / 100; pp %= 100; }
    if (pp >= 10)  { tmp[i++] = '0' + pp / 10;  pp %= 10; }
    tmp[i++] = '0' + pp;
    pgrow(p, i + 1);
    int end = p->len - 1;
    if (p->buf[end] == '\n') {
        memmove(p->buf + end + i + 1, p->buf + end, 1);
        p->buf[end] = ' ';
        memcpy(p->buf + end + 1, tmp, i);
        p->len += i + 1;
    } else {
        /* no trailing newline — append directly */
        p->buf[p->len++] = ' ';
        memcpy(p->buf + p->len, tmp, i);
        p->len += i;
    }
}

static int pln_cb(const pln_sax_ev_t *ev, void *user) {
    pctx_t *p = (pctx_t *)user;
    switch (ev->type) {
    case PLN_SAX_OBJ_BEGIN: pc(p, '{'); pc(p, '\n'); break;
    case PLN_SAX_ARR_BEGIN: pc(p, '['); pc(p, '\n'); break;
    case PLN_SAX_OBJ_END:
    case PLN_SAX_ARR_END:   p->pending_pop += ev->pop ? ev->pop : 1; break;
    case PLN_SAX_KEY:       pw(p, ev->data, ev->len); pw(p, ": ", 2); break;
    case PLN_SAX_STR:
        pc(p, '"'); pw(p, ev->data, ev->len); pc(p, '"');
        if (ev->pop) ppop(p, ev->pop); else pc(p, '\n');
        break;
    case PLN_SAX_INT: {
        char tmp[32]; int n = snprintf(tmp, sizeof(tmp), "%lld", ev->int_val);
        pw(p, tmp, n);
        if (ev->pop) ppop(p, ev->pop); else pc(p, '\n');
        break;
    }
    case PLN_SAX_FLOAT: {
        char tmp[64]; int n = snprintf(tmp, sizeof(tmp), "%.15g", ev->float_val);
        pw(p, tmp, n);
        if (ev->pop) ppop(p, ev->pop); else pc(p, '\n');
        break;
    }
    case PLN_SAX_BOOL:
        pw(p, ev->bool_val ? "true" : "false", ev->bool_val ? 4 : 5);
        if (ev->pop) ppop(p, ev->pop); else pc(p, '\n');
        break;
    case PLN_SAX_NULL:
        pw(p, "null", 4);
        if (ev->pop) ppop(p, ev->pop); else pc(p, '\n');
        break;
    case PLN_SAX_DONE:
        if (p->pending_pop > 0) { ppop_eof(p, p->pending_pop); p->pending_pop = 0; }
        pc(p, '\0');
        break;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *                          Benchmark
 * ═══════════════════════════════════════════════════════════════ */

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static char *read_file(const char *path, long *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    buf[sz] = '\0';
    *len_out = sz;
    return buf;
}

int main(void) {
    long pln_len;
    char *pln_text = read_file("package.pln", &pln_len);
    if (!pln_text) return 1;

    printf("package.pln: %ld bytes\n\n", pln_len);
    const int ITER = 5000;

    /* ── SAX → JSON ── */
    double t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        jctx_t j; memset(&j, 0, sizeof(j));
        pln_sax_parse(pln_text, json_cb, &j);
        free(j.buf);
    }
    double t_json = now_sec() - t0;
    printf("SAX → JSON        : %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_json, t_json / ITER * 1e9, (double)pln_len * ITER / t_json / 1e6);

    /* ── SAX → PopLine + round-trip check ── */
    pctx_t pctx;
    memset(&pctx, 0, sizeof(pctx));
    pln_sax_parse(pln_text, pln_cb, &pctx);

    /* compare: standard pln_loads + pln_dumps */
    pln_value_t *v = pln_loads(pln_text);
    char *std_out = pln_dumps(v);
    int std_len = (int)strlen(std_out);
    pln_value_free(v);

    int sax_len = pctx.len > 0 ? pctx.len - 1 : 0;
    int match = (sax_len == std_len && memcmp(pctx.buf, std_out, std_len) == 0);
    printf("Round-trip        : %s (std=%d sax=%d)\n",
           match ? "MATCH" : "MISMATCH", std_len, sax_len);
    free(std_out);
    free(pctx.buf);

    /* benchmark SAX → PopLine */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        pctx_t pc; memset(&pc, 0, sizeof(pc));
        pln_sax_parse(pln_text, pln_cb, &pc);
        free(pc.buf);
    }
    double t_pln = now_sec() - t0;
    printf("SAX → PopLine     : %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_pln, t_pln / ITER * 1e9, (double)pln_len * ITER / t_pln / 1e6);

    /* ── pln_loads + pln_dumps (standard) ── */
    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        pln_value_t *vv = pln_loads(pln_text);
        char *ss = pln_dumps(vv);
        pln_value_free(vv);
        free(ss);
    }
    double t_std = now_sec() - t0;
    printf("parse + dumps     : %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_std, t_std / ITER * 1e9, (double)pln_len * ITER / t_std / 1e6);

    printf("\n── Speedup ──\n");
    printf("SAX→PopLine vs parse+dumps : %.2fx\n", t_std / t_pln);
    printf("SAX→JSON  vs parse+dumps : %.2fx\n", t_std / t_json);

    /* verify JSON output */
    jctx_t jv; memset(&jv, 0, sizeof(jv));
    pln_sax_parse(pln_text, json_cb, &jv);
    size_t json_len = jv.len > 0 ? jv.len - 1 : 0;
    printf("\nJSON output       : %zu bytes (ratio %.2f)\n", json_len, (double)json_len / pln_len);
    printf("JSON preview      : %.*s\n", 200, jv.buf);
    free(jv.buf);

    free(pln_text);
    return 0;
}
