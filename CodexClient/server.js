const http = require("node:http");
const fs = require("node:fs");
const fsp = require("node:fs/promises");
const path = require("node:path");
const crypto = require("node:crypto");
const { spawn } = require("node:child_process");

const ROOT = __dirname;
const PUBLIC_DIR = path.join(ROOT, "public");
const DATA_DIR = path.join(ROOT, ".codex-client");
const STORE_FILE = path.join(DATA_DIR, "store.json");
const HOST = process.env.HOST || "127.0.0.1";
const BASE_PORT = Number(process.env.PORT || process.env.CODEX_CLIENT_PORT || 3789);
const CODEX_COMMAND = resolveCodexCommand();
const SPAWN_OPTIONS = { windowsHide: true };

const allowedSandboxes = new Set(["read-only", "workspace-write", "danger-full-access"]);
const allowedApprovals = new Set(["never", "on-request", "untrusted"]);
const running = new Map();

const defaultSettings = {
  workspace: process.env.CODEX_CLIENT_WORKSPACE || ROOT,
  model: "",
  sandbox: "workspace-write",
  approval: "never",
  bypassSandbox: false
};

function resolveCodexCommand() {
  if (process.env.CODEX_COMMAND) {
    return process.env.CODEX_COMMAND;
  }

  if (process.platform === "win32") {
    const appData = process.env.APPDATA || (
      process.env.USERPROFILE ? path.join(process.env.USERPROFILE, "AppData", "Roaming") : ""
    );
    const npmShim = appData ? path.join(appData, "npm", "codex.cmd") : "";
    if (npmShim && fs.existsSync(npmShim)) {
      return npmShim;
    }
  }

  return "codex";
}

function uid(prefix) {
  return `${prefix}_${crypto.randomBytes(8).toString("hex")}`;
}

function nowIso() {
  return new Date().toISOString();
}

function normalizeStore(raw) {
  return {
    settings: cleanSettings(raw && raw.settings ? raw.settings : {}),
    sessions: Array.isArray(raw && raw.sessions) ? raw.sessions : []
  };
}

async function readStore() {
  await fsp.mkdir(DATA_DIR, { recursive: true });
  try {
    const text = await fsp.readFile(STORE_FILE, "utf8");
    return normalizeStore(JSON.parse(text));
  } catch (error) {
    if (error.code !== "ENOENT") {
      console.warn(`Unable to read store: ${error.message}`);
    }
    return normalizeStore({});
  }
}

async function writeStore(store) {
  await fsp.mkdir(DATA_DIR, { recursive: true });
  const normalized = normalizeStore(store);
  await fsp.writeFile(STORE_FILE, `${JSON.stringify(normalized, null, 2)}\n`, "utf8");
}

function cleanSettings(input, base = defaultSettings) {
  const merged = { ...defaultSettings, ...base, ...(input || {}) };
  const workspace = String(merged.workspace || defaultSettings.workspace).trim();
  const sandbox = allowedSandboxes.has(merged.sandbox) ? merged.sandbox : defaultSettings.sandbox;
  const approval = allowedApprovals.has(merged.approval) ? merged.approval : defaultSettings.approval;

  return {
    workspace: path.resolve(workspace || defaultSettings.workspace),
    model: String(merged.model || "").trim(),
    sandbox,
    approval,
    bypassSandbox: Boolean(merged.bypassSandbox)
  };
}

function publicSession(session) {
  return {
    id: session.id,
    title: session.title || "新会话",
    createdAt: session.createdAt,
    updatedAt: session.updatedAt,
    codexSessionId: session.codexSessionId || null,
    messages: Array.isArray(session.messages) ? session.messages : []
  };
}

function sendJson(res, status, payload) {
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store"
  });
  res.end(JSON.stringify(payload));
}

function sendNdjson(res, payload) {
  res.write(`${JSON.stringify(payload)}\n`);
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      body += chunk;
      if (body.length > 1024 * 1024) {
        reject(new Error("请求体太大"));
        req.destroy();
      }
    });
    req.on("end", () => {
      if (!body) {
        resolve({});
        return;
      }
      try {
        resolve(JSON.parse(body));
      } catch {
        reject(new Error("JSON 格式无效"));
      }
    });
    req.on("error", reject);
  });
}

