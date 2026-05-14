/* pln_val.c — Compact DOM implementation */

#include "pln_val.h"
#include "popline_sax.h"
#include "popline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════
 *                      Allocation
 * ═══════════════════════════════════════════════════════════════ */

pln_val_t *pln_val_alloc(pln_vtype_t type, int key_len, int str_len) {
    int sz = sizeof(pln_val_t) + key_len + 1 + str_len + 1;
    pln_val_t *v = (pln_val_t *)calloc(1, sz);
    if (!v) { fprintf(stderr, "OOM\n"); abort(); }
    v->type    = (unsigned char)type;
    v->key_len = (unsigned char)key_len;
    v->str_len = (unsigned short)str_len;
    return v;
}

pln_val_t *pln_val_obj(void)          { return pln_val_alloc(PLN_V_OBJ, 0, 0); }
pln_val_t *pln_val_arr(void)          { return pln_val_alloc(PLN_V_ARR, 0, 0); }
pln_val_t *pln_val_int(long long v)   { pln_val_t *p = pln_val_alloc(PLN_V_INT, 0, 0);  p->data.ival = v; return p; }
pln_val_t *pln_val_float(double v)    { pln_val_t *p = pln_val_alloc(PLN_V_FLOAT, 0, 0); p->data.fval = v; return p; }
pln_val_t *pln_val_bool(int v)        { pln_val_t *p = pln_val_alloc(PLN_V_BOOL, 0, 0);  p->data.bval = v; return p; }
pln_val_t *pln_val_null(void)         { return pln_val_alloc(PLN_V_NULL, 0, 0); }
pln_val_t *pln_val_str(const char *s, int len) {
    pln_val_t *p = pln_val_alloc(PLN_V_STR, 0, len);
    memcpy(p->buf, s, len);
    p->buf[len] = '\0';
    return p;
}

/* ═══════════════════════════════════════════════════════════════
 *                     Tree building
 * ═══════════════════════════════════════════════════════════════ */

void pln_val_set_key_len(pln_val_t *v, const char *k, int klen) {
    /* Shift string data right to make room for key */
    if (v->str_len > 0) {
        memmove(v->buf + klen + 1, v->buf, v->str_len + 1);
    }
    memcpy(v->buf, k, klen);
    v->buf[klen] = '\0';
    v->key_len = (unsigned char)klen;
}

