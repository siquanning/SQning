const http = require("node:http");
const https = require("node:https");

const DEEPSEEK_HOST = "api.deepseek.com";
const DEEPSEEK_PATH = "/v1/chat/completions";
const DEEPSEEK_KEY = "sk-c801f792a1444a0bbe420d25c2187d73";
const PORT = Number(process.env.PROXY_PORT || 3791);
const HOST = process.env.PROXY_HOST || "127.0.0.1";

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => { body += chunk; });
    req.on("end", () => {
      try { resolve(body ? JSON.parse(body) : {}); }
      catch { reject(new Error("Invalid JSON")); }
    });
    req.on("error", reject);
  });
}

function extractTextContent(content) {
  if (typeof content === "string") return content;
  if (!Array.isArray(content)) return "";

  return content
    .map((part) => {
      if (typeof part === "string") return part;
      if (!part || typeof part !== "object") return "";
      if (typeof part.text === "string") return part.text;
      if (typeof part.content === "string") return part.content;
      return "";
    })
    .filter(Boolean)
    .join("");
}

function hasChatContent(message) {
  return (
    (typeof message.content === "string" && message.content.length > 0) ||
    Array.isArray(message.content) ||
    (Array.isArray(message.tool_calls) && message.tool_calls.length > 0)
  );
}

function normalizeUsage(usage) {
  if (!usage || typeof usage !== "object") return null;
  const inputTokens = usage.input_tokens ?? usage.prompt_tokens ?? 0;
  const outputTokens = usage.output_tokens ?? usage.completion_tokens ?? 0;
  const totalTokens = usage.total_tokens ?? (inputTokens + outputTokens);

  const normalized = {
    input_tokens: inputTokens,
    input_tokens_details: {
      cached_tokens: 0,
      ...(usage.input_tokens_details || usage.prompt_tokens_details || {})
    },
    output_tokens: outputTokens,
    output_tokens_details: {
      reasoning_tokens: 0,
      ...(usage.output_tokens_details || usage.completion_tokens_details || {})
    },
    total_tokens: totalTokens
  };

  return normalized;
}

// Responses API "input" to Chat Completions "messages"
function inputToMessages(input, instructions) {
  const messages = [];
  // instructions -> system message
  if (instructions) {
    messages.push({ role: "system", content: instructions });
  }
  if (!Array.isArray(input)) return messages;

  for (const item of input) {
    if (item.role === "user") {
      const content = item.content;
      if (typeof content === "string") {
        messages.push({ role: "user", content });
      } else if (Array.isArray(content)) {
        // multimodal content array
        const parts = content.map((p) => {
          if (p.type === "input_text") return { type: "text", text: p.text };
          if (p.type === "input_image" && p.image_url) return { type: "image_url", image_url: { url: p.image_url } };
          return p;
        });
        messages.push({ role: "user", content: parts });
      }
    } else if (item.role === "assistant") {
      const msg = { role: "assistant" };
      const text = extractTextContent(item.content);
      if (text) {
        msg.content = text;
      }
      // tool calls in assistant message
      if (item.tool_calls && Array.isArray(item.tool_calls)) {
        msg.tool_calls = item.tool_calls.map((tc) => ({
          id: tc.id,
          type: "function",
          function: {
            name: tc.function?.name || tc.name,
            arguments: typeof tc.function?.arguments === "string"
              ? tc.function.arguments
              : JSON.stringify(tc.function?.arguments || tc.arguments || {})
          }
        }));
        msg.content = msg.content || null;
      }
      if (hasChatContent(msg)) {
        messages.push(msg);
      }
    } else if (item.role === "system") {
      const text = extractTextContent(item.content);
      if (text) messages.push({ role: "system", content: text });
    } else if (item.type === "message" && item.role === "assistant") {
      const text = extractTextContent(item.content);
      if (text) messages.push({ role: "assistant", content: text });
    } else if (item.type === "function_call_output") {
      messages.push({
        role: "tool",
        tool_call_id: item.call_id,
        content: typeof item.output === "string" ? item.output : JSON.stringify(item.output)
      });
    } else if (item.role === "tool") {
      messages.push({
        role: "tool",
        tool_call_id: item.tool_call_id,
        content: item.content
      });
    }
  }
  return messages;
}

function isPlainObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function normalizeToolParameters(tool, name) {
  const schema = tool.parameters || tool.input_schema || tool.function?.parameters;

  if (isPlainObject(schema) && schema.type === "object") {
    return schema;
  }

  if (name === "apply_patch") {
    return {
      type: "object",
      properties: {
        patch: {
          type: "string",
          description: "Patch text to apply."
        }
      },
      required: ["patch"],
      additionalProperties: false
    };
  }

  if (isPlainObject(schema)) {
    return {
      ...schema,
      type: "object",
      properties: isPlainObject(schema.properties) ? schema.properties : {},
      required: Array.isArray(schema.required) ? schema.required : []
    };
  }

  return {
    type: "object",
    properties: {
      input: {
        type: "string",
        description: "Tool input."
      }
    },
    required: ["input"],
    additionalProperties: false
  };
}

function translateTools(tools) {
  if (!Array.isArray(tools)) return undefined;
  const result = [];
  for (const t of tools) {
    // name may be at top level (Responses API) or nested (Chat Completions style)
    const name = t.name || t.function?.name;
    if (!name) {
      console.warn(`Skipping tool without name: ${JSON.stringify(t).slice(0, 200)}`);
      continue;
    }
    result.push({
      type: "function",
      function: {
        name,
        description: t.description || t.function?.description || "",
        parameters: normalizeToolParameters(t, name)
      }
    });
  }
  return result.length > 0 ? result : undefined;
}

function buildChatRequest(body) {
  const messages = inputToMessages(body.input, body.instructions);
  const chatReq = {
    model: body.model || "deepseek-chat",
    messages,
    stream: body.stream !== false,
    max_tokens: body.max_output_tokens || 4096,
  };
  if (body.temperature != null) chatReq.temperature = body.temperature;
  if (body.top_p != null) chatReq.top_p = body.top_p;
  const tools = translateTools(body.tools);
  if (tools) {
    chatReq.tools = tools;
    chatReq.tool_choice = body.tool_choice || "auto";
  }
  return chatReq;
}

function sendJson(res, status, payload) {
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Access-Control-Allow-Origin": "*"
  });
  res.end(JSON.stringify(payload));
}

function writeSse(res, event, payload) {
  res.write(`event: ${event}\ndata: ${JSON.stringify(payload)}\n\n`);
}

function streamChatResponse(res, json, model) {
  const response = chatToResponse(json, model);
  const responseId = response.id;

  res.writeHead(200, {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache",
    "Connection": "keep-alive"
  });

  writeSse(res, "response.created", {
    type: "response.created",
    response: { ...response, status: "in_progress", output: [] }
  });

  for (let outputIndex = 0; outputIndex < response.output.length; outputIndex += 1) {
    const item = response.output[outputIndex];
    writeSse(res, "response.output_item.added", {
      type: "response.output_item.added",
      output_index: outputIndex,
      item: { ...item, status: "in_progress" }
    });

    if (item.type === "message") {
      const part = item.content?.[0] || { type: "output_text", text: "" };
      const text = part.text || "";
      writeSse(res, "response.content_part.added", {
        type: "response.content_part.added",
        item_id: item.id,
        output_index: outputIndex,
        content_index: 0,
        part: { type: "output_text", text: "" }
      });
      if (text) {
        writeSse(res, "response.output_text.delta", {
          type: "response.output_text.delta",
          item_id: item.id,
          output_index: outputIndex,
          content_index: 0,
          delta: text
        });
      }
      writeSse(res, "response.output_text.done", {
        type: "response.output_text.done",
        item_id: item.id,
        output_index: outputIndex,
        content_index: 0,
        text
      });
      writeSse(res, "response.content_part.done", {
        type: "response.content_part.done",
        item_id: item.id,
        output_index: outputIndex,
        content_index: 0,
        part
      });
    } else if (item.type === "function_call") {
      const args = item.arguments || "{}";
      if (args) {
        writeSse(res, "response.function_call_arguments.delta", {
          type: "response.function_call_arguments.delta",
          item_id: item.id,
          output_index: outputIndex,
          delta: args
        });
      }
      writeSse(res, "response.function_call_arguments.done", {
        type: "response.function_call_arguments.done",
        item_id: item.id,
        output_index: outputIndex,
        arguments: args
      });
    }

    writeSse(res, "response.output_item.done", {
      type: "response.output_item.done",
      output_index: outputIndex,
      item: { ...item, status: "completed" }
    });
  }

  writeSse(res, "response.completed", {
    type: "response.completed",
    response
  });
  res.end();
}

