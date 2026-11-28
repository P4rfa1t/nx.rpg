import {
  dirnamePosix,
  getDefaultRuntimeFs,
  isDirectoryMode,
  joinPosix,
  writeFileAtomicSync,
  type RuntimeFs,
} from '../fs-adapter';
import type { LaunchManifest } from '../types';

/**
 * Sandboxed NW.js (`nw`, `require('fs')`, `require('path')`, `process`) compatibility shim.
 *
 * Why needed:
 * - When `Utils.isNwjs()` returns `true`, RPG Maker MV/MZ `StorageManager` writes
 *   `.rpgsave` / `.rmmzsave` files via `require('fs')` instead of `localStorage`,
 *   and many YEP / VisuStella plugins inspect `process.mainModule.filename` or `nw.Window.get()`.
 * - All write operations targeting `<contentRoot>/save/` are transparently redirected
 *   to `manifest.game.savePath` using atomic temp-file writes.
 */
export interface NwjsSandboxShim {
  install(manifest: LaunchManifest): void;
}

function normalizePosix(p: string): string {
  const s = p.replace(/\\/g, '/');
  const hasPrefix = s.match(/^([a-zA-Z0-9_-]+:\/)/)?.[1] ?? (s.startsWith('/') ? '/' : '');
  const rest = hasPrefix ? s.slice(hasPrefix.length) : s;
  const parts: string[] = [];
  for (const seg of rest.split('/')) {
    if (!seg || seg === '.') continue;
    if (seg === '..') {
      if (parts.length > 0) parts.pop();
      continue;
    }
    parts.push(seg);
  }
  return hasPrefix + parts.join('/');
}

export class NwjsSandboxShimImpl implements NwjsSandboxShim {
  private readonly fs: RuntimeFs;
  private contentRoot = '';
  private saveRoot = '';

  constructor(fs?: RuntimeFs) {
    this.fs = fs ?? getDefaultRuntimeFs();
  }

  /**
   * Maps any save-related path (relative `save/...` or `<contentRoot>/save/...`)
   * into `<savePath>/save/...`, returning both the primary isolated write path
   * and an optional Copy-on-Write read fallback inside `<contentRoot>`.
   */
  resolvePath(rawPath: string): { writePath: string; readFallback: string | null } {
    const clean = normalizePosix(rawPath);
    const contentLower = this.contentRoot.toLowerCase();
    const saveLower = this.saveRoot.toLowerCase();

    if (clean.toLowerCase().startsWith(saveLower + '/') || clean.toLowerCase() === saveLower) {
      return { writePath: clean, readFallback: null };
    }

    let rel = clean;
    if (clean.toLowerCase().startsWith(contentLower + '/')) {
      rel = clean.slice(this.contentRoot.length + 1);
    } else if (clean.toLowerCase() === contentLower) {
      rel = '';
    }

    const relLower = rel.toLowerCase();
    if (relLower === 'save' || relLower.startsWith('save/')) {
      return {
        writePath: joinPosix(this.saveRoot, rel),
        readFallback: joinPosix(this.contentRoot, rel),
      };
    }

    // Non-save path inside contentRoot: read-only from contentRoot
    return {
      writePath: joinPosix(this.contentRoot, rel),
      readFallback: null,
    };
  }

  private isInsideSaveRoot(absPath: string): boolean {
    const norm = absPath.toLowerCase();
    const save = this.saveRoot.toLowerCase();
    return norm === save || norm.startsWith(save + '/');
  }