void pln_val_add_child(pln_val_t *parent, pln_val_t *child) {
    if (!parent->child) {
        parent->child = child;
    } else {
        pln_val_t *sib = parent->child;
        while (sib->next) sib = sib->next;
        sib->next = child;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *                    SAX → DOM builder
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    pln_val_t **stack;       /* container stack */
    int         slen, scap;
    pln_val_t **tail[256];   /* per-level tail, saved across nesting */
    char       *key;         /* saved key from KEY event */
    int         klen;
    pln_val_t  *root;
    int         error;
} dbctx_t;

static void db_init(dbctx_t *d) {
    memset(d, 0, sizeof(*d));
}

static void db_push(dbctx_t *d, pln_val_t *v) {
    d->tail[d->slen] = &v->child;
    if (d->slen >= d->scap) {
        d->scap = d->scap ? d->scap * 2 : 64;
        d->stack = (pln_val_t **)realloc(d->stack, d->scap * sizeof(pln_val_t *));
    }
    d->stack[d->slen++] = v;
}

static void db_add(dbctx_t *d, pln_val_t *v) {
    if (d->slen == 0) { d->root = v; return; }
    *d->tail[d->slen - 1] = v;
    d->tail[d->slen - 1] = &v->next;
}

static int db_cb(const pln_sax_ev_t *ev, void *user) {
    dbctx_t *d = (dbctx_t *)user;
    switch (ev->type) {
    case PLN_SAX_OBJ_BEGIN: {
        pln_val_t *o = pln_val_alloc(PLN_V_OBJ, d->klen, 0);
        if (d->klen > 0) { memcpy(o->buf, d->key, d->klen); o->buf[d->klen] = '\0'; d->klen = 0; }
        db_add(d, o);
        db_push(d, o);
        break;
    }
    case PLN_SAX_ARR_BEGIN: {
        pln_val_t *a = pln_val_alloc(PLN_V_ARR, d->klen, 0);
        if (d->klen > 0) { memcpy(a->buf, d->key, d->klen); a->buf[d->klen] = '\0'; d->klen = 0; }
        db_add(d, a);
        db_push(d, a);
        break;
    }
    case PLN_SAX_OBJ_END:
    case PLN_SAX_ARR_END:
        if (d->slen > 0) d->slen--;
        break;
    case PLN_SAX_KEY:
        d->klen = ev->len;
        if (d->klen > 0) { memcpy(d->key ? d->key : (d->key = (char *)malloc(256)), ev->data, d->klen); }
        break;
    case PLN_SAX_STR: {
        pln_val_t *s = pln_val_alloc(PLN_V_STR, d->klen, ev->len);
        if (d->klen > 0) { memcpy(s->buf, d->key, d->klen); s->buf[d->klen] = '\0'; d->klen = 0; }
        memcpy(s->buf + s->key_len + 1, ev->data, ev->len);
        s->buf[s->key_len + 1 + ev->len] = '\0';
        db_add(d, s);
        if (ev->pop) { int p = ev->pop; while (p-- && d->slen > 0) d->slen--; }
        break;
    }
    case PLN_SAX_INT: {
        pln_val_t *iv = pln_val_alloc(PLN_V_INT, d->klen, 0);
        if (d->klen > 0) { memcpy(iv->buf, d->key, d->klen); iv->buf[d->klen] = '\0'; d->klen = 0; }
        iv->data.ival = ev->int_val;
        db_add(d, iv);
        if (ev->pop) { int p = ev->pop; while (p-- && d->slen > 0) d->slen--; }
        break;
    }
    case PLN_SAX_FLOAT: {
        pln_val_t *fv = pln_val_alloc(PLN_V_FLOAT, d->klen, 0);
        if (d->klen > 0) { memcpy(fv->buf, d->key, d->klen); fv->buf[d->klen] = '\0'; d->klen = 0; }
        fv->data.fval = ev->float_val;
        db_add(d, fv);
        if (ev->pop) { int p = ev->pop; while (p-- && d->slen > 0) d->slen--; }
        break;
    }
    case PLN_SAX_BOOL: {
        pln_val_t *bv = pln_val_alloc(PLN_V_BOOL, d->klen, 0);
        if (d->klen > 0) { memcpy(bv->buf, d->key, d->klen); bv->buf[d->klen] = '\0'; d->klen = 0; }
        bv->data.bval = ev->bool_val;
        db_add(d, bv);
        if (ev->pop) { int p = ev->pop; while (p-- && d->slen > 0) d->slen--; }
        break;
    }
    case PLN_SAX_NULL: {
        pln_val_t *nv = pln_val_alloc(PLN_V_NULL, d->klen, 0);
        if (d->klen > 0) { memcpy(nv->buf, d->key, d->klen); nv->buf[d->klen] = '\0'; d->klen = 0; }
        db_add(d, nv);
        if (ev->pop) { int p = ev->pop; while (p-- && d->slen > 0) d->slen--; }
        break;
    }
    case PLN_SAX_DONE: break;
    }
    return 0;
}

pln_val_t *pln_val_parse(const char *text) {
    dbctx_t d;
    db_init(&d);
    pln_sax_parse(text, db_cb, &d);
    pln_val_t *r = d.root;
    free(d.stack); free(d.key);
    return r;
}

/* ═══════════════════════════════════════════════════════════════
 *                      Serialization
 * ═══════════════════════════════════════════════════════════════ */

static void val_write(pln_gen_t *g, pln_val_t *v) {
    switch (v->type) {
    case PLN_V_OBJ: {
        pln_gen_begin_object(g);
        for (pln_val_t *c = v->child; c; c = c->next) {
            if (c->key_len) pln_gen_key(g, c->buf);
            val_write(g, c);
        }
        pln_gen_end_object(g);
        break;
    }
    case PLN_V_ARR: {
        pln_gen_begin_array(g);
        for (pln_val_t *c = v->child; c; c = c->next) val_write(g, c);
        pln_gen_end_array(g);
        break;
    }
    case PLN_V_STR:
        pln_gen_value_string(g, pln_val_get_str(v));
        break;
    case PLN_V_INT:
        pln_gen_value_int(g, v->data.ival);
        break;
    case PLN_V_FLOAT:
        pln_gen_value_float(g, v->data.fval);
        break;
    case PLN_V_BOOL:
        pln_gen_value_bool(g, v->data.bval);
        break;
    case PLN_V_NULL:
        pln_gen_value_null(g);
        break;
    }
}

char *pln_val_dumps(pln_val_t *v) {
    if (!v) return NULL;
    pln_gen_t g;
    pln_gen_init(&g);
    val_write(&g, v);
    if (g.pending_pop > 0 && g.has_leaf_value) {
        /* emulate g_flush_pop */
        if (g.len > 0 && g.buf[g.len - 1] == '\n') {
            char tmp[16]; int i = 0, x = g.pending_pop;
            if (x >= 100) { tmp[i++] = '0' + x/100; x %= 100; }
            if (x >= 10)  { tmp[i++] = '0' + x/10;  x %= 10; }
            tmp[i++] = '0' + x;
            if (g.len + i + 1 > g.cap) {
                do { g.cap *= 2; } while (g.len + i + 1 > g.cap);
                g.buf = (char *)realloc(g.buf, g.cap);
            }
            int p = g.len - 1;
            memmove(g.buf + p + i + 1, g.buf + p, 1);
            g.buf[p] = ' ';
            memcpy(g.buf + p + 1, tmp, i);
            g.len += i + 1;
        }
        g.pending_pop = 0;
    }
    g.buf[g.len] = '\0';
    char *result = strdup(g.buf);
    pln_gen_free(&g);
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 *                      Free
 * ═══════════════════════════════════════════════════════════════ */

void pln_val_free(pln_val_t *v) {
    if (!v) return;
    pln_val_free(v->child);
    pln_val_free(v->next);
    free(v);
}
