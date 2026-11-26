import { mimeTypeForAssetPath } from '../crypto/rpgm-decryptor';
import type { LaunchManifest } from '../types';
import type { CaseInsensitiveVfsImpl } from '../vfs';

/**
 * Minimal HTML5 DOM & Canvas/WebGL bridge over `nx.js` globals.
 *
 * Key `nx.js` runtime constraints handled here:
 * - `nx.js` `screen` (1280x720) allows only ONE context kind (`'2d'` OR `'webgl2'`),
 *   and does not expose `'webgl'` / `'experimental-webgl'` directly.
 * - RPG Maker MV (PixiJS v4) probes `canvas.getContext('webgl')` first:
 *   this shim maps `'webgl'` -> `screen.getContext('webgl2')` when `renderer !== 'canvas2d'`.
 * - Offscreen `document.createElement('canvas')` delegates to `nx.js` `OffscreenCanvas`
 *   (Skia 2D) for RPG Maker `Bitmap` text and window skin rendering.
 */
export interface DomShimController {
  install(manifest: LaunchManifest): void;
  evaluateIndexHtmlScripts(indexHtmlPath: string): Promise<void>;
}

export class DomShimControllerImpl implements DomShimController {
  private primaryClaimed = false;
  private readonly vfs: CaseInsensitiveVfsImpl;
  private onOpenSwkbd?: (initial: string, maxLen: number) => Promise<string | null>;
  /** Diagnostics channel writing into the per-game run log (guaranteed to
   * reach the SD card even when V8 console output is not redirected). */
  private runLog?: (line: string) => void;

  constructor(
    vfs: CaseInsensitiveVfsImpl,
    onOpenSwkbd?: (initial: string, maxLen: number) => Promise<string | null>,
    runLog?: (line: string) => void,
  ) {
    this.vfs = vfs;
    this.onOpenSwkbd = onOpenSwkbd;
    this.runLog = runLog;
  }

