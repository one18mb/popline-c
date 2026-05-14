# PopLine C

PopLine 序列化格式的 C 参考实现。

## API

```c
#include "popline.h"

// 解析（DOM）
pln_value_t *v = pln_loads("{\nkey: \"value\"\n");

// 序列化
char *s = pln_dumps(v);
free(s);
pln_value_free(v);

// SAX 解析（事件驱动，零 DOM）
int cb(const pln_sax_ev_t *ev, void *u) {
    /* 处理事件... */ return 0;
}
pln_sax_parse(text, cb, user_data);
```

## 构建与测试

```bash
# 纯 PopLine 测试
gcc -O2 -o test test.c popline.c popline_parser.c -lm && ./test
```

## 性能

测试数据：`test-package.json`（17011 B）→ `test-package.pln`（13076 B，**76.9%**），5000 次迭代

| 操作 | JSON (cJSON) | PopLine | 比 |
|------|-------------|---------|------|
| 解析 | 582 ms (116 µs/op) | 496 ms (99 µs/op) | **0.85x** |
| 序列化 | 260 ms (52 µs/op) | 153 ms (31 µs/op) | **0.59x** |

## 文件

| 文件 | 说明 |
|------|------|
| `popline.h` | 公共头文件（DOM + 生成器 API） |
| `popline.c` | 核心（DOM、生成器 `pln_gen_*`、`pln_dumps`） |
| `popline_parser.c` | DOM 解析器 `pln_loads` |
| `popline_sax.c` | SAX 解析器 `pln_sax_parse`（共享于 CLI） |
| `sax_formats.c` | SAX 格式转换器（共享于 CLI） |
| `fmt_json.c` | DOM 格式转换（共享于 CLI） |
| `test.c` | 完整测试 |
| `test-package.json` | 测试数据 |
| `test-package.pln` | PopLine 测试数据 |

## 致谢
本项目的开发得到了以下 AI 工具的大力协助：
- [Claude Code](https://claude.ai)（Anthropic）
- [DeepSeek](https://deepseek.com)（深度求索）
