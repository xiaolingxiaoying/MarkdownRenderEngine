# MWRender 代码质量审查报告

## 1. 总体评价

MWRender 的整体结构较清晰，公开 API、AST、解析器、HTML 渲染器、主题管理和命令行工具之间有明确的职责划分。项目使用了 RAII、`std::unique_ptr`、结构化诊断码，并提供一致性、边界、压力和安全测试，基础工程质量较好。

当前主要问题集中在资源部署、安全策略实现、配置有效性和测试契约一致性上。其中，远程 CSS 检测可被绕过，以及离线脚本依赖当前工作目录，是发布前应优先处理的问题。

综合评价：**6/10**

## 2. 主要问题

### 2.1 高风险：远程 CSS 限制可被绕过

相关代码：

- `src/engine.cpp:21`

当前实现通过删除空白、转换大小写和搜索固定字符串，判断 CSS 是否包含远程资源。这不是完整的 CSS 语法解析，因此无法识别转义或其他等价写法。

例如：

```css
.x {
    background: url(\68 ttps://evil.example/x.png);
}
```

该内容能够绕过检查并进入最终 HTML。实际测试中，程序退出码为 `0`，输出仍包含 `evil.example`。

影响：

- `allowRemoteCssResources = false` 无法提供可靠的安全保证。
- 不可信 CSS 可能触发外部网络请求、用户追踪或信息泄露。

建议：

1. 使用成熟的 CSS tokenizer 或 parser 分析 `url()` 和 `@import`。
2. 如果暂时无法引入解析器，应默认拒绝不可信的自定义 CSS。
3. 增加 CSS 转义、注释、大小写及编码混淆的回归测试。

### 2.2 高风险：离线资源依赖当前工作目录

相关代码：

- `src/render/html_renderer.cpp:729`

MathJax、Mermaid 和 Highlight.js 使用以下相对路径在运行时加载：

```text
resources/js/mathjax-tex-svg.js
resources/js/mermaid.min.js
resources/js/highlight.min.js
resources/css/github.min.css
```

文件读取失败时，代码直接返回空字符串，不产生诊断，也不会令渲染失败。

复现结果：

| 运行目录 | MathJax | Mermaid | Highlight.js | HTML 大小 |
| --- | --- | --- | --- | ---: |
| 项目根目录 | 正常嵌入 | 正常嵌入 | 正常嵌入 | 约 5.2 MB |
| `C:\Windows\Temp` | 缺失 | 缺失 | 缺失 | 约 24 KB |

两次执行的退出码均为 `0`。

影响：

- 安装后的 CLI 或库调用很可能无法找到资源。
- “零依赖离线单文件”功能会静默失效。
- 用户无法从返回结果判断功能是否完整。

建议：

1. 构建时将资源编译进二进制，类似内置主题 CSS。
2. 或根据可执行文件、安装数据目录定位资源，而不是依赖进程工作目录。
3. 资源缺失时产生明确诊断；严格模式下应令渲染失败。
4. 增加从任意工作目录运行安装产物的集成测试。

### 2.3 中风险：`maxNestingDepth` 配置未生效

相关代码：

- `include/mwrender/options.hpp:49`
- `src/parser/parser.cpp:1185`
- `tests/stress_test.cpp:112`

`EngineOptions` 声明了：

```cpp
std::size_t maxNestingDepth = 256;
```

但解析器没有读取或执行该限制。块引用解析会递归调用 `makeDocument`，深层恶意输入可能导致调用栈耗尽。

当前压力测试包含以下恒真断言：

```cpp
require(result.ok || !result.ok, "deep nesting should not crash");
```

该断言无法验证任何行为。另一个深度限制测试只检查输出非空，也没有确认配置值被执行。

建议：

1. 为递归解析传递当前深度，并在超过限制时终止或降级。
2. 返回明确的诊断码。
3. 测试应验证深度 5 与深度 256 产生不同且可预测的结果。
4. 使用超限输入验证程序不会崩溃或无限递归。

### 2.4 中风险：当前测试基线不通过

相关代码：

- `src/render/html_renderer.cpp:428`
- `tests/smoke_test.cpp:217`
- `tests/conformance_test.cpp:320`
- `CMakeLists.txt:132`

渲染器当前输出：

```html
<ul class="mw-list contains-task-list">
```

测试及 CMake 则要求出现：

```text
mw-task-list
```

实际 CTest 结果：

```text
测试总数：9
通过：6
失败：3
```

失败项：

- `mwrender.smoke`
- `mwrender.conformance`
- `mwrender.cli.config`

建议：

1. 明确任务列表 CSS 类名的公共契约。
2. 如果采用 GitHub 风格类名，则同步更新测试和文档。
3. 如果 `mw-task-list` 是稳定 API，则恢复渲染器输出。
4. 在 CI 中将 CTest 作为合并门禁。

### 2.5 低风险：文档与实现存在漂移

相关文档：

- `README.md`
- `docs/Compatibility.md`
- `docs/CLI_USAGE.md`

示例：

- README 宣称 Mermaid、MathJax 和代码高亮已经内置。
- `docs/Compatibility.md` 又将相关能力标为不支持或需要宿主集成。
- CLI 文档中的 HTML policy 参数名称与实际 CLI 接受的值不完全一致。

影响：

- 用户难以判断哪些功能可以直接使用。
- 集成方可能依据错误文档设计部署方案。

建议：

1. 选定一份兼容性文档作为功能状态的唯一事实来源。
2. 从 CLI `--help` 或代码定义生成参数文档。
3. 每次发布前增加文档与功能状态核对清单。

## 3. 做得较好的部分

- API、解析器、渲染器和主题系统边界清楚。
- AST 使用 `std::unique_ptr` 管理所有权，内存模型直观。
- 使用结构化诊断码表达错误和警告。
- 默认禁用原始 HTML，并对危险 URL 做了基础防护。
- 对 UTF-8、输入大小、路径穿越和常见 XSS 场景有测试覆盖。
- 并发渲染时会获取资源快照，避免长时间持锁。
- CMake 安装、导出目标和版本配置较完整。

## 4. 整改优先级

### P0：发布前必须处理

1. 修复远程 CSS 检测绕过。
2. 消除离线资源对当前工作目录的依赖。
3. 修复三个失败测试并恢复全绿基线。

### P1：短期处理

1. 实现 `maxNestingDepth`。
2. 重写无效的压力测试断言。
3. 增加安装后、不同工作目录下的端到端测试。

### P2：持续改进

1. 统一 README、兼容性说明和 CLI 文档。
2. 为资源缺失和功能降级提供可观测诊断。
3. 将基准测试与正确性测试分开，避免性能测试影响常规测试门禁。

## 5. 结论

该项目已经具备较好的模块化基础，也覆盖了不少容易被忽略的安全和边界场景。但目前仍存在两个会影响安全承诺和核心卖点的高风险问题，同时测试基线处于失败状态。

建议先完成 P0 项目，再将当前版本视为可发布版本。