  install(manifest: LaunchManifest): void {
    const g = globalThis as any;
    const vfs = this.vfs;
    const self = this;
    const preferredRenderer = manifest.coreOptions?.mvz?.renderer ?? 'auto';
    const scr = g.screen;

    class ShimEventTarget {
      private _listeners = new Map<string, Array<(e?: any) => void>>();
      addEventListener(type: string, fn: (e?: any) => void): void {
        const list = this._listeners.get(type) ?? [];
        list.push(fn);
        this._listeners.set(type, list);
      }
      removeEventListener(type: string, fn: (e?: any) => void): void {
        const list = this._listeners.get(type);
        if (!list) return;
        this._listeners.set(
          type,
          list.filter((f) => f !== fn),
        );
      }
      dispatchEvent(ev: any): boolean {
        for (const fn of this._listeners.get(ev?.type) ?? []) {
          fn(ev);
        }
        return true;
      }
    }

    class ShimElement extends ShimEventTarget {
      tagName: string;
      style: Record<string, string> = {};
      id = '';
      className = '';
      children: ShimElement[] = [];
      parentNode: ShimElement | null = null;
      private _src = '';
      onload: (() => void) | null = null;
      onerror: ((e?: any) => void) | null = null;

      constructor(tag: string) {
        super();
        this.tagName = tag.toUpperCase();
      }

      get src(): string {
        return this._src;
      }
      set src(val: string) {
        this._src = val;
      }
      appendChild(child: ShimElement): ShimElement {
        child.parentNode = this;
        this.children.push(child);
        if (child.tagName === 'SCRIPT' && child.src) {
          queueMicrotask(() => {
            let asset: ReturnType<typeof vfs.readAssetBytes>;
            try {
              asset = vfs.readAssetBytes(child.src);
              if (!asset) throw new Error(`Script not found: ${child.src}`);
            } catch (err) {
              console.error(`[nx.rpg] Script load failed: ${child.src}`, err);
              const event = { type: 'error', target: child, currentTarget: child, error: err };
              try {
                child.onerror?.call(child, event);
                child.dispatchEvent(event);
              } catch (handlerError) {
                console.error(`[nx.rpg] Script error handler failed: ${child.src}`, handlerError);
              }
              return;
            }
            try {
              (0, eval)(new TextDecoder().decode(asset.bytes));
            } catch (err) {
              // Browsers report execution exceptions separately from resource-load errors.
              console.error(`[nx.rpg] Script execution failed: ${child.src}`, err);
              return;
            }
            try {
              child.onload?.call(child);
              child.dispatchEvent({ type: 'load', target: child, currentTarget: child });
            } catch (err) {
              console.error(`[nx.rpg] Script load handler failed: ${child.src}`, err);
            }
          });
        }
        return child;
      }
      removeChild(child: ShimElement): ShimElement {
        this.children = this.children.filter((c) => c !== child);
        child.parentNode = null;
        return child;
      }
      setAttribute(): void {}
      getAttribute(): string | null {
        return null;
      }
      getContext(_id: string): any {
        return null;
      }
      focus(): void {}
      blur(): void {}
      getBoundingClientRect() {
        return { left: 0, top: 0, width: scr?.width ?? 1280, height: scr?.height ?? 720 };
      }
    }

    class ShimCanvas extends ShimElement {
      private _width = 816;
      private _height = 624;
      private _offscreen: any = null;
      private _isPrimary = false;

      constructor() {
        super('CANVAS');
      }

      get width(): number {
        return this._isPrimary && scr ? scr.width : this._width;
      }
      set width(v: number) {
        this._width = v;
        if (this._offscreen) this._offscreen.width = v;
      }
      get height(): number {
        return this._isPrimary && scr ? scr.height : this._height;
      }
      set height(v: number) {
        this._height = v;
        if (this._offscreen) this._offscreen.height = v;
      }

      /* Native surface backing this shim canvas. Skia's drawImage /
       * createPattern reject plain JS objects ("Image or Canvas expected"),
       * so 2D contexts must unwrap ShimCanvas/ShimImage before native calls
       * (PixiJS v4 probes blend modes by canvas→canvas drawImage at load). */
      ensureNativeSurface(): any {
        if (this._isPrimary && scr) return scr;
        if (!this._offscreen && typeof g.OffscreenCanvas === 'function') {
          this._offscreen = new g.OffscreenCanvas(this._width || 1, this._height || 1);
        }
        return this._offscreen;
      }

      override getContext(contextId: string, _opts?: any): any {
        const lower = String(contextId).toLowerCase();
        const wantsWebgl =
          lower === 'webgl2' || lower === 'webgl' || lower === 'experimental-webgl';

        if (wantsWebgl && preferredRenderer !== 'canvas2d' && !self.primaryClaimed && scr) {
          const gl = scr.getContext('webgl2');
          if (gl) {
            self.primaryClaimed = true;
            this._isPrimary = true;
            return gl;
          }
        }

        if (lower === '2d' && !self.primaryClaimed && preferredRenderer === 'canvas2d' && scr) {
          self.primaryClaimed = true;
          this._isPrimary = true;
          return wrap2dContext(scr.getContext('2d'));
        }

        const native = this.ensureNativeSurface();
        return native ? wrap2dContext(native.getContext('2d')) : null;
      }
    }

    /* nx.js native Image / OffscreenCanvas instances live one hop inside our
     * shim elements; unwrap them so native 2D calls never see a plain object. */
    function unwrapCanvasSource(src: any): any {
      if (!src) return src;
      if (src instanceof ShimCanvas) return src.ensureNativeSurface();
      if (src instanceof ShimImage) return src.nativeImg;
      return src;
    }

    function wrap2dContext(nativeCtx: any): any {
      if (!nativeCtx || (nativeCtx as any).__nxrpgUnwrapped) return nativeCtx;
      const ctx = nativeCtx as any;
      if (typeof ctx.drawImage === 'function') {
        const origDrawImage = ctx.drawImage.bind(ctx);
        ctx.drawImage = (src: any, ...rest: any[]) => origDrawImage(unwrapCanvasSource(src), ...rest);
      }
      if (typeof ctx.createPattern === 'function') {
        const origCreatePattern = ctx.createPattern.bind(ctx);
        ctx.createPattern = (src: any, ...rest: any[]) =>
          origCreatePattern(unwrapCanvasSource(src), ...rest);
      }
      ctx.__nxrpgUnwrapped = true;
      return ctx;
    }

    class ShimInputElement extends ShimElement {
      value = '';
      maxLength = 32;
      type = 'text';
      constructor() {
        super('INPUT');
      }
      override focus(): void {
        if (!manifest.runtime.enableSoftwareKeyboard || !self.onOpenSwkbd) return;
        void self.onOpenSwkbd(this.value, this.maxLength).then((res) => {
          if (res !== null) {
            this.value = res;
            this.dispatchEvent({ type: 'input', target: this });
            this.dispatchEvent({ type: 'change', target: this });
          }
        });
      }
    }

    const NativeImage = g.Image;
    class ShimImage extends ShimElement {
      complete = false;
      naturalWidth = 0;
      naturalHeight = 0;
      nativeImg: any = null;

      constructor() {
        super('IMG');
      }

      override get src(): string {
        return super.src;
      }
      override set src(val: string) {
        super.src = val;
        this.complete = false;
        const asset = vfs.readAssetBytes(val);
        if (!asset) {
          queueMicrotask(() => {
            if (this.onerror) this.onerror(new Error(`Image not found: ${val}`));
            this.dispatchEvent({ type: 'error' });
          });
          return;
        }
        if (typeof NativeImage === 'function') {
          const img = new NativeImage();
          this.nativeImg = img;
          img.onload = () => {
            this.naturalWidth = img.width ?? img.naturalWidth ?? 0;
            this.naturalHeight = img.height ?? img.naturalHeight ?? 0;
            this.complete = true;
            if (this.onload) this.onload();
            this.dispatchEvent({ type: 'load' });
          };
          img.onerror = (err: any) => {
            if (this.onerror) this.onerror(err);
            this.dispatchEvent({ type: 'error' });
          };
          const blob = new Blob([asset.bytes as any], { type: mimeTypeForAssetPath(asset.actualPath) });
          img.src = URL.createObjectURL(blob);
        }
      }
    }

    const body = new ShimElement('BODY');
    const head = new ShimElement('HEAD');
    const doc = Object.assign(new ShimElement('DOCUMENT'), {
      body,
      head,
      documentElement: body,
      title: manifest.game.title,
      visibilityState: 'visible',
      hidden: false,
      fonts: {
        add: () => {},
        ready: Promise.resolve(),
      },
      createElement(tag: string) {
        const t = tag.toLowerCase();
        if (t === 'canvas') return new ShimCanvas();
        if (t === 'img') return new ShimImage();
        if (t === 'input' || t === 'textarea') return new ShimInputElement();
        return new ShimElement(tag);
      },
      getElementById: () => null,
      querySelector: () => null,
      querySelectorAll: () => [],
      getElementsByTagName: () => [],
    });

    g.document = doc;
    g.window = g;
    g.self = g;
    g.Image = ShimImage;
    g.HTMLCanvasElement = ShimCanvas;
    g.HTMLImageElement = ShimImage;
    g.innerWidth = scr?.width ?? 1280;
    g.innerHeight = scr?.height ?? 720;
    g.devicePixelRatio = 1;
    if (typeof g.requestAnimationFrame !== 'function') {
      g.requestAnimationFrame = (cb: (t: number) => void) => setTimeout(() => cb(Date.now()), 16);
      g.cancelAnimationFrame = (id: number) => clearTimeout(id);
    }
  }