function proxyToDeepSeek(chatReq) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(chatReq);
    const req = https.request({
      hostname: DEEPSEEK_HOST,
      port: 443,
      path: DEEPSEEK_PATH,
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": `Bearer ${DEEPSEEK_KEY}`,
        "Accept": chatReq.stream ? "text/event-stream" : "application/json",
        "Content-Length": Buffer.byteLength(payload)
      }
    }, (deepRes) => {
      const isStream = deepRes.headers["content-type"]?.includes("text/event-stream");
      if (isStream) {
        resolve({ stream: deepRes, status: deepRes.statusCode });
      } else {
        let data = "";
        deepRes.on("data", (c) => { data += c.toString(); });
        deepRes.on("end", () => {
          try { resolve({ json: JSON.parse(data), status: deepRes.statusCode }); }
          catch (e) { reject(new Error(`Parse error: ${data.slice(0, 200)}`)); }
        });
      }
    });
    req.on("error", reject);
    req.write(payload);
    req.end();
  });
}

// Convert Chat Completions chunk to Responses SSE event
function chunkToResponseEvent(chunk, responseId, model) {
  const choice = chunk.choices?.[0];
  if (!choice) return null;

  const messageId = "msg_" + responseId;

  // text delta
  if (choice.delta?.content) {
    return [
      `event: response.output_item.added\ndata: ${JSON.stringify({
        type: "response.output_item.added",
        output_index: 0,
        item: {
          id: messageId,
          type: "message",
          role: "assistant",
          status: "in_progress",
          content: []
        }
      })}\n\n`,
      `event: response.content_part.added\ndata: ${JSON.stringify({
        type: "response.content_part.added",
        item_id: messageId,
        output_index: 0,
        content_index: 0,
        part: { type: "output_text", text: "" }
      })}\n\n`,
      `event: response.output_text.delta\ndata: ${JSON.stringify({
        type: "response.output_text.delta",
        item_id: messageId,
        output_index: 0,
        content_index: 0,
        delta: choice.delta.content
      })}\n\n`
    ].join("");
  }

  // tool call delta
  if (choice.delta?.tool_calls) {
    const events = [];
    for (const tc of choice.delta.tool_calls) {
      if (tc.id) {
        // new tool call
        events.push(`event: response.output_item.added\ndata: ${JSON.stringify({
          type: "response.output_item.added",
          output_index: tc.index || 0,
          item: {
            id: tc.id,
            type: "function_call",
            name: tc.function?.name || "",
            call_id: tc.id,
            status: "in_progress"
          }
        })}\n\n`);
      }
      if (tc.function?.arguments) {
        events.push(`event: response.function_call_arguments.delta\ndata: ${JSON.stringify({
          type: "response.function_call_arguments.delta",
          item_id: tc.id,
          output_index: tc.index || 0,
          delta: tc.function.arguments
        })}\n\n`);
      }
    }
    return events.join("") || null;
  }

  // finish reason
  if (choice.finish_reason) {
    if (choice.finish_reason === "stop" || choice.finish_reason === "tool_calls") {
      return [
        `event: response.output_text.done\ndata: ${JSON.stringify({
          type: "response.output_text.done",
          item_id: messageId,
          output_index: 0,
          content_index: 0,
          text: ""
        })}\n\n`,
        `event: response.content_part.done\ndata: ${JSON.stringify({
          type: "response.content_part.done",
          item_id: messageId,
          output_index: 0,
          content_index: 0
        })}\n\n`,
        `event: response.output_item.done\ndata: ${JSON.stringify({
          type: "response.output_item.done",
          output_index: 0,
          item: {
            id: messageId,
            type: "message",
            role: "assistant",
            status: "completed"
          }
        })}\n\n`,
        `event: response.completed\ndata: ${JSON.stringify({
          type: "response.completed",
          response: {
            id: responseId,
            object: "response",
            model,
            status: "completed",
            output: []
          }
        })}\n\n`
      ].join("");
    }
  }

  return null;
}

