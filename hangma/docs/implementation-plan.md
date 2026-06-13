# 杭州麻将引擎 — 分步实现计划

> 每步独立可测，完成后在 `[ ]` 中打 `[x]`。
> 核心实现在 `src/engine.js`，AI 引擎在 `src/ai/`，教练模块在 `src/coach/`。

---

## 第1步：牌池与发牌

**目标：运行后能看到 4 家手牌。**

- [x] `createTilePool()` — 136张牌（万/筒/条 1-9×4 + 7字牌×4），Fisher-Yates 洗牌（engine 内部函数，不导出）
- [x] `createNewGame()` — 调用 createTilePool / createPlayers / 发牌（庄14张闲13张）/ 定财神（白板）/ 返回完整 GameState
- [x] 验证：浏览器打开能看到手牌、剩余牌数显示正确

**参考 PRD 章节：** 1.1 牌具、1.2 座次、1.3 行牌（庄14闲13）

---

## 第2步：胡牌判定

**目标：手牌能正确判断是否胡牌。**

- [x] 辅助函数 `getCanonicalCounts(tiles, wildKey)` — 统计每种牌的数量，财神不计入 count 全部计入 wildCount
- [x] `canFormSets(counts, wildCount)` — 判断剩余牌能否组成 4 面子（顺子/刻子），财神可替代任意牌。带记忆化
- [x] `isStandardWin(tiles, wildKey)` — 4面子+1将（财神可用于面子或参与将牌）
- [x] `isSevenPairs(tiles, wildKey)` — 7对子判定
- [x] `isWinningHand(tiles, wildKey)` — 串联上述两种 + 最终入口

**参考 PRD 章节：** 1.6 胡牌条件、2.1 牌型列表

---

## 第3步：听牌 & 出牌提示

**目标：侧边栏能显示听什么牌、打哪张能听。**

- [x] 辅助：生成 34 种 distinct tile 列表
- [x] `getWaitTiles(tiles, wildKey)` — 13张手牌 + 每种可能的进张 → 判断是否胡
- [x] `getDiscardHints(hand, wildKey)` — 每张手牌打出后 → 计算听牌，按听牌数降序返回

**参考 PRD 章节：** 3.1 爆头（听牌状态判定）

---

## 第4步：吃碰杠选项

**目标：打出牌后能弹出吃/碰/杠提示。**

- [x] `getChiOptions(hand, discardTile, wildKey)` — 上家弃牌后能吃的方式（吃只能吃上家，庄下家吃庄需先亮财神）
- [x] `getPengOptions(hand, tile, wildKey)` — 碰/明杠判定，≥3张优先杠
- [x] `getConcealedGangChoices(hand, wildKey)` — 手中4张相同非财神 → 可暗杠
- [x] `groupHandByKey(hand, wildKey)` — 共享分组辅助函数，消除重复代码
- [x] `庄下家吃庄需先亮财神` — app.js L817 强制执行
- [x] `app.js` checkClaimsAfterDiscard 重构为使用 getPengOptions
- [x] 验证：22 unit tests + win 判定 27 测试全部通过

**参考 PRD 章节：** 1.3 行牌（碰/杠 > 吃）、1.4 杠

---

## 第5步：计分系统

**目标：胡牌后正确计算番数和得分。**

- [x] 12种牌型判定函数（每个牌型一个独立判定）：
  - [x] 平胡、七对、豪华七对、十三幺
  - [x] 杠上开花、爆头、天胡、地胡、大四喜、小四喜、大三元、小三元
  - [ ] 碰碰胡、清一色、混一色、清龙（暂不实现）
- [x] `calculateWinScore(...)` — 牌型叠加乘法 + 庄家/闲家不同计分 + 飘财倍数

**参考 PRD 章节：** 二、牌型 / 五、计分规则

---

## 第6步：ISMCTS AI 引擎（对手 AI）

**目标：三家 AI 用 ISMCTS 做决策，战术深度达到"会玩的人觉得难对付"的水平。**

