import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const rootDir = path.dirname(__filename);

const criticalFiles = [
  'index.js',
  'engine.js',
  'assetLoader.js',
  'constants.js',
  'spatialIndex.js',
  'browser-launcher.mjs',
  'proto-tool.mjs',
  'engine.test.js',
  'launch-mechtest.bat',
  'Launch-Test-Hall.bat',
  'dev_level.json',
  'dist/index.html'
];

function exists(relPath) {
  return fs.existsSync(path.join(rootDir, relPath));
}

function sizeKb(relPath) {
  const fullPath = path.join(rootDir, relPath);
  if (!fs.existsSync(fullPath)) return null;
  const stats = fs.statSync(fullPath);
  return Math.round(stats.size / 1024);
}

function printPieces() {
  console.log('Protoengine Piece Board');
  console.log('=======================');
  for (const relPath of criticalFiles) {
    const present = exists(relPath);
    const marker = present ? 'OK' : 'MISSING';
    const kb = present ? `${sizeKb(relPath)} KB` : '-';
    console.log(`${marker.padEnd(8)} ${relPath.padEnd(22)} ${kb}`);
  }
}

function run(command, args) {
  return new Promise((resolve) => {
    const child = spawn(command, args, {
      cwd: rootDir,
      stdio: 'inherit',
      shell: process.platform === 'win32'
    });
    child.on('exit', (code) => resolve(code ?? 1));
  });
}

async function proveStanding() {
  const checks = [
    ['node', ['--check', './browser-launcher.mjs']],
    ['node', ['--check', './proto-tool.mjs']],
    ['node', ['--test', './engine.test.js']]
  ];

  let allPassed = true;
  for (const [cmd, args] of checks) {
    console.log(`\n[check] ${cmd} ${args.join(' ')}`);
    const code = await run(cmd, args);
    if (code !== 0) allPassed = false;
  }

  console.log('\nStanding verdict');
  console.log('===============');
  console.log(allPassed ? 'TOWER_STANDING' : 'TOWER_UNSTABLE');
  process.exit(allPassed ? 0 : 1);
}

const mode = process.argv[2] || 'pieces';
if (mode === 'pieces') {
  printPieces();
} else if (mode === 'proof') {
  await proveStanding();
} else {
  console.error('Usage: node tower.mjs [pieces|proof]');
  process.exit(1);
}

