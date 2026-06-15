# MWRender 发布检查清单

本清单用于 MWRender 每个版本发布前的最终审查。

## 代码质量

- [ ] 所有 Debug 构建通过无警告（`-Wall -Wextra -Wpedantic` / `/W4`）
- [ ] 所有 Release 构建通过无警告
- [ ] `ctest --output-on-failure` 全部通过（Debug 与 Release）
  - [ ] `mwrender.smoke`
  - [ ] `mwrender.edge`
  - [ ] `mwrender.conformance`
  - [ ] `mwrender.stress`
  - [ ] `mwrender.security`
  - [ ] `mwrender.benchmark`（超时在合理范围内）
  - [ ] `mwrender.cli.version`
  - [ ] `mwrender.cli.themes`
  - [ ] `mwrender.cli.config`

## API 与 CLI 一致性

- [ ] 所有文档示例代码可编译（`examples/api_example.cpp`）
- [ ] CLI 选项与 `DevelopmentGuide.md §13` 一致
- [ ] `mwrender --help` 输出包含所有选项
- [ ] API `RenderRequest`/`RenderResult` 字段与 `DevelopmentGuide.md §12` 一致

## 文档同步

- [ ] `docs/COMPATIBILITY.md` 反映本次版本的实际支持状态
- [ ] `docs/DevelopmentGuide.md` 示例代码与实现一致
- [ ] `docs/ImplementationPlan.md` Phase 8 项目标记为完成
- [ ] `README.md` 版本号、安装说明与实际产物一致

## 安全审查

- [ ] 安全回归测试全部通过（`mwrender.security`）
- [ ] 无已知 XSS 向量（script/event handler/javascript:/data: URL）
- [ ] CSS 注入（@import/远程 url()）已验证阻断
- [ ] 主题/文档 CSS 路径穿越测试通过
- [ ] HTML 默认策略为 `Disabled`，Trusted 需显式选项

## Snapshot 稳定性

- [ ] 输出 HTML 结构确定性（相同输入 + 相同选项 → 相同输出）
- [ ] Slug 生成规则未变更（或有迁移说明）
- [ ] Source attributes 格式未变更（或有迁移说明）

## 安装消费验证

- [ ] `cmake --install build --prefix <dir>` 成功
- [ ] 安装消费示例（`build/consumer/mwrender_api_example.exe`）可运行
- [ ] `MWRenderConfig.cmake` 和 `MWRenderConfigVersion.cmake` 导出正确
- [ ] 安装的头文件与源码 `include/` 一致

## 性能基线

- [ ] `mwrender.benchmark` 在本机记录基线时间
- [ ] 1 MB 文档解析+渲染时间记录到 `docs/COMPATIBILITY.md`
- [ ] 与上一版本基线比较，无显著回退（> 2x）

## 版本号

- [ ] `CMakeLists.txt` 中 `project(MWRender VERSION x.y.z)` 已更新
- [ ] `mwrender --version` 输出与 CMake 版本一致
- [ ] `version.hpp` 的 `versionString` 与 CMake 版本一致

## 兼容性声明

- [ ] `docs/COMPATIBILITY.md` 列出本版本已支持语法
- [ ] 已知偏差（expected-fail）已列出
- [ ] 未支持特性（not-supported）已列出，无静默排除
- [ ] conformance test 中无意外失败（unexpected-fail = 0）

## 发布提交

- [ ] CHANGELOG 或发布说明包含本版本变更
- [ ] Git tag 与版本号一致（`v0.1.0`）
- [ ] 无遗留 TODO/FIXME 阻止发布

---

*本清单由 Phase 8 稳定化工作创建，适用于 MWRender v0.1+ 发布流程。*
