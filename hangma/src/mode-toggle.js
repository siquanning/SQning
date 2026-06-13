// ============================================================
// mode-toggle.js — 模式开关（普通模式 ↔ 教练模式）
// ============================================================

const COACH_MODE_KEY = 'hangma_coach_mode';

let _mode = (localStorage.getItem(COACH_MODE_KEY) || 'normal');
const _listeners = [];

/**
 * 获取当前模式：'normal' | 'coach'
 */
export function getCoachMode() {
  return _mode;
}

export function isCoachMode() {
  return _mode === 'coach';
}

/**
 * 设置模式，持久化到 localStorage，通知所有监听器。
 */
export function setCoachMode(mode) {
  if (mode !== 'normal' && mode !== 'coach') return;
  if (mode === _mode) return;
  _mode = mode;
  localStorage.setItem(COACH_MODE_KEY, mode);
  _listeners.forEach(fn => { try { fn(mode); } catch (_) {} });
}

export function toggleCoachMode() {
  setCoachMode(_mode === 'coach' ? 'normal' : 'coach');
}

/**
 * 注册模式变化监听器。返回取消订阅函数。
 */
export function onModeChange(fn) {
  _listeners.push(fn);
  return () => {
    const idx = _listeners.indexOf(fn);
    if (idx >= 0) _listeners.splice(idx, 1);
  };
}

/**
 * 创建模式切换按钮 DOM。调用方负责插入到合适位置。
 * @returns {HTMLButtonElement}
 */
export function createModeToggleButton() {
  const btn = document.createElement('button');
  btn.id = 'modeToggleBtn';
  btn.type = 'button';
  btn.className = 'mode-toggle-button';
  btn.title = _mode === 'coach' ? '切换到普通模式' : '切换到教练模式';
  _updateButtonDOM(btn);
  btn.addEventListener('click', () => {
    toggleCoachMode();
    _updateButtonDOM(btn);
  });
  onModeChange(() => _updateButtonDOM(btn));
  return btn;
}

function _updateButtonDOM(btn) {
  if (_mode === 'coach') {
    btn.className = 'mode-toggle-button coach-active';
    btn.textContent = '教练';
    btn.title = '切换到普通模式';
  } else {
    btn.className = 'mode-toggle-button';
    btn.textContent = '普通';
    btn.title = '切换到教练模式';
  }
}
