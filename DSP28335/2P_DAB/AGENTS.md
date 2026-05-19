# AGENTS.md — AI 开发助手指南

## 项目概述

基于 TI TMS320F28335 (C2000 DSP) 的 DAB（双有源桥）电源控制系统。

## 文件夹结构及用途

| 文件夹 | 用途 | 规则 |
|--------|------|------|
| `docs/` | 产品文档：PRD、需求迭代记录、关键决策 | 所有文档类文件必须放这里，禁止散落根目录 |
| `assets/design/` | 设计素材：效果图、UI 参考 | 仅存放设计相关图片/素材 |
| `assets/bug/` | 测试报错截图 | 按日期或 bug ID 命名 |
| `assets/reference/` | 参考图、灵感收集 | 用于技术方案对比和选型参考 |
| `notes/` | 学习笔记：踩坑记录、技术方案总结 | 每个主题一个文件，文件名描述主题 |
| 根目录代码区 | CCS 工程源码（`.c`/`.h`/`.syscfg`） | 代码自包含，不引用文档区文件 |

## 开发规则

1. **代码隔离** — 代码区不引用 `docs/`、`assets/`、`notes/` 中的文件；代码自包含
2. **文档集中** — 所有设计文档、需求变更统一放 `docs/`，禁止在代码目录内创建文档
3. **笔记规范** — `notes/` 下每篇笔记开头注明日期和主题摘要；代码片段标注来源文件
4. **资源分类** — 设计图、Bug 截图、参考图严格分目录，不混放
5. **CCS 交互** — 使用 CCS MCP 工具前，必须先读取 `.claude/ccs.settings.md` 获取安装路径，再读取 CCS 安装目录下的 `ccs/theia/resources/ai/CCS.md`
6. **SysConfig** — 修改 `.syscfg` 文件必须通过 CCS SysConfig MCP 工具，禁止直接编辑该文件

## 技术栈

- **MCU**: TI TMS320F28335 (C2000 系列)
- **IDE**: TI Code Composer Studio (CCS) + SysConfig
- **编译器**: TI C2000 Compiler
- **通信协议**: SCI (UART)、Modbus、CAN
- **功率拓扑**: DAB (Dual Active Bridge)、EPWM 控制
