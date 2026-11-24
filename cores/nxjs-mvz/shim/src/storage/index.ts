import {
  getDefaultRuntimeFs,
  joinPosix,
  writeFileAtomicSync,
  type RuntimeFs,
} from '../fs-adapter';
import type { LaunchManifest } from '../types';

/**
 * Per-game isolated `localStorage` & atomic save writer under `sdmc:/switch/nx.rpg/saves/<id>/`.
 *
 * Replaces `nx.js`'s built-in `localStorage` (which mounts Switch NAND account save data
 * and triggers the system Profile Selector popup).
 */
export interface IsolatedSaveStorage {
  install(manifest: LaunchManifest): Promise<void>;
  flushSync(): void;
}

export class IsolatedSaveStorageImpl implements IsolatedSaveStorage {
  private readonly fs: RuntimeFs;
  private storeFile = '';
  private data = new Map<string, string>();
  private dirty = false;

  constructor(fs?: RuntimeFs) {
    this.fs = fs ?? getDefaultRuntimeFs();
  }

  async install(manifest: LaunchManifest): Promise<void> {
    const saveDir = manifest.game.savePath.replace(/\\/g, '/').replace(/\/+$/, '');
    this.fs.mkdirSync(saveDir, 0o777);
    this.storeFile = joinPosix(saveDir, 'localstorage.json');
    this.data.clear();
    this.dirty = false;

    const raw = this.fs.readFileSync(this.storeFile);
    if (raw) {
      try {
        const bytes = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
        const parsed = JSON.parse(new TextDecoder().decode(bytes)) as Record<string, string>;
        if (parsed && typeof parsed === 'object') {
          for (const [k, v] of Object.entries(parsed)) {
            if (typeof v === 'string') {
              this.data.set(k, v);
            }
          }
        }
      } catch {
        // Corrupt localstorage.json starts fresh rather than crashing boot
      }
    }

    const self = this;
    const storageShim: Storage = {
      get length(): number {
        return self.data.size;
      },
      clear(): void {
        if (self.data.size > 0) {
          self.data.clear();
          self.dirty = true;
          self.flushSync();
        }
      },
      getItem(key: string): string | null {
        const v = self.data.get(String(key));
        return v === undefined ? null : v;
      },
      key(index: number): string | null {
        let i = 0;
        for (const k of self.data.keys()) {
          if (i === index) return k;
          i++;
        }
        return null;
      },
      removeItem(key: string): void {
        if (self.data.delete(String(key))) {
          self.dirty = true;
          self.flushSync();
        }
      },
      setItem(key: string, value: string): void {
        self.data.set(String(key), String(value));
        self.dirty = true;
        self.flushSync();
      },
    };

    Object.defineProperty(globalThis, 'localStorage', {
      value: storageShim,
      configurable: true,
      writable: true,
    });
  }

  flushSync(): void {
    if (!this.dirty || !this.storeFile) return;
    const obj: Record<string, string> = {};
    for (const [k, v] of this.data.entries()) {
      obj[k] = v;
    }
    writeFileAtomicSync(this.fs, this.storeFile, JSON.stringify(obj));
    this.dirty = false;
  }
}
