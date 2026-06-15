# MWRender 代码质量复审报告

复审日期：2026-06-15

## 1. 总体结论

上一轮审查发现的四个核心问题已经得到有效修复：

- 远程 CSS 转义和注释混淆绕过已被拦截。
- MathJax、Mermaid 和 Highlight.js 已改为构建时嵌入，不再依赖当前工作目录。
- `maxNestingDepth` 已生效，并能产生 `MW0004` 诊断。
- 任务列表 CSS 类名与测试契约已经统一。

现有 MinGW 构建中的 9 项 CTest 全部通过，安装后的 CLI 也能够从任意目录正常生成包含离线运行时的 HTML。

不过，本次复审发现了一个新的发布阻断问题：全新 MSVC 构建会因嵌入资源生成的 C++ 字符串过大而失败。

综合评价：**7.5/10**

## 2. 审查发现

### 2.1 高风险：全新 MSVC 构建失败

相关文件：

- `CMakeLists.txt:29`
- `cmake/builtin_resources.hpp.in:7`
- `cmake/builtin_theme.hpp.in`

项目通过 CMake 的 `file(READ)` 和 `configure_file`，将 MathJax、Mermaid、Highlight.js、代码高亮 CSS 和内置主题 CSS 生成为大型 C++ 原始字符串字面量。

在已有 MinGW 构建目录中，项目能够正常编译和测试。但使用 Visual Studio 2022 和 MSVC 创建全新构建目录时，编译失败并报告：

```text
error C2026: 字符串太大，已截断尾部字符
```

错误同时出现在：

- `generated/mwrender/builtin_resources.hpp`
- `generated/mwrender/builtin_theme.hpp`

影响：

- Windows 用户按照 README 中的常规 CMake 命令构建时可能直接失败。
- 当前成功的增量构建不能代表干净环境可复现。
- 项目安装、发布和 CI 的跨编译器可靠性不足。

建议：

1. 在生成阶段将大型资源拆分成多个较小的字符串字面量。
2. 或生成 `unsigned char` 字节数组，再通过长度构造 `std::string_view`。
3. 为 MSVC 增加干净构建 CI，不复用历史构建目录。
4. 同时验证 MinGW、MSVC 和至少一种 Linux 编译器。

### 2.2 中风险：兼容性文档仍然自相矛盾

相关文件：

- `docs/Compatibility.md:44`
- `docs/Compatibility.md:120`
- `docs/Compatibility.md:144`

文档前部声明以下功能已经嵌入并受到支持：

- MathJax Offline
- Mermaid Offline
- Highlight.js Offline

但后面的 Unsupported Features 和 Known Limitations 又将这些能力描述为未实现或需要宿主集成。

影响：

- 用户无法准确判断当前版本的能力。
- 发布说明、集成方案和测试预期可能产生分歧。

建议：

1. 从 Unsupported Features 中删除已经实现的功能。
2. 更新 Known Limitations，说明这些功能仅在完整 HTML 模式下嵌入运行时。
3. 将 `docs/Compatibility.md` 作为功能状态的唯一事实来源。

### 2.3 低风险：CLI 文档仍包含过期或错误内容

相关文件：

- `docs/CLI_USAGE.md:21`
- `docs/CLI_USAGE.md:26`
- `docs/CLI_USAGE.md:27`
- `docs/CLI_USAGE.md:28`

发现的问题：

- 文档仍列出无效的 HTML policy 值 `sanitize`，实际参数为 `sanitized`。
- 文档使用 `--theme-dir`，实际 CLI 参数为 `--theme-path`。
- 文档描述了不存在的 `--log-level` 参数。
- `--allow-document-css` 的实际作用是允许读取 Front Matter 引用的 CSS 文件，不是控制是否生成完整 HTML。

建议：

1. 以 `mwrender --help` 的输出为准更新参数列表。
2. 删除不存在的参数和值。
3. 分开描述 `--fragment` 与 `--allow-document-css`。
4. 后续可考虑从 CLI 参数定义自动生成帮助文档。

### 2.4 低风险：一项嵌套深度测试仍然较弱

相关文件：

- `tests/stress_test.cpp:356`

`testNestingDepthLimit` 将 `maxNestingDepth` 设置为 5，但只验证输出不为空：

```cpp
require(!result.html.empty(), "nesting depth limit: output should not be empty");
```

这无法证明深度限制确实生效。

项目中的另一项默认深度测试已经能够验证 `MW0004`，因此不属于功能缺陷，但该测试仍有改进空间。

建议：

- 验证结果包含 `MW0004`。
- 检查实际 AST 或 HTML 嵌套层数没有超过配置值。
- 将两个深度测试整合为参数化测试，减少重复。

## 3. 已确认修复

### 3.1 测试基线恢复

现有构建目录执行结果：

```text
测试总数：9
通过：9
失败：0
```

通过的测试包括：

- Smoke
- Edge
- CommonMark/GFM conformance
- Stress
- Security
- Benchmark
- CLI version
- CLI themes
- CLI config

### 3.2 远程 CSS 检测增强

以下测试变体均被成功拒绝：

- 十六进制转义并带终止空格
- 六位十六进制转义
- 普通字符转义
- 注释拆分协议名称
- 转义冒号

示例：

```css
.x { background: url(\68 ttps://evil.example/a); }
.x { background: url(ht/**/tps://evil.example/a); }
.x { background: url(https\3a //evil.example/a); }
```

最终 HTML 均未包含 `evil.example`。

### 3.3 离线资源不再依赖工作目录

从 `C:\Windows\Temp` 调用构建产物时，生成的 HTML 仍包含：

- `window.MathJax`
- `mermaid.initialize`
- `hljs.highlightAll`

安装到临时目录后的 CLI 也通过了相同验证。

### 3.4 嵌套深度限制已生效

解析器现在会跟踪当前嵌套深度。超过 `EngineOptions::maxNestingDepth` 时：

- 停止继续递归解析。
- 返回可渲染结果。
- 产生 `MW0004` 警告。

### 3.5 任务列表契约已统一

渲染器现在输出：

```html
<ul class="mw-list mw-task-list">
```

Smoke、conformance 和 CLI config 测试均已通过。

## 4. 整改优先级

### P0：发布前必须处理

1. 修复大型嵌入资源导致的 MSVC 干净构建失败。
2. 增加 MSVC 干净构建 CI。

### P1：短期处理

1. 清理 `docs/Compatibility.md` 中互相冲突的功能状态。
2. 根据实际 `--help` 输出修正 CLI 文档。

### P2：持续改进

1. 强化 `testNestingDepthLimit` 的断言。
2. 增加跨编译器和安装产物的自动化测试。
3. 避免将生成目录 `Testing/` 留在源码根目录，或将其加入 `.gitignore`。

## 5. 结论

本轮修复显著提升了项目的安全性、功能稳定性和测试可靠性。上一轮的核心问题均已得到实质解决，而不是仅修改测试绕过。

当前最重要的剩余问题是大型资源嵌入方式破坏了 MSVC 的干净构建。解决该问题并同步修正文档后，项目质量可进一步提升到适合发布的水平。