> 这是整个项目最核心的步骤。ISMCTS = 数万次模拟推演 → 选胜率最高的动作。

### 6a. 快速游戏模拟器 (`src/ai/game-sim.js`)

- [x] **位图 GameState**：136 张牌用 `Uint8Array(34)` TileSet 表示
  - [x] 牌池 / 手牌 / 副露 / 弃牌河 各用 TileSet 表示
  - [x] `clone()` 操作 < 1μs（实测 0.96μs，`new Uint8Array(old)`）
  - [x] 摸牌/弃牌/吃碰杠 在 TileSet 上快速操作
- [x] **Shanten 计算**：带记忆化的递归搜索，含财神处理
  - [x] 查询近乎 O(1)（memo 命中），含财神处理
  - [x] 实现方式：memoized recursive meld evaluation（非预计算查表）
- [x] **贪婪 Rollout 策略**：
  - [x] 每步遍历手牌，选 Shanten 下降最快的动作
  - [x] 吃/碰/杠判断：操作后 Shanten 是否下降
  - [x] 终局判定：`isWinningHandSet()` / 流局
  - [x] 单局模拟速度：464 局/秒（500 rollouts, 1077ms）
- [x] **模拟结果**：返回哪家胡（winner/winKind）、流局（drawReason）、reward 计算

### 6b. ISMCTS 搜索核心 (`src/ai/ismcts.js`)

- [x] **Determinization（确定化采样）**：
  - [x] 将未知信息（对手手牌 + 牌墙顺序）随机赋值，满足已观察约束
  - [x] 对手手牌：从剩余牌池中随机分配 `hand.length` 张
  - [x] 牌墙顺序：随机抽取（drawRandom），等价于洗牌
  - [x] 迭代次数可配置（iterations 参数 + timeLimit）
- [x] **MCTS 四步**（每个迭代）：
  - [x] Selection — UCB1 公式选择最有探索价值的子节点
  - [x] Expansion — 展开合法动作（弃牌 + 吃碰杠 claim）
  - [x] Rollout — 调用 game-sim 的贪婪策略模拟到终局
  - [x] Backprop — 胜负结果反向传播到根节点
- [x] **结果聚合**：所有迭代的访问计数 + 胜率统计 → 每个候选动作的期望胜率
- [x] **动作选择**：选平均胜率最高的动作（平局时选访问次数最多的）
- [x] **树节点复用**：ISMCTSNode 以 parent/children 树结构组织，支持子树保留
- [x] `chooseDiscardISMCTS()` — 高层接口，自动从 SimGameState 选择最佳弃牌
- [x] `shouldClaimISMCTS()` — 碰/杠/吃决策，基于向听数变化 + 手牌结构判断
- [x] `prepareSimState()` — 从 engine.js GameState 转换为 SimGameState

### 6c. 对手信念模型 (`src/ai/opponent-model.js`)

- [x] **花色分布推断**：根据对手弃牌序列，估计其各花色持有比例（suitProbabilities）
  - [x] 基于弃牌比例 vs 期望比例的偏差计算
  - [x] 副露修正：如果某花色已副露 → 提高该花色持有概率
- [x] **面子反推**：对手吃碰杠暴露的牌 → 排除已见牌（knownTileIndices）
- [x] **听牌概率估计**：4 路信号加权
  - [x] 近期弃牌的花色模式变化（突然弃原来留的花色）
  - [x] 弃中张（rank 3-7）作为听牌信号
  - [x] 游戏进度因子（turnFactor，后期概率更高）
  - [x] 副露数量因子（meldFactor，副露越多越接近听牌）
- [x] **Determinization 采样偏置**：`determinizeBiased()` — 对手手牌按信念权重采样
- [x] **增量更新**：每次 `recordDiscard`/`recordMeld` → 增量更新信念
- [x] **安全牌分析**：`getTileDanger()` / `getMostDangerous()` / `scoreAllTiles()`
- [x] `MultiPlayerModel` — 管理 3 个对手的模型，支持从 game state 初始化

