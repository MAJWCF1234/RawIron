/**
 * Proto Engine CLI: asset extract and rebuild-level dispatch.
 * Usage:
 *   node proto-tool.mjs extract
 *   node proto-tool.mjs rebuild-level
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFile as execFileCallback } from 'node:child_process';
import { promisify } from 'node:util';

const execFile = promisify(execFileCallback);
const toolPath = fileURLToPath(import.meta.url);
const rootDir = path.dirname(toolPath);

// --- extract (models/archives) ---

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

/** Run Node with argv[1..] set to `scriptArgs`; never use shell so `Program Files` paths work on Windows. */
function runNodeToExit(scriptArgs) {
  const child = spawn(process.execPath, scriptArgs, {
    cwd: rootDir,
    stdio: 'inherit',
    shell: false
  });
  return new Promise((resolve) => {
    child.on('exit', (code) => resolve(code ?? 0));
  });
}

// --- router ---

async function main() {
  const cmd = process.argv[2];
  if (cmd === 'extract') {
    await runExtract();
    return;
  }
  if (cmd === 'rebuild-level') {
    const code = await runNodeToExit([path.join(rootDir, 'dev-level.build.mjs')]);
    process.exit(code);
    return;
  }
  console.error('Usage: node proto-tool.mjs <extract|rebuild-level>');
  process.exit(1);
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
