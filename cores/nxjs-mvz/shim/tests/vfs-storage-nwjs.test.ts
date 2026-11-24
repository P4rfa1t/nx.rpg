import { describe, expect, it } from 'vitest';
import { RpgmAssetDecryptorImpl } from '../src/crypto/rpgm-decryptor';
import { DomShimControllerImpl } from '../src/dom';
import type { FsStat, RuntimeFs } from '../src/fs-adapter';
import { isExitComboPressed, SwitchInputBridgeImpl } from '../src/input';
import { NwjsSandboxShimImpl } from '../src/nwjs';
import { IsolatedSaveStorageImpl } from '../src/storage';
import type { LaunchManifest } from '../src/types';
import { CaseInsensitiveVfsImpl } from '../src/vfs';

class MemoryRuntimeFs implements RuntimeFs {
  private files = new Map<string, Uint8Array>();
  private dirs = new Set<string>(['sdmc:/']);

  private norm(p: string): string {
    return p.replace(/\\/g, '/').replace(/\/+$/, '');
  }

  mkdirSync(path: string): number {
    const parts = this.norm(path).split('/');
    let cur = '';
    for (const part of parts) {
      cur = cur ? `${cur}/${part}` : part;
      this.dirs.add(cur);
    }
    return 0;
  }

  writeFileSync(path: string, data: ArrayBuffer | Uint8Array): void {
    const n = this.norm(path);
    const idx = n.lastIndexOf('/');
    if (idx > 0) this.mkdirSync(n.slice(0, idx));
    const bytes = data instanceof Uint8Array ? new Uint8Array(data) : new Uint8Array(data);
    this.files.set(n, bytes);
  }

  readFileSync(path: string): Uint8Array | null {
    return this.files.get(this.norm(path)) ?? null;
  }

  renameSync(fromPath: string, toPath: string): void {
    const src = this.norm(fromPath);
    const dst = this.norm(toPath);
    const data = this.files.get(src);
    if (!data) throw new Error(`ENOENT: ${src}`);
    this.files.set(dst, data);
    this.files.delete(src);
  }

  removeSync(path: string): void {
    this.files.delete(this.norm(path));
  }

  statSync(path: string): FsStat | null {
    const n = this.norm(path);
    const f = this.files.get(n);
    if (f) return { size: f.byteLength, mtime: 1000, mode: 0o100644 };
    if (this.dirs.has(n)) return { size: 0, mtime: 1000, mode: 0o040755 };
    return null;
  }

  readDirSync(path: string): string[] | null {
    const n = this.norm(path);
    if (!this.dirs.has(n)) return null;
    const prefix = `${n}/`;
    const names = new Set<string>();
    for (const k of this.files.keys()) {
      if (k.startsWith(prefix)) {
        const rest = k.slice(prefix.length);
        if (!rest.includes('/')) names.add(rest);
      }
    }
    for (const d of this.dirs) {
      if (d.startsWith(prefix)) {
        const rest = d.slice(prefix.length);
        if (rest && !rest.includes('/')) names.add(rest);
      }
    }
    return [...names];
  }
}

function makeManifest(): LaunchManifest {
  return {
    schemaVersion: 1,
    sessionId: 'test-session',
    returnTo: 'sdmc:/switch/nx.rpg/nx.rpg.nro',
    logPath: 'sdmc:/switch/nx.rpg/runtime/last_run.log',
    game: {
      id: 'mv-demo-123456',
      title: 'MV Demo',
      engine: 'rmmv',
      core: 'nxjs-mvz',
      gamePath: 'sdmc:/switch/nx.rpg/games/MVDemo',
      contentRoot: 'sdmc:/switch/nx.rpg/games/MVDemo/www',
      savePath: 'sdmc:/switch/nx.rpg/saves/mv-demo-123456',
      execName: 'Game',
    },
    assets: { rtpPath: '', fallbackFontPath: '' },
    runtime: {
      encoding: 'utf-8',
      rgssVersion: 0,
      cpuProfile: 'stock_1020',
      enableTouch: true,
      enableSoftwareKeyboard: true,
    },
    video: { scalingMode: 'fit', integerScale: false, vsync: true, showFps: false },
    input: { profile: 'default', swapConfirmCancel: false, exitCombo: ['L', 'R', 'PLUS'] },
    coreOptions: { mvz: { renderer: 'auto', pretendNwjs: true, maxTextureSize: 4096 } },
  };
}

