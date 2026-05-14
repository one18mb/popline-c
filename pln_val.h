/* pln_val.h — Compact DOM: single malloc, key+data inlined */
#ifndef PLN_VAL_H
#define PLN_VAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLN_V_NULL, PLN_V_BOOL, PLN_V_INT, PLN_V_FLOAT,
    PLN_V_STR, PLN_V_OBJ, PLN_V_ARR
} pln_vtype_t;

typedef struct pln_val {
    struct pln_val *child;    /* first child / first element */
    struct pln_val *next;     /* next sibling */
    unsigned char   type;
    unsigned char   key_len;  /* 0 = no key */
    unsigned short  str_len;  /* string byte length (0 = no string) */
    union {
        long long ival;
        double    fval;
        int       bval;
    } data;
    char buf[];               /* [key\0][str_data\0] */
} pln_val_t;

/* ─── Allocation ──────────────────────────── */

pln_val_t *pln_val_alloc(pln_vtype_t type, int key_len, int str_len);

/* ─── Constructors ────────────────────────── */

pln_val_t *pln_val_obj(void);
pln_val_t *pln_val_arr(void);
pln_val_t *pln_val_int(long long v);
pln_val_t *pln_val_float(double v);
pln_val_t *pln_val_bool(int v);
pln_val_t *pln_val_null(void);
pln_val_t *pln_val_str(const char *s, int len);

/* ─── Accessors ───────────────────────────── */

static inline const char *pln_val_key(const pln_val_t *v) {
    return v->key_len ? v->buf : NULL;
}
static inline const char *pln_val_get_str(const pln_val_t *v) {
    return v->buf + v->key_len + 1;
}

/* ─── Tree building ───────────────────────── */

void pln_val_set_key_len(pln_val_t *v, const char *k, int klen);
void pln_val_add_child(pln_val_t *parent, pln_val_t *child);

/* ─── Parse & serialize ───────────────────── */

pln_val_t *pln_val_parse(const char *text);
char      *pln_val_dumps(pln_val_t *v);

/* ─── Free ────────────────────────────────── */

void pln_val_free(pln_val_t *v);

#ifdef __cplusplus
}
#endif

#endif /* PLN_VAL_H */
