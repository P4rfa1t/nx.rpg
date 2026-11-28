import { describe, expect, it, vi } from 'vitest';
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
    logPath: 'sdmc:/switch/nx.rpg/logs/games/mv-demo-123456.log',
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

    // Plain requests auto-decrypt; explicit .rpgmvp requests preserve RPGMV header for MV/MZ Decrypter
    const plainRead = vfs.readAssetBytes('img/pictures/Hero.png');
    expect(plainRead?.bytes[0]).toBe(0x89);
    const encRead = vfs.readAssetBytes('img/pictures/Hero.rpgmvp');
    expect(new TextDecoder().decode(encRead?.bytes.slice(0, 5))).toBe('RPGMV');
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

    // Exposes NW.js App.argv, Window.get().showDevTools, and Clipboard for MV core/plugins
    const g = globalThis as any;
    expect(Array.isArray(g.nw.App.argv)).toBe(true);
    expect(g.nw.App.argv.length).toBe(0);
    expect(typeof g.nw.Window.get().showDevTools).toBe('function');
    expect(typeof g.nw.Clipboard.get().get).toBe('function');
    const start = g.process.hrtime();
    const [seconds, nanoseconds] = g.process.hrtime(start);
    expect(seconds).toBeGreaterThanOrEqual(0);
    expect(nanoseconds).toBeGreaterThanOrEqual(0);
    expect(nanoseconds).toBeLessThan(1_000_000_000);
    expect(typeof g.process.hrtime.bigint()).toBe('bigint');

    // Both engines append filenames directly to a joined directory path.
    const path = req('path');
    const base = path.dirname(g.process.mainModule.filename);
    const directory = path.join(base, 'save/');
    expect(directory).toBe(`${manifest.game.contentRoot}/save/`);
    const configFile = `${directory}config.rpgsave`;
    fs.writeFileSync(configFile, 'config');
    expect(fs.readFileSync(configFile, 'utf8')).toBe('config');
    expect(mem.readFileSync(`${manifest.game.savePath}/save/config.rpgsave`)).not.toBeNull();
    expect(mem.readFileSync(configFile)).toBeNull();

    // MZ's backup rotation must work for both bundled saves and isolated rewrites.
    const mzConfig = `${directory}config.rmmzsave`;
    const backup = `${mzConfig}_`;
    mem.writeFileSync(mzConfig, new TextEncoder().encode('legacy config'));
    mem.writeFileSync(backup, new TextEncoder().encode('legacy backup'));
    const remove = vi.spyOn(mem, 'removeSync');
    fs.unlinkSync(backup);
    expect(remove).not.toHaveBeenCalled();
    remove.mockRestore();
    fs.renameSync(mzConfig, backup);
    expect(fs.readFileSync(backup, 'utf8')).toBe('legacy config');
    fs.writeFileSync(mzConfig, 'isolated config');
    fs.unlinkSync(backup);
    fs.renameSync(mzConfig, backup);
    expect(fs.readFileSync(backup, 'utf8')).toBe('isolated config');
    expect(fs.readFileSync(mzConfig, 'utf8')).toBe('legacy config');
    expect(() => fs.renameSync(backup, `${base}/data/System.json`)).toThrow(/EROFS/);
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
    const texCalls: any[] = [];
    const viewport = vi.fn();
    const scissor = vi.fn();
    const fakeGl = {
      isWebGL2: true,
      drawingBufferWidth: 1280,
      drawingBufferHeight: 720,
      FRAMEBUFFER: 0x8d40,
      READ_FRAMEBUFFER: 0x8ca8,
      DRAW_FRAMEBUFFER: 0x8ca9,
      bindFramebuffer: vi.fn(),
      viewport,
      scissor,
      texImage2D(...args: any[]) {
        texCalls.push(['texImage2D', ...args]);
      },
      texSubImage2D(...args: any[]) {
        texCalls.push(['texSubImage2D', ...args]);
      },
    };
    g.screen = {
      width: 1280,
      height: 720,
      getContext(id: string) {
        webglCalls.push(id);
        return id === 'webgl2' ? fakeGl : null;
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

      // Probe canvas (e.g. Graphics.hasWebGL()) must NOT prevent #GameCanvas from claiming primary WebGL2
      const probeCanvas = g.document.createElement('canvas');
      expect(probeCanvas.getContext('webgl')).toBe(fakeGl);

      const primaryCanvas = g.document.createElement('canvas');
      primaryCanvas.id = 'GameCanvas';
      const gl = primaryCanvas.getContext('webgl');
      expect(gl).toBe(fakeGl);
      expect([primaryCanvas.width, primaryCanvas.height]).toEqual([816, 624]);
      gl.viewport(0, 0, 816, 624);
      expect(viewport).toHaveBeenLastCalledWith(169, 0, 942, 720);
      gl.scissor(20, 30, 200, 100);
      expect(scissor).toHaveBeenLastCalledWith(192, 35, 231, 115);

      gl.bindFramebuffer(gl.FRAMEBUFFER, {});
      gl.viewport(0, 0, 256, 128);
      gl.scissor(2, 3, 24, 32);
      expect(viewport).toHaveBeenLastCalledWith(0, 0, 256, 128);
      expect(scissor).toHaveBeenLastCalledWith(2, 3, 24, 32);
      gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, null);
      gl.bindFramebuffer(gl.READ_FRAMEBUFFER, {});
      primaryCanvas.width = 640;
      primaryCanvas.height = 480;
      gl.viewport(0, 0, 640, 480);
      expect(viewport).toHaveBeenLastCalledWith(160, 0, 960, 720);
      expect(webglCalls).toEqual(['webgl2', 'webgl2']);

      // Subsequent canvases do NOT touch screen.getContext('2d')
      const bitmapCanvas = g.document.createElement('canvas');
      const ctx2d = bitmapCanvas.getContext('2d');
      // 2d contexts come back wrapped: drawImage/createPattern unwrap
      // ShimCanvas/ShimImage sources for Skia (see dom/index.ts wrap2dContext).
      // The fake OffscreenCanvas ctx has no drawImage, so only the marker applies.
      expect(ctx2d.isOffscreen2d).toBe(true);
      expect(ctx2d.__nxrpgUnwrapped).toBe(true);
      expect(webglCalls).toEqual(['webgl2', 'webgl2']);

      // WebGL2 texImage2D (6 args) / texSubImage2D (9 args) must unwrap ShimCanvas
      gl.texImage2D(0, 0, 0, 0, 0, bitmapCanvas);
      expect(texCalls[0][6]).toBe(bitmapCanvas.ensureNativeSurface());
      gl.texSubImage2D(0, 0, 0, 0, 0, 0, 0, 0, bitmapCanvas);
      expect(texCalls[1][9]).toBe(bitmapCanvas.ensureNativeSurface());
    } finally {
      g.screen = prevScreen;
      g.OffscreenCanvas = prevOffscreen;
    }
  });

  it('presents Canvas2D at a centered scale while keeping the logical canvas size', () => {
    const manifest = makeManifest();
    manifest.coreOptions!.mvz!.renderer = 'canvas2d';
    const dom = new DomShimControllerImpl(new CaseInsensitiveVfsImpl(new RpgmAssetDecryptorImpl(), new MemoryRuntimeFs()));
    const screenContext = { fillRect: vi.fn(), drawImage: vi.fn() };
    const offscreenContext = {};
    const OffscreenCanvas = class { getContext() { return offscreenContext; } };
    const requestAnimationFrame = vi.fn();
    vi.stubGlobal('screen', { width: 1280, height: 720, getContext: () => screenContext });
    vi.stubGlobal('OffscreenCanvas', OffscreenCanvas);
    vi.stubGlobal('requestAnimationFrame', requestAnimationFrame);
    try {
      dom.install(manifest);
      const canvas = (globalThis as any).document.createElement('canvas');
      canvas.id = 'GameCanvas';
      canvas.width = 640;
      canvas.height = 480;
      expect(canvas.getContext('2d')).toBe(offscreenContext);
      expect(canvas.getContext('2d')).toBe(offscreenContext);
      expect(requestAnimationFrame).toHaveBeenCalledTimes(1);
      requestAnimationFrame.mock.calls[0][0]();
      expect(screenContext.drawImage).toHaveBeenCalledWith(expect.any(OffscreenCanvas), 160, 0, 960, 720);
      expect([canvas.width, canvas.height]).toEqual([640, 480]);
    } finally {
      vi.unstubAllGlobals();
    }
  });

  it('delivers script errors with the element target and loads plugins before onload', async () => {
    const mem = new MemoryRuntimeFs();
    const manifest = makeManifest();
    const root = manifest.game.contentRoot;
    const put = (path: string, text: string) => mem.writeFileSync(`${root}/${path}`, new TextEncoder().encode(text));
    put('index.html', '<link rel="stylesheet" href="fonts/gamefont.css"><script src="js/main.js"></script>');
    put('fonts/gamefont.css', '@font-face { font-family: GameFont; src: url("test.otf"); }');
    const header = '/* plugin header */\n'.repeat(4096);
    put('js/plugin.js', `${header}
      const nested = document.createElement('script');
      nested.src = 'js/nested.js';
      document.body.appendChild(nested);
    `);
    put('js/nested.js', '/* WAY header */\n"use strict";\nvar WAYModuleLoader = { ok: true };\nglobalThis.__pluginLoaded = true;');
    put('js/main.js', `
      const scripts = document.getElementsByTagName('script');
      globalThis.__lastScriptSrc = scripts[scripts.length - 1]?.src ?? '';
      const plugin = document.createElement('script');
      plugin.src = 'js/plugin.js';
      document.body.appendChild(plugin);
      const missing = document.createElement('script');
      missing.src = 'js/missing.js';
      missing._url = missing.src;
      missing.onerror = e => { globalThis.__missingUrl = e.target._url; };
      document.body.appendChild(missing);
      window.onload = () => {
        globalThis.__loadedBeforeOnload = globalThis.__pluginLoaded;
        globalThis.__wayGlobalSeen = typeof WAYModuleLoader === 'object' && WAYModuleLoader.ok === true;
      };
    `);
    const vfs = new CaseInsensitiveVfsImpl(new RpgmAssetDecryptorImpl(), mem);
    await vfs.buildIndex(root);
    const dom = new DomShimControllerImpl(vfs);
    dom.install(manifest);
    await dom.evaluateIndexHtmlScripts('index.html');
    expect((globalThis as any).__loadedBeforeOnload).toBe(true);
    expect((globalThis as any).__wayGlobalSeen).toBe(true);
    expect((globalThis as any).__missingUrl).toBe('js/missing.js');
    expect((globalThis as any).__lastScriptSrc).toBe('js/main.js');
    expect((globalThis as any).document.fonts.check('10px "GameFont"')).toBe(true);
  });

  it('uses nx.js physical button indices for L+R+PLUS', () => {
    const buttons = Array.from({ length: 16 }, () => ({ pressed: false }));
    buttons[4].pressed = true; // L
    buttons[5].pressed = true; // R
    buttons[10].pressed = true; // LSTICK
    expect(isExitComboPressed(buttons, ['L', 'R', 'PLUS'])).toBe(false);
    buttons[9].pressed = true; // PLUS
    expect(isExitComboPressed(buttons, ['L', 'R', 'PLUS'])).toBe(true);
  });

  it.each([
    { combo: ['L', 'R', 'PLUS'], first: 4, second: 5, last: 9 },
    { combo: ['ZL', 'ZR', 'MINUS'], first: 6, second: 7, last: 8 },
    { combo: ['PLUS', 'PLUS'], first: 4, second: 5, last: 9 },
  ])('blocks native Plus exit and honors the configured chord ($combo)', ({ combo, first, second, last }) => {
    const manifest = makeManifest();
    manifest.runtime.enableTouch = false;
    manifest.input.exitCombo = combo;
    const bridge = new SwitchInputBridgeImpl();
    const onExitRequested = vi.fn();
    const windowEvents = new EventTarget();
    const buttons = Array.from({ length: 16 }, () => ({ pressed: false }));
    const getGamepads = vi.fn(() => [{ connected: true, axes: [0, 0], buttons }]);
    vi.useFakeTimers();
    vi.stubGlobal('navigator', { getGamepads });
    vi.stubGlobal('addEventListener', windowEvents.addEventListener.bind(windowEvents));
    vi.stubGlobal('removeEventListener', windowEvents.removeEventListener.bind(windowEvents));
    vi.stubGlobal('requestAnimationFrame', (callback: () => void) => setTimeout(callback, 16));
    vi.stubGlobal('cancelAnimationFrame', (id: ReturnType<typeof setTimeout>) => clearTimeout(id));

    try {
      bridge.install(manifest, onExitRequested);
      buttons[9].pressed = true;
      // nx.js dispatches beforeunload for a Plus press before it runs animation callbacks.
      expect(windowEvents.dispatchEvent(new Event('beforeunload', { cancelable: true }))).toBe(false);
      vi.advanceTimersByTime(16);
      expect(onExitRequested).not.toHaveBeenCalled();
      buttons[9].pressed = false;

      buttons[first].pressed = true;
      buttons[last].pressed = true;
      vi.advanceTimersByTime(16);
      expect(onExitRequested).not.toHaveBeenCalled();
      buttons[second].pressed = true;
      vi.advanceTimersByTime(16);
      expect(onExitRequested).toHaveBeenCalledTimes(1);

      const polledFrames = getGamepads.mock.calls.length;
      vi.advanceTimersByTime(48);
      expect(getGamepads).toHaveBeenCalledTimes(polledFrames);
      expect(windowEvents.dispatchEvent(new Event('beforeunload', { cancelable: true }))).toBe(true);
    } finally {
      bridge.dispose();
      vi.unstubAllGlobals();
      vi.useRealTimers();
    }
  });

  it.each([false, true])('routes physical buttons through one input path (swap=%s)', (swap) => {
    const manifest = makeManifest();
    manifest.runtime.enableTouch = false;
    manifest.input.swapConfirmCancel = swap;
    const bridge = new SwitchInputBridgeImpl();
    const buttons = Array.from({ length: 16 }, () => ({ pressed: false }));
    const pad = { connected: true, axes: [0, 0], buttons };
    const dispatchEvent = vi.fn();
    const nativePoll = vi.fn();
    const input = { _pollGamepads: nativePoll };
    vi.useFakeTimers();
    vi.stubGlobal('navigator', { getGamepads: () => [null, pad] });
    vi.stubGlobal('document', { dispatchEvent });
    vi.stubGlobal('Input', input);
    vi.stubGlobal('requestAnimationFrame', (callback: () => void) => setTimeout(callback, 16));
    vi.stubGlobal('cancelAnimationFrame', (id: ReturnType<typeof setTimeout>) => clearTimeout(id));

    try {
      bridge.install(manifest, vi.fn());
      input._pollGamepads();
      expect(nativePoll).not.toHaveBeenCalled();

      const mappings = [
        [0, swap ? 'Enter' : 'Escape'], // Switch B
        [1, swap ? 'Escape' : 'Enter'], // Switch A
        [3, 'Escape'], // Switch X opens the menu
        [4, 'PageUp'], [5, 'PageDown'],
        [12, 'ArrowUp'], [13, 'ArrowDown'], [14, 'ArrowLeft'], [15, 'ArrowRight'],
      ] as const;
      for (const [index, key] of mappings) {
        dispatchEvent.mockClear();
        buttons[index].pressed = true;
        vi.advanceTimersByTime(32);
        buttons[index].pressed = false;
        vi.advanceTimersByTime(16);
        expect(dispatchEvent.mock.calls.map(([event]) => [event.type, event.key])).toEqual([
          ['keydown', key], ['keyup', key],
        ]);
      }
      const sampleGameFrame = vi.fn();
      const readInput = () => sampleGameFrame(dispatchEvent.mock.lastCall?.[0]);
      pad.axes[0] = -1;
      requestAnimationFrame(readInput);
      vi.advanceTimersByTime(16);
      expect(sampleGameFrame).toHaveBeenLastCalledWith(expect.objectContaining({ type: 'keydown', key: 'ArrowLeft' }));
      pad.connected = false;
      requestAnimationFrame(readInput);
      vi.advanceTimersByTime(16);
      expect(sampleGameFrame).toHaveBeenLastCalledWith(expect.objectContaining({ type: 'keyup', key: 'ArrowLeft' }));
    } finally {
      bridge.dispose();
      vi.unstubAllGlobals();
      vi.useRealTimers();
    }
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

  it('installs global location, history, navigator, and webkitAudioContext for MV/MZ scripts', () => {
    const g = globalThis as any;
    const prevLoc = g.location;
    const prevHist = g.history;
    const prevWebkitAudio = g.webkitAudioContext;
    const prevAudio = g.AudioContext;
    delete g.location;
    delete g.history;
    delete g.webkitAudioContext;
    g.AudioContext = function MockAudioContext() {};

    try {
      const mem = new MemoryRuntimeFs();
      const manifest = makeManifest();
      const vfs = new CaseInsensitiveVfsImpl(new RpgmAssetDecryptorImpl(), mem);
      const dom = new DomShimControllerImpl(vfs);
      dom.install(manifest);

      expect(g.location).toBeDefined();
      expect(g.location.search).toBe('');
      expect(typeof g.location.reload).toBe('function');
      expect(g.document.location).toBe(g.location);
      expect(g.history).toBeDefined();
      expect(typeof g.history.replaceState).toBe('function');
      expect(g.webkitAudioContext).toBe(g.AudioContext);

      const txt = g.document.createTextNode('.');
      expect(txt.nodeType).toBe(3);
      expect(txt.textContent).toBe('.');
      const div = g.document.createElement('div');
      div.id = 'GameDiv';
      div.appendChild(txt);
      g.document.body.appendChild(div);
      expect(div.firstChild).toBe(txt);
      expect(g.document.body.getElementsByTagName('*')).toContain(div);
      expect(g.document.getElementById('GameDiv')).toBe(div);
      expect(g.document.getElementsByTagName('head').item(0)).toBe(g.document.head);

      const audio = g.document.createElement('audio');
      expect(audio.canPlayType('audio/ogg')).toBe('probably');
      expect(audio.canPlayType('audio/mp4')).toBe('probably');
      const vid = g.document.createElement('video');
      expect(typeof vid.play).toBe('function');

      const style = g.document.createElement('style');
      style.sheet.insertRule('@font-face { font-family: "GameFont"; src: url("fonts/missing.ttf"); }', 0);
      expect(g.document.fonts.check('10px "GameFont"')).toBe(true);
      expect(g.top).toBe(g);
      expect(g.document.hasFocus()).toBe(true);
      expect(g.document.documentElement.clientWidth).toBe(1280);
    } finally {
      g.location = prevLoc;
      g.history = prevHist;
      g.webkitAudioContext = prevWebkitAudio;
      g.AudioContext = prevAudio;
    }
  });
});