function contentType(filePath) {
  const ext = path.extname(filePath).toLowerCase();
  if (ext === ".html") return "text/html; charset=utf-8";
  if (ext === ".css") return "text/css; charset=utf-8";
  if (ext === ".js") return "text/javascript; charset=utf-8";
  if (ext === ".svg") return "image/svg+xml";
  if (ext === ".json") return "application/json; charset=utf-8";
  return "application/octet-stream";
}

async function serveStatic(req, res, url) {
  const requestPath = url.pathname === "/" ? "/index.html" : decodeURIComponent(url.pathname);
  const safePath = path.normalize(requestPath).replace(/^(\.\.[/\\])+/, "");
  const filePath = path.join(PUBLIC_DIR, safePath);

  if (!filePath.startsWith(PUBLIC_DIR)) {
    sendJson(res, 403, { error: "Forbidden" });
    return;
  }

  try {
    const stat = await fsp.stat(filePath);
    if (!stat.isFile()) {
      sendJson(res, 404, { error: "Not found" });
      return;
    }
    res.writeHead(200, {
      "Content-Type": contentType(filePath),
      "Cache-Control": "no-store"
    });
    fs.createReadStream(filePath).pipe(res);
  } catch {
    sendJson(res, 404, { error: "Not found" });
  }
}

function firstLineTitle(text) {
  const clean = String(text || "").replace(/\s+/g, " ").trim();
  if (!clean) return "新会话";
  return clean.length > 28 ? `${clean.slice(0, 28)}...` : clean;
}

function buildCodexArgs(settings, codexSessionId) {
  if (codexSessionId) {
    const args = ["exec", "resume", "--json", "--skip-git-repo-check"];
    if (settings.model) {
      args.push("-m", settings.model);
    }
    if (settings.bypassSandbox) {
      args.push("--dangerously-bypass-approvals-and-sandbox");
    }
    args.push(codexSessionId, "-");
    return args;
  }

  const args = [
    "exec",
    "--json",
    "--color",
    "never",
    "--skip-git-repo-check"
  ];

  if (settings.model) {
    args.push("-m", settings.model);
  }

  if (settings.bypassSandbox) {
    args.push("--dangerously-bypass-approvals-and-sandbox");
  } else {
    args.push("-C", settings.workspace);
    args.push("--sandbox", settings.sandbox);
    args.push("--ask-for-approval", settings.approval);
  }

  args.push("-");
  return args;
}

function textFromContent(content) {
  if (typeof content === "string") return content;
  if (!Array.isArray(content)) return "";
  return content
    .map((part) => {
      if (!part) return "";
      if (typeof part === "string") return part;
      if (typeof part.text === "string") return part.text;
      if (typeof part.content === "string") return part.content;
      return "";
    })
    .join("");
}

function extractAssistantFull(event) {
  const payload = event && event.payload ? event.payload : event;
  if (!payload) return "";
  if (event.type === "response_item" && payload.type === "message" && payload.role === "assistant") {
    return textFromContent(payload.content);
  }
  if (payload.type === "message" && payload.role === "assistant") {
    return textFromContent(payload.content);
  }
  if (event.type === "message" && event.role === "assistant") {
    return textFromContent(event.content);
  }
  return "";
}

function extractAssistantDelta(event) {
  const payload = event && event.payload ? event.payload : {};
  const candidates = [
    event && event.delta,
    payload.delta,
    payload.message_delta,
    payload.text_delta,
    payload.content_delta
  ];
  if (event.type === "event_msg" && typeof payload.type === "string") {
    if (payload.type.includes("agent_message_delta") || payload.type.includes("assistant_message_delta")) {
      return candidates.find((value) => typeof value === "string") || "";
    }
  }
  if (event.type === "agent_message_delta" || event.type === "assistant_message_delta") {
    return candidates.find((value) => typeof value === "string") || "";
  }
  return "";
}

