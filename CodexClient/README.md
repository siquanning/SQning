# Codex Local Client

本地浏览器客户端，用当前机器上的 Codex CLI 作为执行后端。

## 运行

```powershell
npm start
```

默认地址是 `http://127.0.0.1:3789`。如果端口被占用，服务会自动尝试后续端口。

## 说明

- 默认工作区是本项目目录。
- 会话和设置保存在 `.codex-client/store.json`。
- 前端通过 `/api/chat` 调用 `codex exec --json`，续聊通过 Codex 返回的 session id 调用 `codex exec resume`。