### 6d. 难度分档 (`src/ai/difficulty.js`)

> **交接：** 调用 `chooseDiscardISMCTS(simState, playerId, iterations)` 时传入对应迭代数即可。
> 随机动作：概率直接返回 `chooseBestDiscard()` 的贪婪结果，跳过 ISMCTS。
> 关键 import：`import { chooseDiscardISMCTS, shouldClaimISMCTS, prepareSimState } from './ismcts.js'`
> Tile index 0-33: c1-9→0-8, d1-9→9-17, b1-9→18-26, honor→27-33 (33=白板财神)
> engine→sim 转换：`prepareSimState(game)` | UI tile→index：`tileTypeIndexFromGame(tile)`

- [x] 简单：500 次模拟/决策 + 15% 概率随机动作（模拟人类新手）
- [x] 普通：1500 次模拟/决策 + 5% 概率随机动作
- [x] 困难：3000 次模拟/决策 + 0% 随机

### 6e. Web Worker 集成 (`src/ai/ai-worker.js`)

- [x] AI Worker 独立线程运行 ISMCTS
- [x] `app.js` 通过 `postMessage` 发送 `{ type: "chooseDiscard", hand, playerId, gameState, difficulty }`
- [x] Worker 返回 `{ action: "discard", tile, stats }` 等
- [x] 超时 1s 兜底 → 降级为 Shanten 贪婪策略

### 6f. engine.js 挂接

- [ ] `chooseDiscard` → 委托 `aiWorker.chooseDiscard()`
- [ ] `shouldPeng` / `shouldMeldGang` → 委托 `aiWorker.shouldClaim()`（模拟"碰 vs 不碰"两条路径比胜率）
- [ ] `chooseChi` → 委托 `aiWorker.chooseChi()`
- [ ] `chooseConcealedGang` → 委托 `aiWorker.chooseConcealedGang()`
- [ ] `shouldPiaoCai` → 委托 `aiWorker.shouldPiaoCai()`（模拟"飘 vs 直接胡"比胜率）
- [ ] `resolveClaim` → 各候选方独立决策 + 优先级规则

**参考 PRD 章节：** 十一、对手 AI — ISMCTS 搜索引擎

---

## 第7步：特殊规则

**目标：四风连打 + 杠后补牌规则。**

- [x] 四风连打检测 — 开局后四家首弃牌均为同一种风牌 → 庄掷骰子付分，本局重开
- [x] 杠后补牌 — 从牌池倒数第二张补（倒数第一张翻开公示）
- [x] 流局处理 — 牌池摸完无人胡 → 原庄连庄

**参考 PRD 章节：** 1.4 杠 / 1.5 流局 / 四、四风连打

---

## 第8步：飘财完整流程

**目标：飘财的完整流程正确运作。**

- [x] 飘财触发：手中≥2张财神 + 处于听牌状态
- [x] 飘财后：其他三家各摸打一轮，期间不能吃碰明杠，但可暗杠/自摸胡
- [x] 飘财者轮回后：摸牌自动爆头胡
- [x] 多飘（二飘/三飘）倍数正确叠加

**参考 PRD 章节：** 三、爆头&飘财

---

## 第9步：测试 & 收尾

- [x] 更新 `src/smoke-test.js` — 覆盖所有牌型判定 + 计分 + 边界条件（300 tests）
- [x] AI 对局测试：跑 100 局 AI vs AI，验证无报错、决策合理（96%胡牌率，4%流局）
- [x] 修复发现的问题（飘财倍率+爆头flag联动）
- [x] 胡牌判定修复 — canFormSets 新增"财神放在当前牌前组顺子"分支（修复 56789筒+财神 无法胡牌）
- [x] 副露不可拆修复 — isWinningWithMelds 改为手牌独立组成 remainingSets 面子，副露不可与手牌重组
- [x] 庄家轮转修复 — declareWin 中设置 _nextDealer（庄家胡牌连庄，闲家胡牌轮庄）
- [x] 分数跨局累积 — startNewGame 保留上局分数
- [x] 手动理牌功能 — 水平拖拽重排手牌，新牌进头端，任何时候可用
- [x] 听牌建议折叠 — 默认折叠，点击展开，状态持久化
- [x] 去除末张牌 UI 显示