  install(manifest: LaunchManifest): void {
    this.contentRoot = manifest.game.contentRoot.replace(/\\/g, '/').replace(/\/+$/, '');
    this.saveRoot = manifest.game.savePath.replace(/\\/g, '/').replace(/\/+$/, '');

    if (manifest.coreOptions?.mvz?.pretendNwjs === false) {
      return;
    }

    const self = this;
    const pathModule = {
      sep: '/',
      join: (...args: string[]) => {
        const joined = args.filter(Boolean).join('/');
        const normalized = normalizePosix(joined) || '.';
        // MV and MZ concatenate the save filename to path.join(base, 'save/').
        return /[\\/]$/.test(joined) && !normalized.endsWith('/') ? `${normalized}/` : normalized;
      },
      resolve: (...args: string[]) => normalizePosix(joinPosix(self.contentRoot, ...args)),
      dirname: (p: string) => dirnamePosix(p),
      basename: (p: string, ext?: string) => {
        const clean = p.replace(/\\/g, '/').replace(/\/+$/, '');
        const idx = clean.lastIndexOf('/');
        const base = idx >= 0 ? clean.slice(idx + 1) : clean;
        return ext && base.endsWith(ext) ? base.slice(0, -ext.length) : base;
      },
      extname: (p: string) => {
        const clean = p.replace(/\\/g, '/');
        const idx = clean.lastIndexOf('.');
        return idx >= 0 ? clean.slice(idx) : '';
      },
    };

    const fsModule = {
      existsSync(target: string): boolean {
        const { writePath, readFallback } = self.resolvePath(target);
        if (self.fs.statSync(writePath) !== null) return true;
        if (readFallback && self.fs.statSync(readFallback) !== null) return true;
        return false;
      },
      mkdirSync(target: string): void {
        const { writePath } = self.resolvePath(target);
        if (!self.isInsideSaveRoot(writePath)) return;
        self.fs.mkdirSync(writePath, 0o777);
      },
      readFileSync(target: string, opts?: string | { encoding?: string }): string | Uint8Array {
        const { writePath, readFallback } = self.resolvePath(target);
        let raw = self.fs.readFileSync(writePath);
        if (!raw && readFallback) {
          raw = self.fs.readFileSync(readFallback);
        }
        if (!raw) {
          throw new Error(`ENOENT: no such file '${target}'`);
        }
        const bytes = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
        const enc = typeof opts === 'string' ? opts : opts?.encoding;
        if (enc === 'utf8' || enc === 'utf-8') {
          return new TextDecoder().decode(bytes);
        }
        return bytes;
      },
      writeFileSync(target: string, data: string | Uint8Array): void {
        const { writePath } = self.resolvePath(target);
        if (!self.isInsideSaveRoot(writePath)) {
          throw new Error(`EROFS: write blocked outside isolated save directory '${target}'`);
        }
        writeFileAtomicSync(self.fs, writePath, data);
      },
      renameSync(source: string, target: string): void {
        const from = self.resolvePath(source);
        const to = self.resolvePath(target).writePath;
        if (!self.isInsideSaveRoot(from.writePath) || !self.isInsideSaveRoot(to)) {
          throw new Error(`EROFS: rename blocked outside isolated save directory '${source}' -> '${target}'`);
        }
        // MZ backs up an existing save before writing it. Legacy files stay read-only.
        if (!self.fs.statSync(from.writePath) && from.readFallback) {
          const legacy = self.fs.readFileSync(from.readFallback);
          if (legacy) {
            const bytes = legacy instanceof Uint8Array ? legacy : new Uint8Array(legacy);
            writeFileAtomicSync(self.fs, to, bytes);
            return;
          }
        }
        self.fs.renameSync(from.writePath, to);
      },
      unlinkSync(target: string): void {
        const { writePath, readFallback } = self.resolvePath(target);
        if (!self.isInsideSaveRoot(writePath)) return;
        if (readFallback && !self.fs.statSync(writePath) && self.fs.statSync(readFallback)) return;
        self.fs.removeSync(writePath);
      },
      readdirSync(target: string): string[] {
        const { writePath, readFallback } = self.resolvePath(target);
        const merged = new Set<string>();
        for (const n of self.fs.readDirSync(writePath) ?? []) merged.add(n);
        if (readFallback) {
          for (const n of self.fs.readDirSync(readFallback) ?? []) merged.add(n);
        }
        return [...merged];
      },
      statSync(target: string) {
        const { writePath, readFallback } = self.resolvePath(target);
        const st = self.fs.statSync(writePath) ?? (readFallback ? self.fs.statSync(readFallback) : null);
        if (!st) throw new Error(`ENOENT: '${target}'`);
        return {
          size: st.size,
          mtimeMs: st.mtime * 1000,
          isFile: () => !isDirectoryMode(st.mode),
          isDirectory: () => isDirectoryMode(st.mode),
        };
      },
    };

    const g = globalThis as any;
    // Effekseer uses the Node clock whenever it sees the NW.js process object.
    const startedAt = Date.now();
    const nanosecondsPerSecond = 1_000_000_000n;
    const nowNanoseconds = () => BigInt(Math.floor(
      (g.performance?.now() ?? Date.now() - startedAt) * 1_000_000,
    ));
    const hrtime = (previous?: [number, number]): [number, number] => {
      let elapsed = nowNanoseconds();
      if (previous) {
        elapsed -= BigInt(previous[0]) * nanosecondsPerSecond + BigInt(previous[1]);
      }
      const nanoseconds = (elapsed % nanosecondsPerSecond + nanosecondsPerSecond) % nanosecondsPerSecond;
      return [Number((elapsed - nanoseconds) / nanosecondsPerSecond), Number(nanoseconds)];
    };
    hrtime.bigint = nowNanoseconds;
    g.process = {
      platform: 'win32',
      versions: { node: '20.0.0', nw: '0.49.2', 'node-webkit': '0.49.2' },
      argv: [],
      mainModule: { filename: joinPosix(this.contentRoot, 'index.html') },
      cwd: () => this.contentRoot,
      env: {},
      hrtime,
      nextTick: (fn: (...args: any[]) => void, ...args: any[]) =>
        queueMicrotask(() => fn(...args)),
      on: () => g.process,
      once: () => g.process,
      off: () => g.process,
      removeListener: () => g.process,
      emit: () => false,
    };
    const makeWindowHandle = () => ({
      show: () => {},
      focus: () => {},
      close: () => {},
      enterFullscreen: () => {},
      leaveFullscreen: () => {},
      showDevTools: () => {},
      setResizable: () => {},
      resizeTo: () => {},
      moveTo: () => {},
      on: () => {},
    });
    g.nw = {
      App: {
        argv: [],
        fullArgv: [],
        dataPath: this.saveRoot,
        manifest: { name: manifest.game.title },
        quit: () => {},
        on: () => {},
      },
      Window: {
        get: makeWindowHandle,
        open: makeWindowHandle,
      },
      Clipboard: {
        get: () => ({
          get: () => '',
          set: () => {},
          clear: () => {},
        }),
      },
    };
    g.require = (id: string): unknown => {
      if (id === 'fs') return fsModule;
      if (id === 'path') return pathModule;
      if (id === 'nw.gui') return g.nw;
      throw new Error(`Module '${id}' is not available in nx.rpg NW.js sandbox`);
    };
  }
}