function summarizeActivity(event) {
  const payload = event && event.payload ? event.payload : {};

  if (event.type === "session_meta" && payload.id) {
    return { tone: "meta", title: "Codex 会话", text: payload.id };
  }

  if (event.type === "event_msg") {
    const type = payload.type || "event";
    if (type === "task_started") {
      return { tone: "meta", title: "开始", text: "任务已启动" };
    }
    if (type === "task_complete" || type === "turn_complete") {
      return { tone: "meta", title: "完成", text: "Codex 已返回" };
    }
    if (type === "exec_command_begin" || type === "exec_command_started") {
      return {
        tone: "command",
        title: "命令",
        text: payload.command || payload.cmd || payload.argv || "执行命令"
      };
    }
    if (type === "exec_command_end" || type === "exec_command_finished") {
      const code = payload.exit_code ?? payload.exitCode ?? payload.code;
      return { tone: code === 0 ? "meta" : "error", title: "命令结束", text: code === undefined ? "" : `退出码 ${code}` };
    }
    if (type === "patch_apply_begin" || type === "patch_apply_started") {
      return { tone: "command", title: "改文件", text: payload.path || "应用补丁" };
    }
    if (type === "error") {
      return { tone: "error", title: "错误", text: payload.message || "Codex 报错" };
    }
    return null;
  }

  if (event.type === "response_item") {
    const itemType = payload.type || "";
    if (itemType === "function_call") {
      return { tone: "command", title: "工具", text: payload.name || "调用工具" };
    }
  }

  return null;
}

function appendLimited(list, item, limit = 160) {
  list.push(item);
  if (list.length > limit) {
    list.splice(0, list.length - limit);
  }
}

function quoteCmdArg(value) {
  return `"${String(value).replace(/(["^&|<>()%])/g, "^$1")}"`;
}

function spawnCodex(args, options = {}) {
  if (process.platform !== "win32") {
    return spawn(CODEX_COMMAND, args, { ...SPAWN_OPTIONS, ...options });
  }

  const commandLine = [CODEX_COMMAND, ...args].map(quoteCmdArg).join(" ");
  return spawn("cmd.exe", ["/d", "/c", `"${commandLine}"`], {
    ...SPAWN_OPTIONS,
    windowsVerbatimArguments: true,
    ...options
  });
}

async function codexVersion() {
  return new Promise((resolve) => {
    const child = spawnCodex(["--version"]);
    let output = "";
    let stderr = "";
    let settled = false;
    const done = (value) => {
      if (settled) return;
      settled = true;
      resolve(value);
    };
    const timer = setTimeout(() => {
      child.kill();
      done(null);
    }, 8000);
    child.stdout.on("data", (chunk) => {
      output += chunk.toString("utf8");
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString("utf8");
    });
    child.on("error", () => {
      clearTimeout(timer);
      done(null);
    });
    child.on("close", () => {
      clearTimeout(timer);
      if (!output.trim() && stderr.trim()) {
        console.warn(`Unable to read codex version: ${stderr.trim()}`);
      }
      done(output.trim() || null);
    });
  });
}