---

## 第10步：教练面板 UI

**目标：右侧教练面板能折叠/展开/拖拽，移动端自适应。**

- [ ] `CoachPanel` UI 组件 — `src/coach-panel.js`（新文件）
  - [ ] 右侧侧边栏，默认宽度 320px，CSS 变量可控
  - [ ] 拖拽左边缘调整宽度（pointer 拖拽，min 240px / max 480px）
  - [ ] 折叠/展开按钮（保留标题栏 40px）
  - [ ] 移动端（<820px）：自动切换为右下角悬浮按钮 + 抽屉弹窗
- [ ] 模式开关 — `src/mode-toggle.js`（新文件）
  - [ ] 开局前选择"普通模式"或"教练模式"
  - [ ] 对局中可切换（不影响牌局进程）
  - [ ] 设置持久化到 localStorage
- [ ] "先判断后对答案"机制
  - [ ] 玩家摸牌 → 面板自动最小化（标题栏显示"打出牌后显示分析"）
  - [ ] 玩家弃牌 → 面板自动展开，展示本轮教练消息
  - [ ] 设置中可切换为"始终展开"
- [ ] `app.js` 集成：在 `drawTileForPlayer` 和 `discardPlayerTile` 中触发面板状态切换

**参考 PRD 章节：** 8.2 启动与开关、8.3 辅助分析面板 UI

---

## 第11步：消息流系统

**目标：教练消息能以优先级排序、可展开、对比玩家和 AI 的决策。**

- [ ] 消息数据结构 — `src/coach/coach-messages.js`（新文件）
  ```js
  { id, type, priority, summary, reasoning, fullComparison, tags, timestamp }
  ```
- [ ] 5 类消息生成函数
  - [ ] `buildDiscardSuggestion(searchStats)` — 从 ISMCTS 搜索统计提取弃牌建议
  - [ ] `buildDirectionAnalysis(searchStats)` — 从搜索树路径提取方向分析
  - [ ] `buildThreatWarning(threatInfo)` — 从对手信念模型提取威胁预警
  - [ ] `buildTimingReminder(opportunityType)` — 时机提醒（爆头/飘财窗口）
  - [ ] `buildDeepAnalysis(searchStats)` — 深度解析（搜索树完整展开）
- [ ] 消息优先级队列
  - [ ] 威胁预警 > 时机提醒 > 弃牌建议 > 方向分析
  - [ ] 高优先级消息即时弹窗浮层（即使面板最小化）
  - [ ] 低优先级消息排队，每轮最多展示 3 条
- [ ] 消息卡片 UI
  - [ ] 一句话结论（始终可见，含胜率/听牌进度等量化信息）
  - [ ] 点击展开推理过程（自然语言 + 知识标签）
  - [ ] 关键节点可进一步展开为路径对比（选 A vs 选 B 的胜率和牌型对比）
- [ ] 弃牌对比模式
  - [ ] 玩家与 ISMCTS TOP-1 一致 → 简略确认 + 一句要点
  - [ ] 玩家与 ISMCTS TOP-1 不同 → 完整推理，展示胜率差异原因

**参考 PRD 章节：** 8.4 辅助分析消息流

---

## 第12步：搜索解释器 + NL 引擎（教练 AI 核心）

**目标：从 ISMCTS 搜索树中提取可读洞察，翻译成自然语言教练消息。**

> 此步骤替代旧方案中的"手牌评估器 + 弃牌推荐器 + 策略分析器"。
> 教练不再自己算权重打分——直接跑 ISMCTS 从玩家视角搜索，然后解释搜索结果。

### 12a. 搜索解释器 (`src/coach/search-interpreter.js`)

