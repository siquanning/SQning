// ============================================================
// coach-panel.js — 教练面板 UI 组件
// ============================================================
// 右侧侧边栏：可折叠/展开，拖拽左边缘调整宽度，移动端 FAB+抽屉。
// ============================================================

import { isCoachMode } from './mode-toggle.js';

const WIDTH_KEY = 'hangma_coach_panel_width';
const AUTO_MINIMIZE_KEY = 'hangma_coach_auto_minimize';
const DEFAULT_WIDTH = 320;
const MIN_WIDTH = 240;
const MAX_WIDTH = 480;
const MOBILE_BP = 820;

let _panel = null;       // #coachPanel DOM
let _width = parseInt(localStorage.getItem(WIDTH_KEY), 10) || DEFAULT_WIDTH;
let _collapsed = false;  // 用户手动折叠
let _autoMinimize = localStorage.getItem(AUTO_MINIMIZE_KEY) !== 'false';
let _minimizedForDraw = false; // 摸牌后自动最小化
let _messages = [];
let _visible = false;
let _isMobile = window.innerWidth < MOBILE_BP;

// DOM refs
let _container, _header, _title, _settingsBtn, _toggleBtn, _resizer, _body, _msgList;
let _fabBtn, _drawerEl;

// ---- 初始化和 DOM 构建 ----

export function initCoachPanel() {
  if (_panel) return;
  _createDOM();
  window.addEventListener('resize', _onViewportResize);
}

export function showCoachPanel() {
  _visible = true;
  _updateLayout();
}

export function hideCoachPanel() {
  _visible = false;
  _collapsed = false;
  _minimizedForDraw = false;
  _messages = [];
  _updateLayout();
  _renderMessages();
}

// ---- "先判断后对答案"机制 ----

/** 玩家摸牌 → 面板自动最小化 */
export function minimizeForDraw() {
  if (!_visible || !_autoMinimize || !isCoachMode()) return;
  _minimizedForDraw = true;
  _applyCollapseState();
}

/** 玩家弃牌 → 面板自动展开 */
export function expandAfterDiscard() {
  if (!_visible || !isCoachMode()) return;
  _minimizedForDraw = false;
  if (_collapsed) return;
  _applyCollapseState();
}

// ---- 消息管理 ----

/**
 * 添加教练消息。
 * @param {{ id?: string, type: string, priority: number, summary: string, reasoning?: string, tags?: string[] }} msg
 */
export function addCoachMessage(msg) {
  _messages.push({ ...msg, id: msg.id || `msg-${Date.now()}-${Math.random().toString(36).slice(2, 6)}`, timestamp: Date.now() });
  _renderMessages();
}

export function clearCoachMessages() {
  _messages = [];
  _renderMessages();
}

export function setCoachMessages(msgs) {
  _messages = msgs.map((m, i) => ({ ...m, id: m.id || `msg-${Date.now()}-${i}`, timestamp: Date.now() }));
  // If panel was auto-minimized for draw, expand to show pre-discard analysis
  if (_minimizedForDraw && msgs.length > 0) {
    _minimizedForDraw = false;
    _applyCollapseState();
  }
  _renderMessages();
}

// ---- 内部实现 ----

function _createDOM() {
  // 查找或创建面板容器
  const gameLayout = document.querySelector('.game-layout');
  if (!gameLayout) return;

  _panel = document.createElement('aside');
  _panel.id = 'coachPanel';
  _panel.className = 'coach-panel';
  _panel.style.display = 'none';
  _panel.style.setProperty('--coach-panel-width', `${_width}px`);

  _panel.innerHTML = `
    <div class="coach-panel-resizer" data-action="resize"></div>
    <div class="coach-panel-header">
      <h2 class="coach-panel-title">教练分析</h2>
      <div class="coach-panel-header-actions">
        <button class="coach-panel-icon-btn" data-action="settings" title="自动最小化设置">⚙</button>
        <button class="coach-panel-icon-btn" data-action="toggle" title="折叠面板">◀</button>
      </div>
    </div>
    <div class="coach-panel-body">
      <div class="coach-messages"></div>
    </div>
  `;

  gameLayout.appendChild(_panel);

  _container = _panel;
  _header = _panel.querySelector('.coach-panel-header');
  _title = _panel.querySelector('.coach-panel-title');
  _settingsBtn = _panel.querySelector('[data-action="settings"]');
  _toggleBtn = _panel.querySelector('[data-action="toggle"]');
  _resizer = _panel.querySelector('.coach-panel-resizer');
  _body = _panel.querySelector('.coach-panel-body');
  _msgList = _panel.querySelector('.coach-messages');

  // 事件绑定
  _settingsBtn.addEventListener('click', _toggleAutoMinimize);
  _toggleBtn.addEventListener('click', _toggleCollapse);
  _setupResizer();
  _updateSettingsIcon();
  _createMobileUI();
}

