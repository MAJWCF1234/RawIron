/**
 * Proto Engine CLI: extract, pieces, proof, launch.
 * Usage:
 *   node proto-tool.mjs extract
 *   node proto-tool.mjs pieces
 *   node proto-tool.mjs proof
 *   node proto-tool.mjs launch [--mechtest|--no-kiosk|--port=N]
 */
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { execFile as execFileCallback, spawn } from 'node:child_process';
import { promisify } from 'node:util';

const execFile = promisify(execFileCallback);
const toolPath = fileURLToPath(import.meta.url);
const rootDir = path.dirname(toolPath);
const criticalFiles = [
  'index.js',
  'engine.js',
  'dev-level.js',
  'proto-tool.mjs',
  'launch-mechtest.bat',
  'dist/index.html'
];

async function runExtract() {
  const modelsDir = path.join(rootDir, 'models');
  const unpackedDir = path.join(modelsDir, 'unpacked');
  const cachePath = path.join(modelsDir, '.asset-extract-cache.json');

  fs.mkdirSync(unpackedDir, { recursive: true });

  function readJson(filePath, fallback) {
    try {
      return JSON.parse(fs.readFileSync(filePath, 'utf8'));
    } catch {
      return fallback;
    }
  }

  function writeJson(filePath, value) {
    fs.writeFileSync(filePath, JSON.stringify(value, null, 2) + '\n', 'utf8');
  }

  function getArchiveSignature(stats) {
    return `${stats.size}:${stats.mtimeMs}`;
  }

  function listArchives() {
    return fs.readdirSync(modelsDir, { withFileTypes: true })
      .filter((entry) => entry.isFile())
      .map((entry) => entry.name)
      .filter((name) => /\.(rar|zip)$/i.test(name))
      .sort((a, b) => a.localeCompare(b))
      .map((name) => {
        const archivePath = path.join(modelsDir, name);
        const stats = fs.statSync(archivePath);
        return {
          name,
          archivePath,
          ext: path.extname(name).toLowerCase(),
          targetName: path.basename(name, path.extname(name)),
          targetPath: path.join(unpackedDir, path.basename(name, path.extname(name))),
          signature: getArchiveSignature(stats),
          size: stats.size,
          mtimeMs: stats.mtimeMs
        };
      });
  }

  function pathHasContent(targetPath) {
    if (!fs.existsSync(targetPath)) return false;
    const stats = fs.statSync(targetPath);
    if (stats.isFile()) return stats.size > 0;
    const entries = fs.readdirSync(targetPath);
    return entries.length > 0;
  }

  function removePathIfExists(targetPath) {
    if (!fs.existsSync(targetPath)) return;
    fs.rmSync(targetPath, { recursive: true, force: true });
  }

  async function extractRar(archivePath, targetPath) {
    let createExtractorFromFile = null;
    try {
      ({ createExtractorFromFile } = await import('node-unrar-js'));
    } catch {
      throw new Error('RAR extraction requires node-unrar-js. Install dev dependencies with: npm install');
    }
    const extractor = await createExtractorFromFile({
      filepath: archivePath,
      targetPath
    });
    const extracted = extractor.extract();
    let extractedCount = 0;
    for (const _file of extracted.files) extractedCount += 1;
    return extractedCount;
  }

  async function extractZip(archivePath, targetPath) {
    await execFile(
      'powershell',
      [
        '-NoProfile',
        '-Command',
        `Expand-Archive -LiteralPath '${archivePath.replace(/'/g, "''")}' -DestinationPath '${targetPath.replace(/'/g, "''")}' -Force`
      ],
      { cwd: rootDir }
    );
  }

  async function ensureArchiveExtracted(archive, cache) {
    const cachedSignature = cache.archives?.[archive.name]?.signature;
    const alreadyExtracted = pathHasContent(archive.targetPath);
    if (alreadyExtracted && cachedSignature === archive.signature) {
      return { status: 'cached', extractedCount: 0 };
    }

    removePathIfExists(archive.targetPath);

    let extractedCount = 0;
    if (archive.ext === '.rar') {
      fs.mkdirSync(archive.targetPath, { recursive: true });
      extractedCount = await extractRar(archive.archivePath, archive.targetPath);
    } else if (archive.ext === '.zip') {
      await extractZip(archive.archivePath, archive.targetPath);
      extractedCount = fs.readdirSync(archive.targetPath, { recursive: true }).length;
    } else {
      throw new Error(`Unsupported archive type: ${archive.ext}`);
    }

    cache.archives ||= {};
    cache.archives[archive.name] = {
      signature: archive.signature,
      targetName: archive.targetName,
      targetPath: path.relative(rootDir, archive.targetPath).replace(/\\/g, '/'),
      extractedAt: new Date().toISOString()
    };
    return { status: 'extracted', extractedCount };
  }

  const cache = readJson(cachePath, { archives: {} });
  const archives = listArchives();

  if (archives.length === 0) {
    console.log('No archive files found in models.');
    return;
  }

  let extractedArchives = 0;
  for (const archive of archives) {
    const result = await ensureArchiveExtracted(archive, cache);
    if (result.status === 'cached') {
      console.log(`Using cached assets for ${archive.name}`);
    } else {
      extractedArchives += 1;
      console.log(`Extracted ${archive.name} -> ${path.relative(rootDir, archive.targetPath)} (${result.extractedCount} entries)`);
    }
  }

  writeJson(cachePath, cache);

  if (extractedArchives === 0) {
    console.log('All archives already unpacked and up to date.');
  } else {
    console.log(`Updated ${extractedArchives} archive(s).`);
  }
}