- [ ] **TOP-K 提取**：从搜索树中提取访问次数最多的 K 个候选动作及其模拟胜率
- [ ] **威胁检测**：分析对手信念模型，当某对手听牌概率 > 阈值时标记
  - [ ] 检测阈值分两档：60%（可能听牌）、85%（大概率听牌）
  - [ ] 结合安全牌分析：标注哪些候选牌对哪些对手危险
- [ ] **方向分歧识别**：
  - [ ] 两条路径胜率接近（差 < 5%）但牌型不同 → 关键分岔口
  - [ ] 检测大牌 vs 平胡速听的胜率差异
- [ ] **飘财窗口判断**：飘财模拟 vs 直接胡模拟的期望得分对比
- [ ] **搜索置信度**：模拟访问次数不足/结果方差大 → 标注"仅供参考"

### 12b. 自然语言解释器 (`src/coach/nl-explainer.js`)

- [ ] **搜索统计 → 自然语言映射**：
  - [ ] 胜率 60-70% → "有把握"
  - [ ] 胜率 45-55% → "五五开，需要权衡"
  - [ ] 访问次数/方差 → "结论可靠" / "数据不足，仅供参考"
- [ ] **5 类消息模板库**（每类 5-10 个模板，轮换避免重复）
- [ ] **上下文拼接引擎**：手牌摘要 + 胜率排序 + 对手状态 → 完整句子
- [ ] **知识标签附加**：`[牌效基础] [方向判断] [防守博弈] [概率推演]`
- [ ] **措辞规范检查**：
  - [ ] 用"建议""推荐"，不用"必须""一定"
  - [ ] 概率用高/中/低三级，胜率数字精确到 5% 档位
  - [ ] 术语附简短解释
  - [ ] 避免评价性语言

### 12c. Coach Worker (`src/coach/coach-worker.js`)

- [ ] 独立 Web Worker 运行 ISMCTS + 搜索解释器
- [ ] 与 AI Worker 共享 `game-sim.js` + `ismcts.js` + `opponent-model.js`
- [ ] 支持三种分析深度：简略（<100ms）/ 标准（<300ms）/ 深度（<1s）
- [ ] 超时兜底：深度模式超时返回标准模式结果

**参考 PRD 章节：** 十、教练 AI 技术架构

---

## 第13步：战后复盘 & 玩家画像

**目标：每局后可复盘，长期数据形成玩家档案。**

- [ ] 局后复盘报告 — `src/coach/replay-report.js`（新文件）
  - [ ] 本局数据概览（胡牌者、倍率、得分、轮数）
  - [ ] 关键决策点标记（玩家选择 vs ISMCTS TOP-1 不一致的时刻）
  - [ ] 决策对比表（你的选择 vs ISMCTS 推荐，附胜率差异和模拟依据）
  - [ ] 亮点记录（玩家与 ISMCTS 一致且最终受益的操作）
- [ ] 牌谱存储 — `src/coach/game-records.js`（新文件）
  - [ ] IndexedDB 封装（CRUD + 按月分库）
  - [ ] 牌谱 JSON 结构（基本信息 + 初始手牌 + 每步操作 + ISMCTS 搜索快照 + 结果）
  - [ ] 导出/导入 JSON 文件
  - [ ] 容量管理（> 1000 局时自动清理旧局）
- [ ] 玩家画像 — `src/coach/player-profile.js`（新文件）
  - [ ] 5 个维度统计：贪大牌倾向、防守风格、弃牌效率、牌型偏好、财神利用率
  - [ ] 对比基准：玩家的实际选择 vs ISMCTS 推荐（偏差率 = 画像关键指标）
  - [ ] 增量更新（新局结束后重新统计）
- [ ] 习惯识别 & 针对性提示
  - [ ] 4 类不良倾向检测（贪大牌、防守消极、拆搭错误、财神保守）
  - [ ] 触发阈值配置化
  - [ ] 提示注入到第 11 步的消息流中
- [ ] 数据本地化：所有数据存 IndexedDB，用户可随时清除/导出