async function handleChat(req, res) {
  let body;
  try {
    body = await readBody(req);
  } catch (error) {
    sendJson(res, 400, { error: error.message });
    return;
  }

  const prompt = String(body.message || "").trim();
  if (!prompt) {
    sendJson(res, 400, { error: "请输入内容" });
    return;
  }

  const store = await readStore();
  const settings = cleanSettings(body.settings || {}, store.settings);
  let session = store.sessions.find((item) => item.id === body.sessionId);
  const timestamp = nowIso();

  if (!session) {
    session = {
      id: uid("ses"),
      title: firstLineTitle(prompt),
      createdAt: timestamp,
      updatedAt: timestamp,
      codexSessionId: null,
      messages: []
    };
    store.sessions.unshift(session);
  }

  const userMessage = {
    id: uid("msg"),
    role: "user",
    content: prompt,
    createdAt: timestamp
  };
  const assistantMessage = {
    id: uid("msg"),
    role: "assistant",
    content: "",
    status: "running",
    createdAt: timestamp,
    events: []
  };

  if (!session.title || session.title === "新会话") {
    session.title = firstLineTitle(prompt);
  }
  session.updatedAt = timestamp;
  session.messages.push(userMessage, assistantMessage);
  store.settings = settings;
  await writeStore(store);

  let workspaceStat = null;
  try {
    workspaceStat = await fsp.stat(settings.workspace);
  } catch {
    workspaceStat = null;
  }

  if (!workspaceStat || !workspaceStat.isDirectory()) {
    assistantMessage.status = "error";
    assistantMessage.content = `工作区不存在：${settings.workspace}`;
    session.updatedAt = nowIso();
    await writeStore(store);
    sendJson(res, 400, { error: assistantMessage.content, session: publicSession(session) });
    return;
  }

  res.writeHead(200, {
    "Content-Type": "application/x-ndjson; charset=utf-8",
    "Cache-Control": "no-store",
    "Connection": "keep-alive",
    "X-Accel-Buffering": "no"
  });

  const runId = uid("run");
  sendNdjson(res, {
    type: "run_started",
    runId,
    session: publicSession(session),
    settings
  });

  const args = buildCodexArgs(settings, session.codexSessionId);
  const child = spawnCodex(args, {
    cwd: settings.workspace,
    env: { ...process.env, FORCE_COLOR: "0" }
  });

  const runState = { child, stopRequested: false };
  running.set(runId, runState);

  let stdoutBuffer = "";
  let stderrBuffer = "";
  let closedByClient = false;
  let finished = false;

  function finish(status, details) {
    if (finished) return;
    finished = true;
    running.delete(runId);
    assistantMessage.status = status;
    if (!assistantMessage.content.trim() && details) {
      assistantMessage.content = details;
    }
    session.updatedAt = nowIso();
    writeStore(store)
      .catch((error) => {
        console.warn(`Unable to write store: ${error.message}`);
      })
      .finally(() => {
        if (!res.destroyed) {
          sendNdjson(res, {
            type: "run_finished",
            status,
            details,
            session: publicSession(session)
          });
          res.end();
        }
      });
  }

  function processEventLine(line) {
    const trimmed = line.trim();
    if (!trimmed) return;

    let event;
    try {
      event = JSON.parse(trimmed);
    } catch {
      const activity = { tone: "raw", title: "输出", text: trimmed };
      appendLimited(assistantMessage.events, activity);
      sendNdjson(res, { type: "activity", activity });
      return;
    }

    if (event.type === "session_meta" && event.payload && event.payload.id) {
      session.codexSessionId = event.payload.id;
    }

    const activity = summarizeActivity(event);
    if (activity) {
      appendLimited(assistantMessage.events, activity);
      sendNdjson(res, { type: "activity", activity });
    }

    const delta = extractAssistantDelta(event);
    if (delta) {
      assistantMessage.content += delta;
      sendNdjson(res, { type: "assistant_delta", messageId: assistantMessage.id, delta });
    }

    const full = extractAssistantFull(event);
    if (full && full !== assistantMessage.content) {
      assistantMessage.content = full;
      sendNdjson(res, { type: "assistant_replace", messageId: assistantMessage.id, content: full });
    }
  }

  child.stdout.on("data", (chunk) => {
    stdoutBuffer += chunk.toString("utf8");
    const lines = stdoutBuffer.split(/\r?\n/);
    stdoutBuffer = lines.pop() || "";
    for (const line of lines) {
      processEventLine(line);
    }
  });

  child.stderr.on("data", (chunk) => {
    stderrBuffer += chunk.toString("utf8");
    const lines = stderrBuffer.split(/\r?\n/);
    stderrBuffer = lines.pop() || "";
    for (const line of lines) {
      if (!line.trim()) continue;
      const activity = { tone: "error", title: "stderr", text: line.trim() };
      appendLimited(assistantMessage.events, activity);
      sendNdjson(res, { type: "activity", activity });
    }
  });

  child.on("error", (error) => {
    finish("error", `无法启动 Codex：${error.message}`);
  });

  child.on("close", (code, signal) => {
    if (stdoutBuffer.trim()) {
      processEventLine(stdoutBuffer);
      stdoutBuffer = "";
    }
    if (stderrBuffer.trim()) {
      const activity = { tone: "error", title: "stderr", text: stderrBuffer.trim() };
      appendLimited(assistantMessage.events, activity);
      sendNdjson(res, { type: "activity", activity });
    }

    if (closedByClient || runState.stopRequested) {
      finish("cancelled", "请求已取消");
    } else if (code === 0) {
      finish("complete", "");
    } else {
      const reason = signal ? `信号 ${signal}` : `退出码 ${code}`;
      finish("error", `Codex 运行失败：${reason}`);
    }
  });

  child.stdin.on("error", () => {});
  child.stdin.end(prompt);

  res.on("close", () => {
    if (!finished && !child.killed) {
      closedByClient = true;
      child.kill("SIGTERM");
    }
  });
}

