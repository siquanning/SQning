# AGENTS.md — 杭州麻将 (Hangzhou Mahjong) 项目开发指南

## 文件夹结构

| 文件夹 | 用途 | 规则 |
|--------|------|------|
| `src/` | **所有代码**（HTML、JS、CSS、游戏资源、测试脚本） | 代码只能放这里，不引用外部文件 |
| `src/assets/` | 游戏运行时资源（牌面 SVG、字体、桌面纹理） | 仅供 `src/` 内代码引用 |
| `docs/` | 产品文档（PRD、需求迭代、关键决策记录） | 所有文档、会议纪要、需求说明放这里 |
| `assets/design/` | 设计素材（效果图、UI 参考截图） | 供设计参考，代码不引用这里的内容 |
| `assets/bug/` | 测试报错截图 | 复现 bug 时截图存档于此 |
| `assets/reference/` | 参考图、灵感收集 | 外部参考素材 |
| `notes/` | 学习笔记（踩坑记录、技术方案总结） | 开发过程中学到的知识点归档 |

## 开发规则

1. **代码隔离** — 所有源代码、脚本、游戏资源一律放在 `src/`。代码区不引用项目外部的文件
2. **文档归档** — PRD、需求变更、技术决策文档统一放 `docs/`
3. **资源分类** — 设计稿放 `assets/design/`，bug 截图放 `assets/bug/`，参考图放 `assets/reference/`
4. **笔记沉淀** — 非显而易见的坑、技术选型理由写入 `notes/`
5. **根目录简洁** — 根目录只保留 `package.json`、`node_modules/`、`AGENTS.md`、`README.md`

## 项目技术概要

- **类型**：纯前端单页游戏，无框架，vanilla JS + CSS
- **入口**：直接用浏览器打开 `src/index.html` 即可运行
- **玩法**：杭州麻将（白板财神、吃碰杠、七对、爆头、杠开、飘财）
- **规则**：纯自摸（无点炮），飘财胡牌双倍计分
- **音效**：Web Audio API 合成，无外部音频文件
- **存档**：localStorage 自动保存/恢复（含飘财状态）
- **AI**：启发式评估 + 搭子识别 + 听牌质量评分 + 防守意识 + 记忆化递归决策

## 脚本说明

| 脚本 | 用途 | 运行方式 |
|------|------|----------|
| `src/extract-tiles.js` | 从 Mahjong.Colored.otf 字体中提取牌面 SVG | `node src/extract-tiles.js` |
| `src/smoke-test.js` | 游戏逻辑单元测试（Node vm 沙箱） | `node src/smoke-test.js` |
| `src/verify-ui.js` | Playwright UI 验证（简单流程） | `node src/verify-ui.js` |
| `src/ui-check.mjs` | Playwright UI 全面检查（ES module） | `node src/ui-check.mjs` |

## 修改代码注意事项

- `app.js` 中 `tileAssetPath()` 返回 `./assets/tiles/...`，移动代码时需确保相对路径正确
- `styles.css` 中 `url("./assets/table-pattern.svg")` 同样依赖相对路径
- AI 策略修改集中在 `app.js` 后半部分的评估函数（`evaluateShape`、`chooseAiDiscard` 等）
- 添加新牌型判定逻辑需同步更新 `smoke-test.js` 的测试用例
