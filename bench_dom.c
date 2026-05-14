/* bench_dom.c — Compare: SAX direct vs new DOM vs old DOM */

#include "pln_val.h"
#include "popline_sax.h"
#include "popline.h"
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
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(sz + 1);
    if (fread(b, 1, sz, f) != (size_t)sz) { free(b); fclose(f); return NULL; }
    fclose(f); b[sz] = '\0'; *len = sz; return b;
}

/* minimal SAX→PopLine serializer for SAX direct path */
typedef struct { char *buf; int len, cap; int ppop; } spctx_t;
static void sg(spctx_t *p, int n) { while (p->len+n+1>p->cap) { p->cap=p->cap?p->cap*2:4096; p->buf=(char*)realloc(p->buf,p->cap); } }
static void sw(spctx_t *p, const char *s, int n) { sg(p,n); memcpy(p->buf+p->len,s,n); p->len+=n; }
static void sc(spctx_t *p, char c) { sg(p,1); p->buf[p->len++]=c; }
static void spop(spctx_t *p, int n) {
    if (n<=0) return; char t[16]; int i=0, x=n;
    while (x) { t[i++] = '0' + x%10; x/=10; }
    for (int j=0;j<i/2;j++) { char tmp=t[j]; t[j]=t[i-1-j]; t[i-1-j]=tmp; }
    sg(p,i+2); p->buf[p->len++]=' '; memcpy(p->buf+p->len,t,i); p->len+=i; p->buf[p->len++]='\n';
}
static void speof(spctx_t *p, int n) {
    if (n<=0||p->len<=0) return;
    char t[16]; int i=0, x=n;
    while (x) { t[i++] = '0' + x%10; x/=10; }
    for (int j=0;j<i/2;j++) { char tmp=t[j]; t[j]=t[i-1-j]; t[i-1-j]=tmp; }
    sg(p,i+1); int e=p->len-1;
    memmove(p->buf+e+i+1,p->buf+e,1); p->buf[e]=' '; memcpy(p->buf+e+1,t,i); p->len+=i+1;
}

static int sax_pln_cb(const pln_sax_ev_t *ev, void *u) {
    spctx_t *p = (spctx_t *)u;
    switch (ev->type) {
    case PLN_SAX_OBJ_BEGIN: sc(p,'{'); sc(p,'\n'); break;
    case PLN_SAX_ARR_BEGIN: sc(p,'['); sc(p,'\n'); break;
    case PLN_SAX_OBJ_END: case PLN_SAX_ARR_END: p->ppop++; break;
    case PLN_SAX_KEY: sw(p,ev->data,ev->len); sw(p,": ",2); break;
    case PLN_SAX_STR:
        sc(p,'"'); sw(p,ev->data,ev->len); sc(p,'"');
        if (ev->pop) spop(p,ev->pop); else sc(p,'\n');
        break;
    case PLN_SAX_INT: {
        char t[32]; int n = snprintf(t,sizeof(t),"%lld",ev->int_val);
        sw(p,t,n); if (ev->pop) spop(p,ev->pop); else sc(p,'\n');
        break;
    }
    case PLN_SAX_FLOAT: {
        char t[64]; int n = snprintf(t,sizeof(t),"%.15g",ev->float_val);
        sw(p,t,n); if (ev->pop) spop(p,ev->pop); else sc(p,'\n');
        break;
    }
    case PLN_SAX_BOOL:
        sw(p,ev->bool_val?"true":"false",ev->bool_val?4:5);
        if (ev->pop) spop(p,ev->pop); else sc(p,'\n');
        break;
    case PLN_SAX_NULL:
        sw(p,"null",4); if (ev->pop) spop(p,ev->pop); else sc(p,'\n');
        break;
    case PLN_SAX_DONE:
        if (p->ppop>0) speof(p,p->ppop);
        sc(p,'\0');
        break;
    }
    return 0;
}

int main(void) {
    long len;
    char *text = read_file("package.pln", &len);
    if (!text) { perror("read"); return 1; }

    printf("Input: %ld bytes\n\n", len);
    const int ITER = 5000;

    /* ── 1. SAX direct (no DOM) ── */
    double t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        spctx_t p; memset(&p,0,sizeof(p));
        pln_sax_parse(text, sax_pln_cb, &p);
        free(p.buf);
    }
    double t_sax = now_sec() - t0;
    printf("SAX direct       : %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_sax, t_sax/ITER*1e9, (double)len*ITER/t_sax/1e6);

    /* ── 2. New DOM (pln_val) ── */
    /* verify round-trip */
    pln_val_t *nv = pln_val_parse(text);
    char *nv_out = pln_val_dumps(nv);
    int nv_match = ((int)strlen(nv_out) == len && memcmp(nv_out, text, len) == 0);
    printf("New DOM roundtrip: %s\n", nv_match ? "MATCH" : "MISMATCH");
    pln_val_free(nv); free(nv_out);

    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        pln_val_t *v = pln_val_parse(text);
        char *s = pln_val_dumps(v);
        pln_val_free(v); free(s);
    }
    double t_new = now_sec() - t0;
    printf("New DOM parse+dump: %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_new, t_new/ITER*1e9, (double)len*ITER/t_new/1e6);

    /* ── 3. Old DOM (pln_value_t) ── */
    pln_value_t *ov = pln_loads(text);
    char *ov_out = pln_dumps(ov);
    int ov_match = ((int)strlen(ov_out) == len && memcmp(ov_out, text, len) == 0);
    printf("Old DOM roundtrip: %s\n", ov_match ? "MATCH" : "MISMATCH");
    pln_value_free(ov); free(ov_out);

    t0 = now_sec();
    for (int i = 0; i < ITER; i++) {
        pln_value_t *v = pln_loads(text);
        char *s = pln_dumps(v);
        pln_value_free(v); free(s);
    }
    double t_old = now_sec() - t0;
    printf("Old DOM parse+dump: %.4f s  (%.0f ns/iter, %.1f MB/s)\n",
           t_old, t_old/ITER*1e9, (double)len*ITER/t_old/1e6);

    printf("\n── Speedup ──\n");
    printf("SAX direct vs Old DOM: %.2fx\n", t_old / t_sax);
    printf("New DOM  vs Old DOM: %.2fx\n", t_old / t_new);
    printf("SAX direct vs New DOM: %.2fx\n", t_new / t_sax);

    free(text);
    return 0;
}