describe('CaseInsensitiveVfsImpl, IsolatedSaveStorageImpl & NwjsSandboxShimImpl', () => {
  it('resolves mixed-case paths and encrypted fallbacks under contentRoot', async () => {
    const mem = new MemoryRuntimeFs();
    const manifest = makeManifest();
    mem.writeFileSync(
      `${manifest.game.contentRoot}/Audio/BGM/Town1.OGG`,
      new TextEncoder().encode('OggS'),
    );
    mem.writeFileSync(
      `${manifest.game.contentRoot}/img/pictures/Hero.RPGMVP`,
      new Uint8Array([0x52, 0x50, 0x47, 0x4d, 0x56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x89]),
    );

    const dec = new RpgmAssetDecryptorImpl();
    const vfs = new CaseInsensitiveVfsImpl(dec, mem);
    await vfs.buildIndex(manifest.game.contentRoot);

    expect(vfs.resolveAssetPath('audio/bgm/town1.ogg?v=1')).toBe(
      `${manifest.game.contentRoot}/Audio/BGM/Town1.OGG`,
    );
    expect(vfs.resolveAssetPath('./img/pictures/hero.png')).toBe(
      `${manifest.game.contentRoot}/img/pictures/Hero.RPGMVP`,
    );
    expect(vfs.resolveAssetPath('../../secret.txt')).toBeNull();
  });

  it('persists localStorage atomically in saves/<id>/localstorage.json', async () => {
    const mem = new MemoryRuntimeFs();
    const manifest = makeManifest();
    const storage = new IsolatedSaveStorageImpl(mem);
    await storage.install(manifest);

    globalThis.localStorage.setItem('RPG File1', '{"gold":500}');
    expect(globalThis.localStorage.getItem('RPG File1')).toBe('{"gold":500}');

    const saved = mem.readFileSync(`${manifest.game.savePath}/localstorage.json`);
    expect(saved).not.toBeNull();
    expect(mem.readFileSync(`${manifest.game.savePath}/localstorage.json.tmp`)).toBeNull();

    // Re-install from disk verifies persistence
    const storage2 = new IsolatedSaveStorageImpl(mem);
    await storage2.install(manifest);
    expect(globalThis.localStorage.getItem('RPG File1')).toBe('{"gold":500}');
  });

  it('isolates NW.js fs writes to saves/<id>/ while supporting CoW fallback from contentRoot/save', () => {
    const mem = new MemoryRuntimeFs();
    const manifest = makeManifest();
    mem.writeFileSync(
      `${manifest.game.contentRoot}/save/file1.rpgsave`,
      new TextEncoder().encode('legacy-save'),
    );

    const nwjs = new NwjsSandboxShimImpl(mem);
    nwjs.install(manifest);

    const req = (globalThis as any).require as (id: string) => any;
    const fs = req('fs');

    // Reads initial save from contentRoot/save via CoW fallback
    expect(fs.existsSync('save/file1.rpgsave')).toBe(true);
    expect(fs.readFileSync('save/file1.rpgsave', 'utf8')).toBe('legacy-save');

    // Writes go strictly to saves/<id>/save/ without mutating contentRoot
    fs.writeFileSync('save/file1.rpgsave', 'new-isolated-save');
    expect(fs.readFileSync('save/file1.rpgsave', 'utf8')).toBe('new-isolated-save');
    expect(
      new TextDecoder().decode(mem.readFileSync(`${manifest.game.contentRoot}/save/file1.rpgsave`)!),
    ).toBe('legacy-save');
    expect(
      new TextDecoder().decode(mem.readFileSync(`${manifest.game.savePath}/save/file1.rpgsave`)!),
    ).toBe('new-isolated-save');

    // Rejects writes outside save directory
    expect(() => fs.writeFileSync('data/System.json', '{}')).toThrow(/EROFS/);
  });

  it('bridges webgl -> webgl2 on primary screen and keeps secondary canvases offscreen', () => {
    const mem = new MemoryRuntimeFs();
    const manifest = makeManifest();
    const dec = new RpgmAssetDecryptorImpl();
    const vfs = new CaseInsensitiveVfsImpl(dec, mem);

    const g = globalThis as any;
    const prevScreen = g.screen;
    const prevOffscreen = g.OffscreenCanvas;

    const webglCalls: string[] = [];
    g.screen = {
      width: 1280,
      height: 720,
      getContext(id: string) {
        webglCalls.push(id);
        return id === 'webgl2' ? { isWebGL2: true } : null;
      },
    };
    g.OffscreenCanvas = class {
      constructor(public width: number, public height: number) {}
      getContext(id: string) {
        return { isOffscreen2d: id === '2d' };
      }
    };

    try {
      const dom = new DomShimControllerImpl(vfs);
      dom.install(manifest);

      const primaryCanvas = g.document.createElement('canvas');
      const gl = primaryCanvas.getContext('webgl');
      expect(gl).toEqual({ isWebGL2: true });
      expect(webglCalls).toEqual(['webgl2']);

      // Subsequent canvases do NOT touch screen.getContext('2d')
      const bitmapCanvas = g.document.createElement('canvas');
      const ctx2d = bitmapCanvas.getContext('2d');
      expect(ctx2d).toEqual({ isOffscreen2d: true });
      expect(webglCalls).toEqual(['webgl2']);
    } finally {
      g.screen = prevScreen;
      g.OffscreenCanvas = prevOffscreen;
    }
  });

  it('detects L+R+PLUS exit combo accurately', () => {
    const buttons = Array.from({ length: 16 }, () => ({ pressed: false }));
    buttons[6].pressed = true; // L
    buttons[7].pressed = true; // R
    expect(isExitComboPressed(buttons, ['L', 'R', 'PLUS'])).toBe(false);
    buttons[10].pressed = true; // PLUS
    expect(isExitComboPressed(buttons, ['L', 'R', 'PLUS'])).toBe(true);
  });

  it('drives nx.js VirtualKeyboard EventTarget for openSoftwareKeyboard', async () => {
    const g = globalThis as any;
    const prevDesc = Object.getOwnPropertyDescriptor(g, 'navigator');
    const listeners = new Map<string, Array<() => void>>();
    const mockKbd = {
      value: '',
      maxLength: 0,
      boundingRect: { width: 0, height: 0 },
      addEventListener(type: string, fn: () => void) {
        const arr = listeners.get(type) ?? [];
        arr.push(fn);
        listeners.set(type, arr);
      },
      removeEventListener(type: string, fn: () => void) {
        listeners.set(
          type,
          (listeners.get(type) ?? []).filter((f) => f !== fn),
        );
      },
      show() {
        mockKbd.boundingRect.width = 1280;
        for (const fn of listeners.get('geometrychange') ?? []) fn();
        mockKbd.value = '勇者小明';
        for (const fn of listeners.get('submit') ?? []) fn();
      },
    };
    Object.defineProperty(g, 'navigator', {
      value: { virtualKeyboard: mockKbd },
      configurable: true,
    });
    try {
      const bridge = new SwitchInputBridgeImpl();
      const entered = await bridge.openSoftwareKeyboard('Hero', 8);
      expect(entered).toBe('勇者小明');
      expect(mockKbd.maxLength).toBe(8);
    } finally {
      if (prevDesc) {
        Object.defineProperty(g, 'navigator', prevDesc);
      } else {
        delete g.navigator;
      }
    }
  });
});
