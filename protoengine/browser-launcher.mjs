import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = __dirname;
const distDir = path.join(rootDir, 'dist');

const args = process.argv.slice(2);
const useKiosk = !args.includes('--no-kiosk');
const mechtest = args.includes('--mechtest');
const host = '127.0.0.1';
const requestedPort = Number.parseInt(args.find((a) => a.startsWith('--port='))?.split('=')[1] || '0', 10) || 0;

const MIME_TYPES = {
  '.css': 'text/css; charset=utf-8',
  '.glb': 'model/gltf-binary',
  '.gltf': 'model/gltf+json',
  '.html': 'text/html; charset=utf-8',
  '.jpeg': 'image/jpeg',
  '.jpg': 'image/jpeg',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.txt': 'text/plain; charset=utf-8',
  '.wasm': 'application/wasm'
};

function resolveBrowserExecutable() {
  if (process.platform !== 'win32') return null;
  const candidates = [
    process.env.PROTO_BROWSER_PATH,
    process.env.BROWSER,
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe'
  ].filter(Boolean);
  return candidates.find((candidate) => fs.existsSync(candidate)) || null;
}

function buildUrl(port) {
  const query = mechtest ? '?mechtest=1' : '';
  return `http://${host}:${port}/index.html${query}`;
}

function resolveRequestPath(urlPath) {
  const safePath = decodeURIComponent((urlPath || '/').split('?')[0]);
  const relativePath = (safePath === '/' ? '/index.html' : safePath).replace(/^\/+/, '');
  if (relativePath.includes('..')) return null;

  const filePath = path.normalize(path.join(distDir, relativePath));
  const rel = path.relative(distDir, filePath);
  if (!rel || rel.startsWith('..') || path.isAbsolute(rel)) return null;
  return filePath;
}

function launchBrowser(url) {
  const browserExe = resolveBrowserExecutable();
  if (!browserExe) {
    console.log(`[launcher] no Chrome/Edge executable found; open manually: ${url}`);
    return null;
  }
  const browserArgs = useKiosk
    ? ['--kiosk', '--app=' + url, '--disable-features=Translate,AutofillServerCommunication']
    : ['--app=' + url];
  const child = spawn(browserExe, browserArgs, {
    cwd: rootDir,
    stdio: 'ignore',
    detached: false,
    windowsHide: false
  });
  child.on('error', (error) => {
    console.warn('[launcher] browser failed to start:', error.message);
    console.log(`[launcher] open manually: ${url}`);
  });
  return child;
}

if (!fs.existsSync(path.join(distDir, 'index.html'))) {
  console.error('[launcher] dist/index.html is missing. Build first with: npm run build:quick');
  process.exit(1);
}

const server = http.createServer((request, response) => {
  const filePath = resolveRequestPath(request.url || '/');
  if (!filePath || !fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    response.writeHead(404);
    response.end('Not found');
    return;
  }
  fs.readFile(filePath, (error, data) => {
    if (error) {
      response.writeHead(500);
      response.end('Server error');
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    response.writeHead(200, {
      'Content-Type': MIME_TYPES[ext] || 'application/octet-stream',
      'Cache-Control': 'no-cache'
    });
    response.end(data);
  });
});

server.listen(requestedPort, host, () => {
  const address = server.address();
  const port = typeof address === 'object' && address ? address.port : requestedPort;
  const url = buildUrl(port);
  console.log(`[launcher] serving ${distDir}`);
  console.log(`[launcher] url: ${url}`);
  launchBrowser(url);
});

const shutdown = () => {
  server.close(() => process.exit(0));
};
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