function _createMobileUI() {
  // 移动端 FAB 按钮
  _fabBtn = document.createElement('button');
  _fabBtn.id = 'coachFab';
  _fabBtn.type = 'button';
  _fabBtn.className = 'coach-fab';
  _fabBtn.textContent = '教';
  _fabBtn.title = '打开教练面板';
  _fabBtn.addEventListener('click', _openDrawer);
  document.body.appendChild(_fabBtn);

  // 移动端抽屉
  _drawerEl = document.createElement('div');
  _drawerEl.id = 'coachDrawer';
  _drawerEl.className = 'coach-drawer';
  _drawerEl.innerHTML = `
    <div class="coach-drawer-backdrop" data-action="close-drawer"></div>
    <div class="coach-drawer-sheet">
      <div class="coach-drawer-header">
        <h2>教练分析</h2>
        <button class="coach-drawer-close" data-action="close-drawer">✕</button>
      </div>
      <div class="coach-drawer-body">
        <div class="coach-messages"></div>
      </div>
    </div>
  `;
  document.body.appendChild(_drawerEl);

  _drawerEl.querySelector('[data-action="close-drawer"]').addEventListener('click', _closeDrawer);
  _drawerEl.querySelector('.coach-drawer-backdrop').addEventListener('click', _closeDrawer);

  _updateMobileState();
}

function _setupResizer() {
  let dragging = false;
  let startX = 0;
  let startWidth = 0;
  let pointerId = null;

  _resizer.addEventListener('pointerdown', (e) => {
    if (_isMobile) return;
    dragging = true;
    pointerId = e.pointerId;
    startX = e.clientX;
    startWidth = _width;
    _resizer.setPointerCapture(pointerId);
    _panel.classList.add('resizing');
    e.preventDefault();
  });

  _resizer.addEventListener('pointermove', (e) => {
    if (!dragging || e.pointerId !== pointerId) return;
    const dx = e.clientX - startX;
    _width = Math.min(MAX_WIDTH, Math.max(MIN_WIDTH, startWidth + dx));
    _panel.style.setProperty('--coach-panel-width', `${_width}px`);
    const gl = document.querySelector('.game-layout');
    if (gl) gl.style.setProperty('--right-column-width', `${_width}px`);
  });

  const endResize = (e) => {
    if (!dragging || e.pointerId !== pointerId) return;
    dragging = false;
    _panel.classList.remove('resizing');
    localStorage.setItem(WIDTH_KEY, String(_width));
    pointerId = null;
  };
  _resizer.addEventListener('pointerup', endResize);
  _resizer.addEventListener('pointercancel', endResize);
}

function _toggleCollapse() {
  if (_minimizedForDraw && _autoMinimize) {
    // 用户手动操作时优先用户意图
    _minimizedForDraw = false;
  }
  _collapsed = !_collapsed;
  _applyCollapseState();
}

function _applyCollapseState() {
  const active = _collapsed || _minimizedForDraw;
  _panel.classList.toggle('collapsed', active);
  _toggleBtn.textContent = _collapsed ? '▶' : '◀';
  _toggleBtn.title = _collapsed ? '展开面板' : '折叠面板';

  if (_minimizedForDraw && !_collapsed) {
    _title.textContent = '打出牌后显示分析';
    _panel.classList.add('waiting-discard');
  } else {
    _title.textContent = '教练分析';
    _panel.classList.remove('waiting-discard');
  }
}

function _toggleAutoMinimize() {
  _autoMinimize = !_autoMinimize;
  localStorage.setItem(AUTO_MINIMIZE_KEY, String(_autoMinimize));
  _updateSettingsIcon();

  if (!_autoMinimize && _minimizedForDraw) {
    _minimizedForDraw = false;
    _applyCollapseState();
  }
}

function _updateSettingsIcon() {
  if (!_autoMinimize) {
    _settingsBtn.textContent = '📌';
    _settingsBtn.title = '自动最小化：已关闭（始终展开）';
  } else {
    _settingsBtn.textContent = '⚙';
    _settingsBtn.title = '自动最小化：已开启（摸牌时收起）';
  }
}

function _renderMessages() {
  if (!_msgList) return;
  _msgList.innerHTML = '';

  if (_messages.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'coach-empty';
    empty.textContent = '教练正在准备分析……\n打出牌后会显示建议。';
    _msgList.appendChild(empty);
    return;
  }

  // 按优先级降序
  const sorted = [..._messages].sort((a, b) => b.priority - a.priority);
  sorted.forEach((msg) => {
    const card = _renderMessageCard(msg);
    _msgList.appendChild(card);
  });

  // 同步移动端抽屉消息
  _syncDrawerMessages();
}

