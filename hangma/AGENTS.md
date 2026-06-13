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

- **类型**：纯前端单页游戏，无框架，vanilla JS + CSS + ES Modules
- **入口**：需通过 HTTP 服务打开 `src/index.html`（ES Modules 不支持 file:// 协议）
- **架构**：UI 层 (`app.js`) + 引擎层 (`engine.js`)，通过 ES Module import 解耦
- **玩法**：杭州麻将（白板财神、吃碰杠、七对、爆头、杠开、飘财）—— 见 [PRD](E:/CLAUDE/hangzhou_majiang/docs/PRD.md)
- **规则**：纯自摸（无点炮），庄家/闲家不同计分，详见 PRD
- **音效**：Web Audio API 合成，无外部音频文件
- **存档**：localStorage 自动保存/恢复（含飘财状态）

## 核心架构

```
src/
├── index.html          # 入口 HTML
├── styles.css          # 全部样式
├── app.js              # UI 层：渲染、音效、输入、游戏流程编排
├── engine.js           # 引擎层：[接口文档 + Stub 桩] → 待你实现实际逻辑
├── assets/tiles/*.svg  # 牌面 SVG 资源
└── ...
```

### app.js（UI 层）职责
- 所有 DOM 渲染（render* 函数）
- 音效管理（AudioManager）
- 粒子特效（ParticleSpawner）
- 手势/键盘输入
- 游戏流程编排（回合推进、吃碰杠执行、胡牌结算、存档恢复）

### engine.js（引擎层）职责
- 牌具操作：createTile, tileKey, tileLabel, sortTiles 等
- 游戏状态创建：createNewGame, createPlayers, createWall
- 规则判定：isWinningHand, getWaitTiles, getChiOptions, getConcealedGangChoices
- 出牌提示：getDiscardHints
- AI 决策：chooseDiscard, shouldPeng, shouldMeldGang, chooseChi, chooseConcealedGang, shouldPiaoCai, resolveClaim
- 计分：calculateWinScore

### 当前状态：Stub 模式
`engine.js` 当前为桩（Stub）实现，所有规则判定返回安全默认值（false/空），UI 可正常渲染空桌。你需要按照 `engine.js` 中的 `[TODO: IMPLEMENT]` 标记和 [PRD](E:/CLAUDE/hangzhou_majiang/docs/PRD.md) 实现实际逻辑。

## 本地运行

```powershell
# 在 src/ 目录启动静态服务（ES Modules 需要 HTTP）
npx serve src/
# 或
cd src; npx http-server -p 8080 -c-1
```

## 脚本说明

| 脚本 | 用途 | 运行方式 |
|------|------|----------|
| `src/extract-tiles.js` | 从 Mahjong.Colored.otf 字体中提取牌面 SVG | `node src/extract-tiles.js` |
| `src/smoke-test.js` | 游戏逻辑单元测试（Node vm 沙箱）— 需随引擎实现同步更新 | `node src/smoke-test.js` |
| `src/verify-ui.js` | Playwright UI 验证（简单流程） | `node src/verify-ui.js` |
| `src/ui-check.mjs` | Playwright UI 全面检查（ES module） | `node src/ui-check.mjs` |

## 修改代码注意事项

- 规则/AI 逻辑全部在 `engine.js` 中实现，不要写入 `app.js`
- `app.js` 中的流程编排函数（discardPlayerTile、checkClaimsAfterDiscard 等）依赖 engine 返回值做分支，修改 engine 接口时需同步更新 app.js 的调用处
- `tileAssetPath()` 返回 `./assets/tiles/...`，移动代码时需确保相对路径正确
- `styles.css` 中 `url("./assets/table-pattern.svg")` 同样依赖相对路径
