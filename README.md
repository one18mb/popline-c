# PopLine C

PopLine 序列化格式的 C 参考实现。

## API

```c
#include "popline.h"

// 解析
pln_value_t *v = pln_loads("{\nkey: \"value\"\n");

// 序列化
char *s = pln_dumps(v);
free(s);
pln_value_free(v);

// JSON 互转
pln_value_t *jv = pln_loads_json("{\"key\":\"value\"}");
char *js = pln_dumps_json(jv);
```

## 构建与测试

```bash
gcc -O2 -o test test.c popline.c popline_parser.c popline_json.c -lcjson -lm && ./test
```

## 性能

测试数据：`package.json`（17011 B）→ `package.pln`（13074 B，**76.9%**），5000 次迭代

| 操作 | JSON (cJSON) | PopLine | 比 |
|------|-------------|---------|------|
| 解析 | 520 ms (104 µs/op) | 387 ms (77 µs/op) | **0.74x** |
| 序列化 | 275 ms (55 µs/op) | 186 ms (37 µs/op) | **0.68x** |

依赖：`libcjson-dev`（`apt install libcjson-dev`）

## 文件

| 文件 | 说明 |
|------|------|
| `popline.h` | 公共头文件 |
| `popline.c` | 核心（DOM 类型、生成器 `pln_dumps`） |
| `popline_parser.c` | 解析器 `pln_loads` |
| `popline_json.c` | JSON 双向转换 `pln_loads_json`/`pln_dumps_json` |
| `test.c` | 完整测试（单元测试 + JSON 一致性 + 基准） |
| `package.json` | 测试数据 |
| `package.pln` | PopLine 测试数据 |