  async evaluateIndexHtmlScripts(indexHtmlPath: string): Promise<void> {
    const asset = this.vfs.readAssetBytes(indexHtmlPath);
    if (!asset) {
      throw new Error(`Missing ${indexHtmlPath} in contentRoot`);
    }
    const html = new TextDecoder().decode(asset.bytes);
    const scriptRegex = /<script\b[^>]*\bsrc\s*=\s*["']([^"']+)["'][^>]*>/gi;
    const scripts: string[] = [];
    let match: RegExpExecArray | null;
    while ((match = scriptRegex.exec(html)) !== null) {
      scripts.push(match[1]);
    }
    if (scripts.length === 0) {
      // Standard RPG Maker MV/MZ script order fallback
      scripts.push('js/main.js');
    }

    for (const scriptRel of scripts) {
      const scriptAsset = this.vfs.readAssetBytes(scriptRel);
      if (!scriptAsset) {
        throw new Error(`Missing script referenced by index.html: ${scriptRel}`);
      }
      const code = new TextDecoder().decode(scriptAsset.bytes);
      (0, eval)(code);
      // Dynamic scripts with async=false must run before the page load handler.
      await new Promise<void>((resolve) => queueMicrotask(resolve));
    }

    const g = globalThis as any;
    // MV's error screen can itself fail before Graphics has created its canvas.
    // Log the original exception into the run log BEFORE its handler attempts
    // to render it (V8 console output does not reliably reach the log file).
    const describeErr = (err: unknown): string => {
      if (err instanceof Error) return `${err.name}: ${err.message}\n${err.stack ?? ''}`;
      return String(err);
    };
    const sceneManager = g.SceneManager;
    if (sceneManager && typeof sceneManager.catchException === 'function') {
      const self = this;
      const originalCatchException = sceneManager.catchException;
      sceneManager.catchException = function (error: unknown) {
        try { self.runLog?.(`[ERROR] SceneManager exception: ${describeErr(error)}`); }
        catch { /* never break the error path */ }
        console.error('[nx.rpg] SceneManager exception:', error);
        return originalCatchException.call(this, error);
      };
    }
    // Surface unhandled promise rejections too (MV/MZ async plugin failures).
    if (typeof g.addEventListener === 'function') {
      g.addEventListener('unhandledrejection', (ev: any) => {
        const reason = ev?.reason ?? ev?.detail?.reason ?? ev;
        try { this.runLog?.(`[ERROR] Unhandled rejection: ${describeErr(reason)}`); }
        catch { /* ignore */ }
        console.error('[nx.rpg] Unhandled rejection:', reason);
      });
    }
    if (typeof g.onload === 'function') {
      g.onload();
    }
    if (g.document && typeof g.document.dispatchEvent === 'function') {
      g.document.dispatchEvent({ type: 'DOMContentLoaded' });
    }
    if (typeof g.dispatchEvent === 'function') {
      g.dispatchEvent({ type: 'load' });
    }
  }
}