// Convert non-streaming Chat Completions response to Responses format
function chatToResponse(json, model) {
  const choice = json.choices?.[0];
  const msg = choice?.message || {};

  const output = [];
  if (msg.content) {
    output.push({
      id: "msg_" + json.id,
      type: "message",
      role: "assistant",
      status: "completed",
      content: [{ type: "output_text", text: msg.content }]
    });
  }
  if (msg.tool_calls) {
    for (const tc of msg.tool_calls) {
      output.push({
        id: tc.id || "call_" + Math.random().toString(36).slice(2, 10),
        type: "function_call",
        name: tc.function?.name || "",
        call_id: tc.id,
        arguments: tc.function?.arguments || "{}",
        status: "completed"
      });
    }
  }

  return {
    id: json.id || "resp_" + Math.random().toString(36).slice(2, 10),
    object: "response",
    model: model || json.model,
    status: "completed",
    output,
    usage: normalizeUsage(json.usage)
  };
}

const server = http.createServer(async (req, res) => {
  // CORS
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "*");

  if (req.method === "OPTIONS") {
    res.writeHead(204);
    res.end();
    return;
  }

  const url = new URL(req.url || "/", `http://${req.headers.host}`);
  const pathname = url.pathname;

  // Health check
  if (req.method === "GET" && (pathname === "/" || pathname === "/health")) {
    sendJson(res, 200, { status: "ok", target: `https://${DEEPSEEK_HOST}${DEEPSEEK_PATH}` });
    return;
  }

  // Responses API endpoint
  if (req.method === "POST" && (pathname === "/responses" || pathname.endsWith("/responses"))) {
    let body;
    try {
      body = await readBody(req);
    } catch (e) {
      sendJson(res, 400, { error: e.message });
      return;
    }

    const chatReq = buildChatRequest(body);
    const upstreamReq = { ...chatReq, stream: false };
    const wantsStream = body.stream !== false;
    console.log(`[${new Date().toISOString()}] ${chatReq.messages.length} msgs -> DeepSeek`);

    try {
      const result = await proxyToDeepSeek(upstreamReq);

      if (result.json?.error || (result.status && result.status >= 400)) {
        sendJson(res, result.status || 502, { error: result.json?.error || { message: "DeepSeek error" } });
      } else {
        const respBody = chatToResponse(result.json, body.model);
        if (wantsStream) {
          streamChatResponse(res, result.json, body.model);
        } else {
          sendJson(res, 200, respBody);
        }
      }
    } catch (e) {
      console.error("Proxy error:", e.message);
      sendJson(res, 502, { error: { message: `DeepSeek error: ${e.message}` } });
    }
    return;
  }

  // Models list (Codex may query this)
  if (req.method === "GET" && (pathname === "/v1/models" || pathname === "/models")) {
    sendJson(res, 200, {
      object: "list",
      data: [{ id: "deepseek-chat", object: "model" }]
    });
    return;
  }

  sendJson(res, 404, { error: { message: "Not found" } });
});

server.listen(PORT, HOST, () => {
  console.log(`Codex→DeepSeek proxy: http://${HOST}:${PORT}`);
  console.log(`POST /responses → https://${DEEPSEEK_HOST}${DEEPSEEK_PATH}`);
});