function _renderMessageCard(msg) {
  const card = document.createElement('div');
  card.className = `coach-msg-card coach-msg-${msg.type}`;
  card.dataset.msgId = msg.id;

  const header = document.createElement('div');
  header.className = 'coach-msg-header';

  const typeLabel = _typeLabel(msg.type);
  const typeBadge = document.createElement('span');
  typeBadge.className = `coach-msg-badge badge-${msg.type}`;
  typeBadge.textContent = typeLabel;
  header.appendChild(typeBadge);

  const summary = document.createElement('p');
  summary.className = 'coach-msg-summary';
  summary.textContent = msg.summary;
  header.appendChild(summary);

  card.appendChild(header);

  if (msg.tags && msg.tags.length > 0) {
    const tagRow = document.createElement('div');
    tagRow.className = 'coach-msg-tags';
    msg.tags.forEach((tag) => {
      const tagEl = document.createElement('span');
      tagEl.className = 'coach-msg-tag';
      tagEl.textContent = tag;
      tagRow.appendChild(tagEl);
    });
    card.appendChild(tagRow);
  }

  if (msg.reasoning) {
    const detailBtn = document.createElement('button');
    detailBtn.className = 'coach-msg-expand';
    detailBtn.textContent = '展开推理';
    const detailBody = document.createElement('div');
    detailBody.className = 'coach-msg-detail';
    detailBody.textContent = msg.reasoning;
    detailBody.style.display = 'none';

    detailBtn.addEventListener('click', () => {
      const expanded = detailBody.style.display !== 'none';
      detailBody.style.display = expanded ? 'none' : 'block';
      detailBtn.textContent = expanded ? '收起推理' : '展开推理';
    });

    card.appendChild(detailBtn);
    card.appendChild(detailBody);
  }

  // 路径对比（选 A vs 选 B）
  if (msg.fullComparison) {
    const cmpBtn = document.createElement('button');
    cmpBtn.className = 'coach-msg-expand coach-msg-compare-btn';
    cmpBtn.textContent = '展开路径对比';
    const cmpBody = _renderComparison(msg.fullComparison);
    cmpBody.style.display = 'none';

    cmpBtn.addEventListener('click', () => {
      const expanded = cmpBody.style.display !== 'none';
      cmpBody.style.display = expanded ? 'none' : 'block';
      cmpBtn.textContent = expanded ? '收起路径对比' : '展开路径对比';
    });

    card.appendChild(cmpBtn);
    card.appendChild(cmpBody);
  }

  return card;
}

function _typeLabel(type) {
  const labels = { discard: '弃牌建议', threat: '威胁预警', timing: '时机提醒', direction: '方向分析', deep: '深度解析', confirm: '确认' };
  return labels[type] || type;
}

function _renderComparison(comparison) {
  const container = document.createElement('div');
  container.className = 'coach-msg-comparison';

  const renderOption = (opt, side) => {
    const el = document.createElement('div');
    el.className = `coach-compare-option coach-compare-${side}`;
    el.innerHTML = `
      <div class="coach-compare-label">${opt.label || ''}</div>
      <div class="coach-compare-winrate">胜率：${opt.winRate || '?'}</div>
      <div class="coach-compare-desc">${opt.desc || ''}</div>
    `;
    return el;
  };

  container.appendChild(renderOption(comparison.optionA, 'a'));
  container.appendChild(renderOption(comparison.optionB, 'b'));

  return container;
}

function _syncDrawerMessages() {
  if (!_drawerEl) return;
  const drawerMsgList = _drawerEl.querySelector('.coach-messages');
  if (!drawerMsgList) return;
  drawerMsgList.innerHTML = _msgList ? _msgList.innerHTML : '';
}

function _updateLayout() {
  if (!_panel) return;
  const shouldShow = _visible && isCoachMode();
  const infoPanel = document.querySelector('.info-panel');
  const gameLayout = document.querySelector('.game-layout');

  // 教练模式：隐藏牌局 panel，显示教练 panel（占同一列）
  if (infoPanel) {
    infoPanel.style.display = shouldShow ? 'none' : '';
  }
  _panel.style.display = shouldShow ? '' : 'none';

  // 动态设置右侧列宽
  if (gameLayout) {
    if (shouldShow && !_isMobile) {
      gameLayout.style.setProperty('--right-column-width', `${_width}px`);
    } else {
      gameLayout.style.setProperty('--right-column-width', '290px');
    }
  }

  _applyCollapseState();
  _updateMobileState();

  if (shouldShow && !_isMobile) {
    _panel.style.setProperty('--coach-panel-width', `${_width}px`);
  }
}

function _onViewportResize() {
  const wasMobile = _isMobile;
  _isMobile = window.innerWidth < MOBILE_BP;
  if (wasMobile !== _isMobile) {
    _updateMobileState();
    _updateLayout();
  }
}

function _updateMobileState() {
  if (!_fabBtn || !_drawerEl) return;
  const active = _visible && isCoachMode() && _isMobile;
  _fabBtn.style.display = active ? '' : 'none';
  // 如果是桌面模式，确保抽屉关闭
  if (!_isMobile) {
    _closeDrawer();
  }
  if (!_visible || !isCoachMode()) {
    _closeDrawer();
    _fabBtn.style.display = 'none';
  }
}

function _openDrawer() {
  if (!_drawerEl) return;
  _drawerEl.classList.add('open');
  _syncDrawerMessages();
}

function _closeDrawer() {
  if (!_drawerEl) return;
  _drawerEl.classList.remove('open');
}