function runCommand(command, args) {
  return new Promise((resolve) => {
    const child = spawn(command, args, {
      cwd: rootDir,
      stdio: 'inherit',
      shell: process.platform === 'win32'
    });
    child.on('exit', (code) => resolve(code ?? 1));
  });
}

function printPieces() {
  const exists = (relPath) => fs.existsSync(path.join(rootDir, relPath));
  const sizeKb = (relPath) => {
    const fullPath = path.join(rootDir, relPath);
    if (!fs.existsSync(fullPath)) return null;
    return Math.round(fs.statSync(fullPath).size / 1024);
  };
  console.log('Protoengine Piece Board');
  console.log('=======================');
  for (const relPath of criticalFiles) {
    const present = exists(relPath);
    const marker = present ? 'OK' : 'MISSING';
    const kb = present ? `${sizeKb(relPath)} KB` : '-';
    console.log(`${marker.padEnd(8)} ${relPath.padEnd(22)} ${kb}`);
  }
}

async function runProof() {
  const checks = [
    ['node', ['--check', './proto-tool.mjs']]
  ];
  let allPassed = true;
  for (const [cmd, args] of checks) {
    console.log(`\n[check] ${cmd} ${args.join(' ')}`);
    const code = await runCommand(cmd, args);
    if (code !== 0) allPassed = false;
  }
  try {
    const {
      clampFiniteNumber,
      createRuntimeId
    } = await import('./engine.js');
    assert.match(createRuntimeId('piece'), /^piece_[0-9A-Za-z]{10}$/);
    assert.equal(clampFiniteNumber(999, 5, 0, 10), 10);
    const { devLevel } = await import('./dev-level.js');
    assert.ok(devLevel && typeof devLevel === 'object' && !Array.isArray(devLevel),
      'dev-level.js must export a level object');
    assert.ok(Array.isArray(devLevel.geometry) && devLevel.geometry.length > 0,
      'dev-level.js must contain a non-empty geometry array');
    assert.ok(Array.isArray(devLevel.lights), 'dev-level.js must contain a lights array');
  } catch (error) {
    allPassed = false;
    console.error('[check] embedded engine + level assertions failed');
    console.error(error);
  }
  console.log('\nStanding verdict');
  console.log('===============');
  console.log(allPassed ? 'TOWER_STANDING' : 'TOWER_UNSTABLE');
  process.exit(allPassed ? 0 : 1);
}

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

function resolveRequestPath(distDir, urlPath) {
  const safePath = decodeURIComponent((urlPath || '/').split('?')[0]);
  const relativePath = (safePath === '/' ? '/index.html' : safePath).replace(/^\/+/, '');
  if (relativePath.includes('..')) return null;
  const filePath = path.normalize(path.join(distDir, relativePath));
  const rel = path.relative(distDir, filePath);
  if (!rel || rel.startsWith('..') || path.isAbsolute(rel)) return null;
  return filePath;
}

async function runLaunch(launchArgs = []) {
  const distDir = path.join(rootDir, 'dist');
  if (!fs.existsSync(path.join(distDir, 'index.html'))) {
    throw new Error('[launcher] dist/index.html is missing. Build first with: npm run build:quick');
  }
  const useKiosk = !launchArgs.includes('--no-kiosk');
  const mechtest = launchArgs.includes('--mechtest');
  const host = '127.0.0.1';
  const requestedPort = Number.parseInt(launchArgs.find((a) => a.startsWith('--port='))?.split('=')[1] || '0', 10) || 0;
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

  const server = http.createServer((request, response) => {
    const filePath = resolveRequestPath(distDir, request.url || '/');
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

  const buildUrl = (port) => {
    const query = mechtest ? '?mechtest=1' : '';
    return `http://${host}:${port}/index.html${query}`;
  };

  const launchBrowser = (url) => {
    const browserExe = resolveBrowserExecutable();
    if (!browserExe) {
      console.log(`[launcher] no Chrome/Edge executable found; open manually: ${url}`);
      return;
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
  };

  await new Promise((resolve) => server.listen(requestedPort, host, resolve));
  const address = server.address();
  const port = typeof address === 'object' && address ? address.port : requestedPort;
  const url = buildUrl(port);
  console.log(`[launcher] serving ${distDir}`);
  console.log(`[launcher] url: ${url}`);
  launchBrowser(url);
  const shutdown = () => server.close(() => process.exit(0));
  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

async function main() {
  const cmd = process.argv[2];
  if (cmd === 'extract') {
    await runExtract();
    return;
  }
  if (cmd === 'pieces') {
    printPieces();
    return;
  }
  if (cmd === 'proof') {
    await runProof();
    return;
  }
  if (cmd === 'launch') {
    await runLaunch(process.argv.slice(3));
    return;
  }
  console.error('Usage: node proto-tool.mjs <extract|pieces|proof|launch>');
  process.exit(1);
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