async function handleApi(req, res, url) {
  if (req.method === "GET" && url.pathname === "/api/health") {
    const store = await readStore();
    sendJson(res, 200, {
      ok: true,
      codex: await codexVersion(),
      settings: store.settings
    });
    return;
  }

  if (req.method === "GET" && url.pathname === "/api/sessions") {
    const store = await readStore();
    sendJson(res, 200, {
      settings: store.settings,
      sessions: store.sessions.map(publicSession)
    });
    return;
  }

  if (req.method === "POST" && url.pathname === "/api/sessions") {
    const store = await readStore();
    const timestamp = nowIso();
    const session = {
      id: uid("ses"),
      title: "新会话",
      createdAt: timestamp,
      updatedAt: timestamp,
      codexSessionId: null,
      messages: []
    };
    store.sessions.unshift(session);
    await writeStore(store);
    sendJson(res, 201, { session: publicSession(session) });
    return;
  }

  if (req.method === "PATCH" && url.pathname === "/api/settings") {
    let body;
    try {
      body = await readBody(req);
    } catch (error) {
      sendJson(res, 400, { error: error.message });
      return;
    }
    const store = await readStore();
    store.settings = cleanSettings(body.settings || body, store.settings);
    await writeStore(store);
    sendJson(res, 200, { settings: store.settings });
    return;
  }

  if (req.method === "DELETE" && url.pathname.startsWith("/api/sessions/")) {
    const id = decodeURIComponent(url.pathname.split("/").pop() || "");
    const store = await readStore();
    const before = store.sessions.length;
    store.sessions = store.sessions.filter((item) => item.id !== id);
    await writeStore(store);
    sendJson(res, before === store.sessions.length ? 404 : 200, { deleted: before !== store.sessions.length });
    return;
  }

  if (req.method === "POST" && url.pathname === "/api/chat") {
    await handleChat(req, res);
    return;
  }

  if (req.method === "POST" && url.pathname.startsWith("/api/runs/") && url.pathname.endsWith("/stop")) {
    const id = decodeURIComponent(url.pathname.split("/")[3] || "");
    const runState = running.get(id);
    if (runState && runState.child && !runState.child.killed) {
      runState.stopRequested = true;
      runState.child.kill("SIGTERM");
      sendJson(res, 200, { stopped: true });
    } else {
      sendJson(res, 404, { stopped: false });
    }
    return;
  }

  sendJson(res, 404, { error: "Not found" });
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  if (url.pathname.startsWith("/api/")) {
    handleApi(req, res, url).catch((error) => {
      console.error(error);
      if (!res.headersSent) {
        sendJson(res, 500, { error: error.message || "Server error" });
      } else {
        res.end();
      }
    });
    return;
  }
  serveStatic(req, res, url).catch((error) => {
    console.error(error);
    sendJson(res, 500, { error: error.message || "Server error" });
  });
});

function listen(port, remainingAttempts) {
  server.once("error", (error) => {
    if (error.code === "EADDRINUSE" && remainingAttempts > 0) {
      listen(port + 1, remainingAttempts - 1);
      return;
    }
    console.error(error);
    process.exit(1);
  });
  server.listen(port, HOST, () => {
    console.log(`Codex client is running at http://${HOST}:${port}`);
  });
}

listen(BASE_PORT, 10);
