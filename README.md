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

测试数据：`package.json`（17011 B）→ `package.pln`（13074 B，**76.9%**），50000 次迭代

| 操作 | JSON (cJSON) | PopLine | 比 |
|------|-------------|---------|------|
| 解析 | 4954 ms (99 µs/op) | 3718 ms (74 µs/op) | **0.75x** |
| 序列化 | 2692 ms (54 µs/op) | 1742 ms (35 µs/op) | **0.65x** |

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
