# PopLine C

C reference implementation of the PopLine serialization format.

## API

```c
#include "popline.h"

// Parse
pln_value_t *v = pln_loads("{\nkey: \"value\"\n");

// Serialize
char *s = pln_dumps(v);
free(s);
pln_value_free(v);

// JSON conversion
pln_value_t *jv = pln_loads_json("{\"key\":\"value\"}");
char *js = pln_dumps_json(jv);
```

## Build & Test

```bash
gcc -O2 -o test test.c popline.c popline_parser.c popline_json.c -lcjson -lm && ./test
```

Requires: `libcjson-dev` (`apt install libcjson-dev`)

## Performance

Data: `test-package.json` (17011 B) / `test-package.pln` (13074 B, 76.9%)

| Operation | JSON (cJSON) | PopLine | Ratio |
|-----------|-------------|---------|-------|
| Parse | 4954 ms | 3718 ms | **0.75x** |
| Serialize | 2692 ms | 1742 ms | **0.65x** |

## Files

| File | Description |
|------|-------------|
| `popline.h` | Public header |
| `popline.c` | Core (DOM types, generator `pln_dumps`) |
| `popline_parser.c` | Parser `pln_loads` |
| `popline_json.c` | JSON conversion |
| `test.c` | Unit tests + JSON consistency + benchmark |

## Acknowledgments
This project was developed with the assistance of:
- [Claude Code](https://claude.ai) (Anthropic)
- [DeepSeek](https://deepseek.com) (DeepSeek)
