const state = {
  settings: null,
  sessions: [],
  activeSessionId: null,
  runningId: null,
  currentAssistantId: null,
  activity: []
};

const el = {
  healthText: document.getElementById("healthText"),
  sessionList: document.getElementById("sessionList"),
  newSessionButton: document.getElementById("newSessionButton"),
  workspaceInput: document.getElementById("workspaceInput"),
  modelInput: document.getElementById("modelInput"),
  sandboxSelect: document.getElementById("sandboxSelect"),
  approvalSelect: document.getElementById("approvalSelect"),
  bypassInput: document.getElementById("bypassInput"),
  saveSettingsButton: document.getElementById("saveSettingsButton"),
  chatTitle: document.getElementById("chatTitle"),
  runStatus: document.getElementById("runStatus"),
  stopButton: document.getElementById("stopButton"),
  deleteSessionButton: document.getElementById("deleteSessionButton"),
  conversation: document.getElementById("conversation"),
  activityPanel: document.getElementById("activityPanel"),
  composer: document.getElementById("composer"),
  messageInput: document.getElementById("messageInput"),
  sendButton: document.getElementById("sendButton"),
  toast: document.getElementById("toast")
};

function activeSession() {
  return state.sessions.find((session) => session.id === state.activeSessionId) || null;
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function renderMarkdown(text) {
  const source = String(text || "");
  const parts = source.split(/```/);
  return parts
    .map((part, index) => {
      if (index % 2 === 1) {
        const lines = part.replace(/^\w+\n/, "").trimEnd();
        return `<pre><code>${escapeHtml(lines)}</code></pre>`;
      }
      return part
        .split(/\n{2,}/)
        .filter((block) => block.length > 0)
        .map((block) => `<p>${escapeHtml(block).replace(/\n/g, "<br>")}</p>`)
        .join("");
    })
    .join("");
}

function formatTime(value) {
  if (!value) return "";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return "";
  return date.toLocaleString("zh-CN", {
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit"
  });
}

function showToast(message) {
  el.toast.textContent = message;
  el.toast.classList.add("show");
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => {
    el.toast.classList.remove("show");
  }, 2600);
}

function setRunning(running, statusText = running ? "运行中" : "空闲") {
  el.runStatus.textContent = statusText;
  el.runStatus.classList.toggle("running", running);
  el.runStatus.classList.toggle("error", statusText === "错误");
  el.stopButton.disabled = !running;
  el.sendButton.disabled = running;
}

function syncSettingsForm() {
  const settings = state.settings || {};
  el.workspaceInput.value = settings.workspace || "";
  el.modelInput.value = settings.model || "";
  el.sandboxSelect.value = settings.sandbox || "workspace-write";
  el.approvalSelect.value = settings.approval || "never";
  el.bypassInput.checked = Boolean(settings.bypassSandbox);
}

function readSettingsForm() {
  return {
    workspace: el.workspaceInput.value.trim(),
    model: el.modelInput.value.trim(),
    sandbox: el.sandboxSelect.value,
    approval: el.approvalSelect.value,
    bypassSandbox: el.bypassInput.checked
  };
}

function renderSessions() {
  if (!state.sessions.length) {
    el.sessionList.innerHTML = "";
    return;
  }

  el.sessionList.innerHTML = state.sessions
    .map((session) => {
      const active = session.id === state.activeSessionId ? " active" : "";
      const count = Array.isArray(session.messages) ? session.messages.length : 0;
      return `
        <button class="session-item${active}" type="button" data-session-id="${escapeHtml(session.id)}">
          <span class="session-name">${escapeHtml(session.title || "新会话")}</span>
          <span class="session-time">${count ? count : ""}</span>
          <span class="session-time">${formatTime(session.updatedAt)}</span>
          <span></span>
        </button>
      `;
    })
    .join("");
}

function renderConversation() {
  const session = activeSession();
  el.chatTitle.textContent = session ? session.title || "新会话" : "新会话";

  if (!session || !session.messages || !session.messages.length) {
    el.conversation.innerHTML = `<div class="empty-state">开始一个 Codex 任务</div>`;
    renderActivity([]);
    return;
  }

  el.conversation.innerHTML = session.messages
    .map((message) => {
      const isUser = message.role === "user";
      const label = isUser ? "你" : "Codex";
      const avatar = isUser ? "你" : "C";
      const status = message.status === "running" ? "运行中" : formatTime(message.createdAt);
      return `
        <article class="message ${isUser ? "user" : "assistant"}" data-message-id="${escapeHtml(message.id)}">
          <div class="avatar" aria-hidden="true">${avatar}</div>
          <div class="bubble">
            <div class="bubble-header">
              <span>${label}</span>
              <span>${escapeHtml(status || "")}</span>
            </div>
            <div class="bubble-content">${renderMarkdown(message.content || "")}</div>
          </div>
        </article>
      `;
    })
    .join("");

  const lastAssistant = [...session.messages].reverse().find((message) => message.role === "assistant");
  renderActivity(lastAssistant && lastAssistant.events ? lastAssistant.events : state.activity);
  el.conversation.scrollTop = el.conversation.scrollHeight;
}

function renderActivity(items) {
  const events = (items || []).slice(-40);
  if (!events.length) {
    el.activityPanel.innerHTML = "";
    return;
  }
  el.activityPanel.innerHTML = events
    .map((item) => `
      <div class="activity-row ${escapeHtml(item.tone || "")}">
        <strong>${escapeHtml(item.title || "事件")}</strong>
        <span>${escapeHtml(item.text || "")}</span>
      </div>
    `)
    .join("");
  el.activityPanel.scrollTop = el.activityPanel.scrollHeight;
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });
  if (!response.ok) {
    let message = `HTTP ${response.status}`;
    try {
      const payload = await response.json();
      message = payload.error || message;
    } catch {
      message = await response.text();
    }
    throw new Error(message);
  }
  return response.json();
}

async function loadInitialData() {
  try {
    const [health, data] = await Promise.all([
      api("/api/health"),
      api("/api/sessions")
    ]);
    state.settings = data.settings || health.settings || {};
    state.sessions = data.sessions || [];
    state.activeSessionId = state.sessions[0] ? state.sessions[0].id : null;
    el.healthText.textContent = health.codex || "Codex 可用";
    syncSettingsForm();
    renderSessions();
    renderConversation();
  } catch (error) {
    el.healthText.textContent = "连接失败";
    showToast(error.message);
  }
}

async function refreshSessions(keepActive = true) {
  const data = await api("/api/sessions");
  state.settings = data.settings || state.settings;
  state.sessions = data.sessions || [];
  if (!keepActive || !state.sessions.some((session) => session.id === state.activeSessionId)) {
    state.activeSessionId = state.sessions[0] ? state.sessions[0].id : null;
  }
  renderSessions();
  renderConversation();
}

async function createSession() {
  const data = await api("/api/sessions", { method: "POST", body: "{}" });
  state.sessions.unshift(data.session);
  state.activeSessionId = data.session.id;
  state.activity = [];
  renderSessions();
  renderConversation();
}

async function saveSettings() {
  const data = await api("/api/settings", {
    method: "PATCH",
    body: JSON.stringify({ settings: readSettingsForm() })
  });
  state.settings = data.settings;
  syncSettingsForm();
  showToast("设置已保存");
}

async function deleteActiveSession() {
  const session = activeSession();
  if (!session || state.runningId) return;
  await api(`/api/sessions/${encodeURIComponent(session.id)}`, { method: "DELETE" });
  await refreshSessions(false);
}

function appendLocalMessage(role, content) {
  let session = activeSession();
  if (!session) {
    session = {
      id: `local_${Date.now()}`,
      title: "新会话",
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
      messages: []
    };
    state.sessions.unshift(session);
    state.activeSessionId = session.id;
  }

  const message = {
    id: `local_msg_${cryptoRandom()}`,
    role,
    content,
    status: role === "assistant" ? "running" : undefined,
    createdAt: new Date().toISOString(),
    events: []
  };
  session.messages.push(message);
  if (role === "assistant") {
    state.currentAssistantId = message.id;
  }
  return message;
}

function cryptoRandom() {
  const array = new Uint32Array(1);
  window.crypto.getRandomValues(array);
  return array[0].toString(16);
}

function currentAssistantMessage() {
  const session = activeSession();
  if (!session) return null;
  return session.messages.find((message) => message.id === state.currentAssistantId) || null;
}

async function sendMessage(message) {
  const activeBefore = activeSession();
  appendLocalMessage("user", message);
  appendLocalMessage("assistant", "");
  state.activity = [];
  renderSessions();
  renderConversation();
  setRunning(true);

  const response = await fetch("/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      sessionId: activeBefore ? activeBefore.id : null,
      message,
      settings: readSettingsForm()
    })
  });

  if (!response.ok || !response.body) {
    let detail = `HTTP ${response.status}`;
    try {
      const payload = await response.json();
      detail = payload.error || detail;
    } catch {
      detail = await response.text();
    }
    throw new Error(detail);
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";

  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    buffer += decoder.decode(value, { stream: true });
    const lines = buffer.split(/\r?\n/);
    buffer = lines.pop() || "";
    for (const line of lines) {
      if (line.trim()) {
        handleStreamEvent(JSON.parse(line));
      }
    }
  }

  if (buffer.trim()) {
    handleStreamEvent(JSON.parse(buffer));
  }
}

function handleStreamEvent(event) {
  if (event.type === "run_started") {
    state.runningId = event.runId;
    if (event.session && !state.sessions.some((session) => session.id === event.session.id)) {
      state.sessions.unshift(event.session);
    }
    if (event.session) {
      const local = activeSession();
      if (local && local.id.startsWith("local_")) {
        local.id = event.session.id;
        local.title = event.session.title;
        local.createdAt = event.session.createdAt;
        local.updatedAt = event.session.updatedAt;
      }
      state.activeSessionId = event.session.id;
    }
    renderSessions();
    return;
  }

  if (event.type === "assistant_delta") {
    const message = currentAssistantMessage();
    if (message) {
      message.content += event.delta || "";
      renderConversation();
    }
    return;
  }

  if (event.type === "assistant_replace") {
    const message = currentAssistantMessage();
    if (message) {
      message.content = event.content || "";
      renderConversation();
    }
    return;
  }

  if (event.type === "activity") {
    const message = currentAssistantMessage();
    if (message) {
      message.events = message.events || [];
      message.events.push(event.activity);
      if (message.events.length > 160) {
        message.events.splice(0, message.events.length - 160);
      }
      renderActivity(message.events);
    } else {
      state.activity.push(event.activity);
      renderActivity(state.activity);
    }
    return;
  }

  if (event.type === "run_finished") {
    const message = currentAssistantMessage();
    if (message) {
      message.status = event.status;
      if (!message.content && event.details) {
        message.content = event.details;
      }
    }
    state.runningId = null;
    setRunning(false, event.status === "error" ? "错误" : "空闲");
    if (event.session) {
      const index = state.sessions.findIndex((session) => session.id === event.session.id);
      if (index >= 0) {
        state.sessions[index] = event.session;
      }
      state.activeSessionId = event.session.id;
    }
    renderSessions();
    renderConversation();
  }
}

function autoSizeComposer() {
  el.messageInput.style.height = "auto";
  el.messageInput.style.height = `${Math.min(el.messageInput.scrollHeight, 180)}px`;
}

el.sessionList.addEventListener("click", (event) => {
  const button = event.target.closest("[data-session-id]");
  if (!button || state.runningId) return;
  state.activeSessionId = button.dataset.sessionId;
  state.activity = [];
  renderSessions();
  renderConversation();
});

el.newSessionButton.addEventListener("click", () => {
  if (state.runningId) return;
  createSession().catch((error) => showToast(error.message));
});

el.saveSettingsButton.addEventListener("click", () => {
  saveSettings().catch((error) => showToast(error.message));
});

el.deleteSessionButton.addEventListener("click", () => {
  deleteActiveSession().catch((error) => showToast(error.message));
});

el.stopButton.addEventListener("click", () => {
  if (!state.runningId) return;
  api(`/api/runs/${encodeURIComponent(state.runningId)}/stop`, { method: "POST" })
    .catch((error) => showToast(error.message));
});

el.messageInput.addEventListener("input", autoSizeComposer);

el.messageInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey && !event.isComposing) {
    event.preventDefault();
    el.composer.requestSubmit();
  }
});

el.composer.addEventListener("submit", (event) => {
  event.preventDefault();
  const message = el.messageInput.value.trim();
  if (!message || state.runningId) return;
  el.messageInput.value = "";
  autoSizeComposer();
  sendMessage(message).catch((error) => {
    const messageNode = currentAssistantMessage();
    if (messageNode) {
      messageNode.status = "error";
      messageNode.content = error.message;
      renderConversation();
    }
    state.runningId = null;
    setRunning(false, "错误");
    showToast(error.message);
  });
});

loadInitialData();
