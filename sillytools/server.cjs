const http = require('http');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');
const lib = require('./lib.js');

const PORT = 8765;
const SIL_DIR = lib.SIL_BASE.replace('/data/default-user', '');

function json(res, data, code = 200) {
  res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Access-Control-Allow-Origin': '*' });
  res.end(JSON.stringify(data));
}

function body(req) {
  return new Promise(resolve => { let d = ''; req.on('data', c => d += c); req.on('end', () => resolve(d)); });
}

function handleApi(method, url, reqBody) {
  // GET /api/chars - list all chars
  if (method === 'GET' && url === '/api/chars') {
    try { const chars = lib.listChars(); return [200, chars]; }
    catch (e) { return [500, { error: e.message }]; }
  }

  // GET /api/chars/:name - get one char
  if (method === 'GET' && url.startsWith('/api/chars/')) {
    const name = decodeURIComponent(url.slice(11));
    try { const { card, file, path: pngPath } = lib.extractChar(name); return [200, { card, file, path: pngPath }]; }
    catch (e) { return [404, { error: e.message }]; }
  }

  // PUT /api/chars/:name - save char
  if (method === 'PUT' && url.startsWith('/api/chars/')) {
    const name = decodeURIComponent(url.slice(11));
    try {
      const { path: pngPath } = lib.extractChar(name);
      lib.writeCharCard(pngPath, JSON.parse(reqBody));
      return [200, { ok: true }];
    } catch (e) { return [400, { error: e.message }]; }
  }

  // GET /api/worlds - list all worlds
  if (method === 'GET' && url === '/api/worlds') {
    try {
      const worlds = lib.listWorlds().map(w => {
        const data = lib.readWorld(w.name);
        return { ...w, count: Object.keys(data.entries || {}).length };
      });
      return [200, worlds];
    } catch (e) { return [500, { error: e.message }]; }
  }

  // GET /api/worlds/:name - get one world
  if (method === 'GET' && url.match(/^\/api\/worlds\/[^/]+$/)) {
    const name = decodeURIComponent(url.slice(12));
    try { return [200, lib.readWorld(name)]; }
    catch (e) { return [404, { error: e.message }]; }
  }

  // PUT /api/worlds/:name - save world
  if (method === 'PUT' && url.match(/^\/api\/worlds\/[^/]+$/)) {
    const name = decodeURIComponent(url.slice(12));
    try { lib.writeWorld(name, JSON.parse(reqBody)); return [200, { ok: true }]; }
    catch (e) { return [400, { error: e.message }]; }
  }

  // POST /api/worlds/:name/entries - add entry
  if (method === 'POST' && url.match(/^\/api\/worlds\/.+\/entries$/)) {
    const name = decodeURIComponent(url.slice(12, url.lastIndexOf('/entries')));
    try {
      const id = lib.addWorldEntry(name, JSON.parse(reqBody));
      return [200, { ok: true, id }];
    } catch (e) { return [400, { error: e.message }]; }
  }

  // DELETE /api/worlds/:name/entries/:id - remove entry
  if (method === 'DELETE' && url.match(/^\/api\/worlds\/.+\/entries\/\d+$/)) {
    const parts = url.split('/');
    const name = decodeURIComponent(parts[3]);
    const id = parseInt(parts[5]);
    try { lib.removeWorldEntry(name, id); return [200, { ok: true }]; }
    catch (e) { return [400, { error: e.message }]; }
  }

  // GET /api/info
  if (method === 'GET' && url === '/api/info') {
    return [200, { silDir: SIL_DIR, version: '1.17.0' }];
  }

  // POST /api/launch
  if (method === 'POST' && url === '/api/launch') {
    exec(`start "" "${SIL_DIR}\\Start.bat"`, { cwd: SIL_DIR }, (err) => {
      if (err) console.error('Launch error:', err);
    });
    return [200, { ok: true, message: 'SillyTavern 正在启动...' }];
  }

  return [404, { error: 'API not found' }];
}

const server = http.createServer(async (req, res) => {
  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'Access-Control-Allow-Origin': '*', 'Access-Control-Allow-Methods': 'GET,PUT,POST,DELETE',
      'Access-Control-Allow-Headers': 'Content-Type'
    });
    return res.end();
  }

  const url = req.url.split('?')[0];

  if (url.startsWith('/api/')) {
    const reqBody = ['PUT', 'POST'].includes(req.method) ? await body(req) : '';
    const [code, data] = handleApi(req.method, url, reqBody);
    return json(res, data, code);
  }

  // Static files
  const filePath = url === '/' ? '/index.html' : url;
  const fullPath = path.join(__dirname, filePath);
  const mime = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript' }[path.extname(fullPath)] || 'text/plain';
  try {
    const content = fs.readFileSync(fullPath);
    res.writeHead(200, { 'Content-Type': `${mime}; charset=utf-8` });
    res.end(content);
  } catch {
    res.writeHead(404);
    res.end('Not found');
  }
});

server.listen(PORT, () => {
  console.log(`\n  SillyTavern Manager → http://localhost:${PORT}\n`);
  exec(`start http://localhost:${PORT}`);
});
