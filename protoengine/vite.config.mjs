import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'node:url';
import { transform as esbuildTransform } from 'esbuild';
import { defineConfig } from 'vite';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function isIgnorableWindowsCopyError(error) {
  if (!error || typeof error !== 'object') return false;
  return error.code === 'EACCES' || error.code === 'EPERM' || error.code === 'EIO';
}

function safeCopyDirectory(sourceDir, targetDir) {
  if (!fs.existsSync(sourceDir)) return;
  fs.mkdirSync(targetDir, { recursive: true });
  const entries = fs.readdirSync(sourceDir, { withFileTypes: true });

  for (const entry of entries) {
    const sourcePath = path.join(sourceDir, entry.name);
    const targetPath = path.join(targetDir, entry.name);
    if (entry.isDirectory()) {
      safeCopyDirectory(sourcePath, targetPath);
      continue;
    }
    if (!entry.isFile()) continue;
    try {
      fs.mkdirSync(path.dirname(targetPath), { recursive: true });
      const isCacheFile = sourcePath.includes(`${path.sep}cache${path.sep}`);
      if (isCacheFile) {
        continue;
      }
      fs.copyFileSync(sourcePath, targetPath);
    } catch (error) {
      if (isIgnorableWindowsCopyError(error) && fs.existsSync(targetPath)) {
        console.warn(`[vite copy] keeping existing locked file: ${targetPath}`);
        continue;
      }
      throw error;
    }
  }
}

function copyRuntimeSupportFiles() {
  const fileEntries = ['dev_level.json'];
  const dirEntries = ['models', 'subprojects'];

  return {
    name: 'copy-runtime-support-files',
    closeBundle() {
      const rootDir = __dirname;
      const distDir = path.resolve(rootDir, 'dist');

      for (const relativeFile of fileEntries) {
        const sourcePath = path.resolve(rootDir, relativeFile);
        const targetPath = path.resolve(distDir, relativeFile);
        if (!fs.existsSync(sourcePath)) continue;
        fs.mkdirSync(path.dirname(targetPath), { recursive: true });
        fs.copyFileSync(sourcePath, targetPath);
      }

      for (const relativeDir of dirEntries) {
        const sourcePath = path.resolve(rootDir, relativeDir);
        const targetPath = path.resolve(distDir, relativeDir);
        if (!fs.existsSync(sourcePath)) continue;
        safeCopyDirectory(sourcePath, targetPath);
      }
    }
  };
}

function prepareRuntimeOutDir() {
  return {
    name: 'prepare-runtime-out-dir',
    buildStart() {
      const rootDir = __dirname;
      const distDir = path.resolve(rootDir, 'dist');
      if (!fs.existsSync(distDir)) return;
      const cleanupTargets = [
        path.resolve(distDir, 'assets'),
        path.resolve(distDir, 'index.html'),
        path.resolve(distDir, 'dev_level.json'),
        path.resolve(distDir, 'audio')
      ];
      cleanupTargets.forEach((targetPath) => {
        try {
          if (fs.existsSync(targetPath)) {
            fs.rmSync(targetPath, { recursive: true, force: true });
          }
        } catch (error) {
          if (isIgnorableWindowsCopyError(error)) {
            console.warn(`[vite cleanup] keeping locked path: ${targetPath}`);
            return;
          }
          throw error;
        }
      });
    }
  };
}

function slimVendorImports() {
  const vendorPathPattern = /[\\/]node_modules[\\/]/;
  const extensionPattern = /\.(m?js|cjs|jsx|ts|tsx)(?:\?.*)?$/i;

  const toLoader = (id) => {
    if (/\.tsx(?:\?.*)?$/i.test(id)) return 'tsx';
    if (/\.ts(?:\?.*)?$/i.test(id)) return 'ts';
    if (/\.jsx(?:\?.*)?$/i.test(id)) return 'jsx';
    return 'js';
  };

  return {
    name: 'slim-vendor-imports',
    apply: 'build',
    enforce: 'pre',
    async transform(code, id) {
      if (!vendorPathPattern.test(id) || !extensionPattern.test(id)) return null;
      const result = await esbuildTransform(code, {
        loader: toLoader(id),
        minify: true,
        legalComments: 'none',
        target: 'es2020',
        sourcemap: false
      });
      return { code: result.code, map: null };
    }
  };
}

export default defineConfig({
    plugins: [prepareRuntimeOutDir(), copyRuntimeSupportFiles(), slimVendorImports()],
    optimizeDeps: {
      entries: ['index.html'],
      noDiscovery: true
    },
    server: {
      preTransformRequests: false
    },
    resolve: {
      alias: {
        '@': path.resolve(__dirname, '.')
      }
    },
    build: {
      emptyOutDir: false,
      minify: 'esbuild',
      sourcemap: false,
      esbuild: {
        legalComments: 'none'
      },
      reportCompressedSize: false,
      target: 'es2020',
      chunkSizeWarningLimit: 700,
      rollupOptions: {
        output: {
          manualChunks(id) {
            if (id.includes('node_modules/three/build/')) return 'three-core';
            if (id.includes('node_modules/three/examples/')) return 'three-extras';
            if (id.includes('/assetLoader.js') || id.includes('/constants.js')) return 'game-data';
            if (id.includes('/engine.js') || id.includes('/spatialIndex.js')) {
              return 'game-systems';
            }
          }
        }
      }
    }
});