**参考 PRD 章节：** 九、战后复盘与习惯学习

---

## 第14步：教练模式集成 & 收尾测试

- [ ] 教练模式 App 集成
  - [ ] `app.js` 中新增 `coachMode` 标志位
  - [ ] 玩家回合开始 → Coach Worker 后台跑 ISMCTS → 生成消息
  - [ ] 玩家摸牌 → 面板最小化，消息在后台生成
  - [ ] 玩家弃牌 → 面板展开 + 弃牌对比（你的选择 vs ISMCTS 推荐）
  - [ ] 其他玩家操作后 → 威胁模型增量更新 → 如有威胁则即时弹窗
- [ ] 性能验证
  - [ ] ISMCTS 单次决策 < 1s（困难）
  - [ ] 教练简略分析 < 100ms
  - [ ] 教练标准分析 < 300ms
  - [ ] 面板首次渲染 < 100ms
  - [ ] 内存占用（AI引擎+教练+历史数据）< 50MB
- [ ] 手动测试
  - [ ] 跑 10 局教练模式 — 面板交互 + 消息质量 + 复盘报告
  - [ ] 跑 20 局 AI vs AI — 验证 AI 决策统计合理性
  - [ ] 移动端适配测试（375px 宽）
  - [ ] IndexedDB 导出/导入/清除
- [ ] 更新 `smoke-test.js` — 覆盖 ISMCTS 关键路径 + 教练消息生成

---

## 依赖关系图

```
Phase 1 (基础引擎 + ISMCTS AI):
  第1步 (牌池发牌)
    ├─→ 第2步 (胡牌判定) ← 关键！ISMCTS rollout 需要
    │     ├─→ 第3步 (听牌提示 ← Shanten 基础)
    │     └─→ 第5步 (计分系统 ← ISMCTS reward 信号)
    ├─→ 第4步 (吃碰杠选项)
    └─→ 第6步 (ISMCTS AI) ← 需要 2 + 3 + 4 + 5 全部完成
          ├─ 6a (game-sim) ← 需要 2 (胡牌判定)
          ├─ 6b (ismcts core) ← 需要 6a
          ├─ 6c (opponent-model) ← 可并行于 6b
          ├─ 6d (difficulty) ← 需要 6b
          ├─ 6e (ai-worker) ← 需要 6b + 6c + 6d
          └─ 6f (engine 挂接) ← 需要 6e
  第7步 (特殊规则) ← 可并行于 6
  第8步 (飘财流程) ← 需要 2 + 3 + 6
  第9步 (测试收尾) ← 1-8 全部完成

Phase 2 (教练模式):
  第10步 (教练面板 UI) ← 需要基础引擎可运行
    └─→ 第11步 (消息流系统) ← 需要 10
          └─→ 第12步 (搜索解释器+NL) ← 需要 6 + 11
                ├─ 12a (search-interpreter) ← 需要 6b (ISMCTS 搜索树结构)
                ├─ 12b (nl-explainer) ← 需要 12a
                └─ 12c (coach-worker) ← 需要 12a + 12b
  第13步 (复盘+画像) ← 可并行于 12，需要 6 (ISMCTS 快照)
  第14步 (集成收尾) ← 10-13 全部完成
```

**建议执行顺序：**
- Phase 1: 1 → 2 → 3+4+5 并行 → 6a → 6b+6c 并行 → 6d+6e → 6f → 7+8 → 9
- Phase 2: 10 → 11 → 12+13 并行 → 14

**关键变更（vs 旧版计划）：**
1. AI 决策从静态加权打分 → ISMCTS 蒙特卡洛搜索
2. 教练 AI 不再有独立的 HandEvaluator/DiscardRecommender → 改为搜索解释器 + NL 引擎
3. 对手 AI 和教练 AI 共享同一 ISMCTS 引擎，教练只是多了"解释"层
4. 新增 `src/ai/` 目录（game-sim + ismcts + opponent-model + difficulty + ai-worker）
