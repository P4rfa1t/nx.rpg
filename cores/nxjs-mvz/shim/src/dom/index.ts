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
  evaluateIndexHtmlScripts(indexHtmlPath: string, beforeLoad?: () => void): Promise<void>;
}

export class DomShimControllerImpl implements DomShimController {
  private pendingDynamicScripts = 0;
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
    const stripTopLevelUseStrict = (code: string): string => {
      // Linear O(n) scanner to strip only a leading top-level "use strict" / 'use strict'
      // directive without regex backtracking on comment-heavy files (e.g. rpg_core.js).
      let i = 0;
      const n = code.length;
      while (i < n) {
        const ch = code.charCodeAt(i);
        if (ch === 32 || ch === 9 || ch === 10 || ch === 13 || ch === 0xfeff) {
          i++;
          continue;
        }
        if (ch === 47 /* '/' */ && i + 1 < n) {
          const next = code.charCodeAt(i + 1);
          if (next === 47 /* '//' */) {
            i += 2;
            while (i < n && code.charCodeAt(i) !== 10 && code.charCodeAt(i) !== 13) i++;
            continue;
          }
          if (next === 42 /* '/*' */) {
            const end = code.indexOf('*/', i + 2);
            if (end < 0) return code;
            i = end + 2;
            continue;
          }
        }
        break;
      }
      const quote = code[i];
      if ((quote === '"' || quote === "'") && code.startsWith('use strict', i + 1)) {
        const afterQuote = i + 1 + 10;
        if (code[afterQuote] === quote) {
          let j = afterQuote + 1;
          while (j < n && (code.charCodeAt(j) === 32 || code.charCodeAt(j) === 9)) j++;
          if (code[j] === ';') return code.slice(0, i) + code.slice(j + 1);
          // Keep the string's value and line breaks when the statement uses ASI.
          // Escaping a character makes this string ineligible as a strict directive.
          return code.slice(0, i + 1) + '\\x75' + code.slice(i + 2);
        }
      }
      return code;
    };

    const evalClassicScript = (code: string) => {
      (0, eval)(stripTopLevelUseStrict(code));
    };
    (this as any)._evalClassicScript = evalClassicScript;

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
      nodeType = 1;
      style: Record<string, string | number> = {};
      id = '';
      className = '';
      innerHTML = '';
      textContent = '';
      clientWidth = 0;
      clientHeight = 0;
      offsetWidth = 0;
      offsetHeight = 0;
      children: ShimElement[] = [];
      parentNode: ShimElement | null = null;
      private _src = '';
      private _attrs = new Map<string, string>();
      /** Internal flag used when registering static index.html <script> tags. */
      _nxrpgStaticScript = false;
      onload: (() => void) | null = null;
      onerror: ((e?: any) => void) | null = null;
      oncontextmenu: (() => boolean) | null = null;

      constructor(tag: string, nodeType = 1) {
        super();
        this.tagName = tag.toUpperCase();
        this.nodeType = nodeType;
      }

      get childNodes(): ShimElement[] {
        return this.children;
      }
      get firstChild(): ShimElement | null {
        return this.children[0] ?? null;
      }
      get outerHTML(): string {
        const t = this.tagName.toLowerCase();
        const idAttr = this.id ? ` id="${this.id}"` : '';
        return `<${t}${idAttr}>${this.innerHTML}</${t}>`;
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
        if (child.tagName === 'SCRIPT' && child.src && !child._nxrpgStaticScript) {
          self.pendingDynamicScripts++;
          queueMicrotask(() => {
            try {
              let asset: ReturnType<typeof vfs.readAssetBytes>;
              try {
                asset = vfs.readAssetBytes(child.src);
                if (!asset) throw new Error(`Script not found: ${child.src}`);
              } catch (err) {
                self.runLog?.(`[ERROR] Script load failed: ${child.src}`);
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
                if (g.document) g.document.currentScript = child;
                evalClassicScript(new TextDecoder().decode(asset.bytes));
              } catch (err) {
                // Browsers report execution exceptions separately from resource-load errors.
                const msg = err instanceof Error ? `${err.name}: ${err.message}` : String(err);
                self.runLog?.(`[ERROR] Script execution failed: ${child.src} (${msg})`);
                console.error(`[nx.rpg] Script execution failed: ${child.src}`, err);
                return;
              } finally {
                if (g.document) g.document.currentScript = null;
              }
              try {
                child.onload?.call(child);
                child.dispatchEvent({ type: 'load', target: child, currentTarget: child });
              } catch (err) {
                console.error(`[nx.rpg] Script load handler failed: ${child.src}`, err);
              }
            } finally {
              self.pendingDynamicScripts--;
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
      insertBefore(newChild: ShimElement, refChild: ShimElement | null): ShimElement {
        newChild.parentNode = this;
        const idx = refChild ? this.children.indexOf(refChild) : -1;
        if (idx >= 0) {
          this.children.splice(idx, 0, newChild);
        } else {
          this.children.push(newChild);
        }
        return newChild;
      }
      setAttribute(name: string, value: string): void {
        const k = String(name);
        const v = String(value);
        this._attrs.set(k, v);
        if (k === 'id') this.id = v;
        if (k === 'class') this.className = v;
        if (k === 'src') this._src = v;
      }
      getAttribute(name: string): string | null {
        const k = String(name);
        if (k === 'id') return this.id || null;
        if (k === 'class') return this.className || null;
        if (k === 'src') return this._src || null;
        return this._attrs.get(k) ?? null;
      }
      getAttributeNames(): string[] {
        return [...this._attrs.keys()];
      }
      getContext(_id: string): any {
        return null;
      }
      focus(): void {}
      blur(): void {}
      getBoundingClientRect() {
        return { left: 0, top: 0, width: scr?.width ?? 1280, height: scr?.height ?? 720 };
      }

      private collectDescendants(pred: (el: ShimElement) => boolean, out: ShimElement[] = []): ShimElement[] {
        for (const child of this.children) {
          if (child.nodeType === 1 && pred(child)) {
            out.push(child);
          }
          child.collectDescendants(pred, out);
        }
        return out;
      }

      getElementsByTagName(tagName: string): ShimElement[] & { item(index: number): ShimElement | null } {
        const upper = String(tagName).toUpperCase();
        const matches = this.collectDescendants((el) => upper === '*' || el.tagName === upper);
        const col = matches as ShimElement[] & { item(index: number): ShimElement | null };
        col.item = (index: number) => col[index] ?? null;
        return col;
      }

      getElementById(id: string): ShimElement | null {
        const target = String(id);
        return this.collectDescendants((el) => el.id === target)[0] ?? null;
      }

      querySelectorAll(selector: string): ShimElement[] {
        const sel = String(selector).trim();
        if (!sel) return [];
        if (sel.startsWith('#')) {
          const id = sel.slice(1);
          return this.collectDescendants((el) => el.id === id);
        }
        if (sel.startsWith('.')) {
          const cls = sel.slice(1);
          return this.collectDescendants((el) => el.className.split(/\s+/).includes(cls));
        }
        const upper = sel.toUpperCase();
        return this.collectDescendants((el) => upper === '*' || el.tagName === upper);
      }

      querySelector(selector: string): ShimElement | null {
        return this.querySelectorAll(selector)[0] ?? null;
      }
    }

    let primaryWebglCanvas: ShimCanvas | null = null;
    let primary2dCanvas: ShimCanvas | null = null;

    function screenFrame(width: number, height: number, screenWidth: number, screenHeight: number) {
      const mode = manifest.video?.scalingMode ?? 'fit';
      if (mode === 'stretch') return { x: 0, y: 0, scaleX: screenWidth / width, scaleY: screenHeight / height };

      const fit = Math.min(screenWidth / width, screenHeight / height);
      const scale = mode === 'native' ? 1 : manifest.video?.integerScale && fit >= 1 ? Math.floor(fit) : fit;
      return {
        x: Math.floor((screenWidth - width * scale) / 2),
        y: Math.floor((screenHeight - height * scale) / 2),
        scaleX: scale,
        scaleY: scale,
      };
    }

    class ShimCanvas extends ShimElement {
      private _width = 816;
      private _height = 624;
      private _offscreen: any = null;

      constructor() {
        super('CANVAS');
      }

      get width(): number {
        return this._width;
      }
      set width(v: number) {
        this._width = v;
        if (this._offscreen) this._offscreen.width = v;
      }
      get height(): number {
        return this._height;
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
        if (!this._offscreen && typeof g.OffscreenCanvas === 'function') {
          this._offscreen = new g.OffscreenCanvas(this._width || 1, this._height || 1);
        }
        return this._offscreen;
      }

      override getContext(contextId: string, _opts?: any): any {
        const lower = String(contextId).toLowerCase();
        const wantsWebgl =
          lower === 'webgl2' || lower === 'webgl' || lower === 'experimental-webgl';

        if (wantsWebgl && preferredRenderer !== 'canvas2d' && scr) {
          const gl = scr.getContext('webgl2');
          if (gl) {
            if (this.id === 'GameCanvas' || (this.parentNode !== null && !primaryWebglCanvas)) {
              primaryWebglCanvas = this;
            }
            return wrapWebGL2Context(gl);
          }
        }

        if (lower !== '2d') return null;

        const native = this.ensureNativeSurface();
        const context = native?.getContext('2d');
        const isMainScreenCanvas = this.id === 'GameCanvas' ||
          (preferredRenderer === 'canvas2d' && this.parentNode !== null);
        if (context && isMainScreenCanvas && scr && !primaryWebglCanvas && !primary2dCanvas) {
          const screenContext = scr.getContext('2d');
          if (screenContext) {
            primary2dCanvas = this;
            const present = () => {
              const { width, height } = this;
              if (width > 0 && height > 0) {
                const frame = screenFrame(width, height, scr.width, scr.height);
                screenContext.fillStyle = '#000';
                screenContext.fillRect(0, 0, scr.width, scr.height);
                screenContext.drawImage(native, frame.x, frame.y, width * frame.scaleX, height * frame.scaleY);
              }
              g.requestAnimationFrame(present);
            };
            g.requestAnimationFrame(present);
          }
        }
        return context ? wrap2dContext(context) : null;
      }
    }

    /* nx.js native Image / OffscreenCanvas instances live one hop inside our
     * shim elements; unwrap them so native 2D/WebGL2 calls never see a plain object. */
    function unwrapCanvasSource(src: any): any {
      if (!src) return src;
      if (src instanceof ShimCanvas) return src.ensureNativeSurface();
      if (src instanceof ShimImage) return src.nativeImg;
      return src;
    }

    function wrapWebGL2Context(nativeGl: any): any {
      if (!nativeGl || (nativeGl as any).__nxrpgUnwrapped) return nativeGl;
      const gl = nativeGl as any;
      if (typeof gl.bindFramebuffer === 'function' && typeof gl.viewport === 'function') {
        let drawingToScreen = true;
        const bindFramebuffer = gl.bindFramebuffer.bind(gl);
        gl.bindFramebuffer = (target: number, framebuffer: unknown) => {
          bindFramebuffer(target, framebuffer);
          if (target === gl.FRAMEBUFFER || target === gl.DRAW_FRAMEBUFFER) {
            drawingToScreen = framebuffer == null;
          }
        };

        // Pixi uses logical canvas coordinates. Transform only the window framebuffer;
        // render textures and their filter/scissor passes keep their own pixel sizes.
        for (const method of ['viewport', 'scissor'] as const) {
          if (typeof gl[method] !== 'function') continue;
          const apply = gl[method].bind(gl);
          gl[method] = (x: number, y: number, width: number, height: number) => {
            const canvas = primaryWebglCanvas;
            if (!drawingToScreen || !canvas || canvas.width <= 0 || canvas.height <= 0) {
              apply(x, y, width, height);
              return;
            }
            const frame = screenFrame(canvas.width, canvas.height, gl.drawingBufferWidth, gl.drawingBufferHeight);
            const left = Math.round(x * frame.scaleX);
            const bottom = Math.round(y * frame.scaleY);
            apply(frame.x + left, frame.y + bottom,
              Math.round((x + width) * frame.scaleX) - left,
              Math.round((y + height) * frame.scaleY) - bottom);
          };
        }
      }
      for (const method of ['texImage2D', 'texSubImage2D'] as const) {
        if (typeof gl[method] === 'function') {
          const orig = gl[method].bind(gl);
          gl[method] = (...args: any[]) => {
            if (args.length >= 6) {
              const last = args[args.length - 1];
              const unwrapped = unwrapCanvasSource(last);
              if (unwrapped !== last) {
                args = [...args.slice(0, -1), unwrapped];
              }
            }
            return orig(...args);
          };
        }
      }
      gl.__nxrpgUnwrapped = true;
      return gl;
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
      // nx.js Skia 2D context returns without setting a font if none of the
      // requested font-family names are registered in globalThis.fonts.
      // Appending `, sans-serif` guarantees fallback to Switch system-ui text.
      const proto = Object.getPrototypeOf(ctx);
      const fontDesc =
        Object.getOwnPropertyDescriptor(ctx, 'font') ??
        (proto ? Object.getOwnPropertyDescriptor(proto, 'font') : undefined);
      if (fontDesc?.set && fontDesc?.get) {
        const origSet = fontDesc.set;
        const origGet = fontDesc.get;
        Object.defineProperty(ctx, 'font', {
          configurable: true,
          enumerable: true,
          get() {
            return origGet.call(ctx);
          },
          set(val: string) {
            const s = String(val ?? '10px sans-serif');
            const withFallback = /sans-serif|system-ui|serif|monospace/i.test(s)
              ? s
              : `${s}, sans-serif`;
            origSet.call(ctx, withFallback);
          },
        });
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

      get width(): number {
        return this.naturalWidth;
      }
      get height(): number {
        return this.naturalHeight;
      }

      override get src(): string {
        return super.src;
      }
      override set src(val: string) {
        super.src = val;
        this.complete = false;
        const isBlobOrData = val.startsWith('blob:') || val.startsWith('data:');
        let directUrl: string | null = isBlobOrData ? val : null;

        if (!directUrl) {
          const asset = vfs.readAssetBytes(val);
          if (!asset) {
            queueMicrotask(() => {
              if (this.onerror) this.onerror(new Error(`Image not found: ${val}`));
              this.dispatchEvent({ type: 'error' });
            });
            return;
          }
          const blob = new Blob([asset.bytes as any], { type: mimeTypeForAssetPath(asset.actualPath) });
          directUrl = URL.createObjectURL(blob);
        }

        if (typeof NativeImage === 'function' && directUrl) {
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
          img.src = directUrl;
        }
      }
    }

    // RPG Maker MV's Utils.isOptionValid() reads `location.search` and SceneManager
    // calls `location.reload()`. Provide a synthetic browser Location & History so
    // MV core scripts and plugins never throw ReferenceError: location is not defined.
    if (!g.location) {
      g.location = {
        href: 'app://nx.rpg/index.html',
        protocol: 'app:',
        host: 'nx.rpg',
        hostname: 'nx.rpg',
        port: '',
        pathname: '/index.html',
        search: '',
        hash: '',
        origin: 'app://nx.rpg',
        assign: () => {},
        replace: () => {},
        reload: () => {},
        toString() {
          return this.href;
        },
      };
    }

    class ShimVideoElement extends ShimElement {
      width = 0;
      height = 0;
      volume = 1;
      paused = true;
      currentTime = 0;
      duration = 0;
      constructor() {
        super('VIDEO');
      }
      play(): Promise<void> {
        return Promise.resolve();
      }
      pause(): void {
        this.paused = true;
      }
      load(): void {}
      canPlayType(type = ''): string {
        return /webm|mp4/i.test(String(type)) ? 'maybe' : '';
      }
    }

    class ShimAudioElement extends ShimElement {
      volume = 1;
      paused = true;
      currentTime = 0;
      duration = 0;
      constructor() {
        super('AUDIO');
      }
      play(): Promise<void> {
        return Promise.resolve();
      }
      pause(): void {
        this.paused = true;
      }
      load(): void {}
      canPlayType(type = ''): string {
        const t = String(type).toLowerCase();
        if (/ogg|vorbis|opus|mp4|aac|m4a|mpeg|mp3|wav|webm/.test(t)) {
          return 'probably';
        }
        return '';
      }
    }

    const registeredFamilies = new Set<string>(['system-ui', 'sans-serif', 'serif', 'monospace']);
    const normalizeFontFamily = (name: string) =>
      name.trim().replace(/^["']|["']$/g, '');

    const NativeFontFace = g.FontFace;
    const resolveFontSourceBytes = (source: string): ArrayBuffer | null => {
      const trimmed = String(source).trim();
      const urlMatch = /^url\(\s*["']?([^"')]+)["']?\s*\)$/i.exec(trimmed);
      const rel = urlMatch ? urlMatch[1] : trimmed;
      const asset = vfs.readAssetBytes(rel);
      if (!asset) return null;
      return asset.bytes.buffer.slice(
        asset.bytes.byteOffset,
        asset.bytes.byteOffset + asset.bytes.byteLength,
      ) as ArrayBuffer;
    };

    class ShimFontFace {
      family: string;
      status: 'unloaded' | 'loading' | 'loaded' | 'error' = 'unloaded';
      loaded: Promise<any>;
      private _native: any = null;
      private _loadError: Error | null = null;

      constructor(family: string, source: string | ArrayBuffer | ArrayBufferView, descriptors?: any) {
        this.family = normalizeFontFamily(String(family));
        let buf: ArrayBuffer | null = null;
        if (typeof source === 'string') {
          buf = resolveFontSourceBytes(source);
          if (!buf) {
            this._loadError = new Error(`Font not found in VFS: ${source}`);
          }
        } else if (source instanceof ArrayBuffer) {
          buf = source;
        } else if (ArrayBuffer.isView(source)) {
          buf = source.buffer.slice(source.byteOffset, source.byteOffset + source.byteLength) as ArrayBuffer;
        }

        if (buf && typeof NativeFontFace === 'function') {
          try {
            this._native = new NativeFontFace(this.family, buf, descriptors);
            this.status = 'loaded';
          } catch (err) {
            // e.g. unsupported WOFF container — keep shim loaded so game boots with fallback font
            self.runLog?.(`[WARN] Native FontFace(${this.family}) failed, using fallback: ${String(err)}`);
            this._native = null;
            this.status = 'loaded';
          }
        } else if (!this._loadError) {
          this.status = 'loaded';
        }
        this.loaded = this._loadError ? Promise.reject(this._loadError) : Promise.resolve(this._native ?? this);
        // Prevent unhandled rejection when caller only inspects .load()
        this.loaded.catch(() => {});
      }

      async load(): Promise<any> {
        if (this._loadError) {
          this.status = 'error';
          throw this._loadError;
        }
        this.status = 'loaded';
        return this._native ?? this;
      }

      unwrapNative(): any {
        return this._native;
      }
    }

    if (typeof NativeFontFace === 'function') {
      g.FontFace = ShimFontFace;
    }

    const baseFontSet = g.fonts;
    const fallbackFonts = new Set<any>();
    const fontSet = baseFontSet ?? {
      status: 'loaded',
      ready: null as any,
      add(face: any) {
        fallbackFonts.add(face);
        return fontSet;
      },
      has(face: any) {
        return fallbackFonts.has(face);
      },
      delete(face: any) {
        return fallbackFonts.delete(face);
      },
      clear() {
        fallbackFonts.clear();
      },
      forEach(cb: any, thisArg?: any) {
        fallbackFonts.forEach((f) => cb.call(thisArg, f, f, fontSet));
      },
      [Symbol.iterator]() {
        return fallbackFonts[Symbol.iterator]();
      },
    };
    if (!baseFontSet) {
      fontSet.ready = Promise.resolve(fontSet);
    }

    const origAddFont = typeof fontSet.add === 'function' ? fontSet.add.bind(fontSet) : null;
    fontSet.add = (face: any) => {
      if (face?.family) {
        registeredFamilies.add(normalizeFontFamily(String(face.family)));
      }
      const target = typeof face?.unwrapNative === 'function' ? face.unwrapNative() : face;
      if (target && origAddFont) {
        try {
          origAddFont(target);
        } catch {
          // ignore if native FontFaceSet rejects non-native fallback object
        }
      }
      return fontSet;
    };
    // nx.js FontFaceSet.prototype.check throws "Method not implemented."
    fontSet.check = (fontStr: string): boolean => {
      const m = /["']([^"']+)["']|([^\s,]+)\s*$/.exec(String(fontStr ?? ''));
      const fam = normalizeFontFamily(m?.[1] ?? m?.[2] ?? '');
      if (!fam) return true;
      if (registeredFamilies.has(fam)) return true;
      for (const f of fontSet) {
        if (normalizeFontFamily(String(f?.family ?? '')) === fam) return true;
      }
      return false;
    };
    if (!fontSet.ready) {
      fontSet.ready = Promise.resolve(fontSet);
    }

    const registerFontFaceCss = (cssText: string, baseDir = '') => {
      const blockRegex = /@font-face\s*\{([^}]*)\}/gi;
      let bm: RegExpExecArray | null;
      while ((bm = blockRegex.exec(cssText)) !== null) {
        const bodyCss = bm[1];
        const famMatch = /font-family\s*:\s*([^;]+)/i.exec(bodyCss);
        const urlMatch = /src\s*:[^;]*url\(\s*["']?([^"')]+)["']?\s*\)/i.exec(bodyCss);
        if (!famMatch) continue;
        const family = normalizeFontFamily(famMatch[1].split(',')[0]);
        if (!family) continue;
        if (urlMatch) {
          const rawUrl = urlMatch[1].trim();
          const joined =
            baseDir && !rawUrl.startsWith('/') && !rawUrl.includes(':')
              ? `${baseDir.replace(/\/+$/, '')}/${rawUrl}`
              : rawUrl;
          try {
            const face = new ShimFontFace(family, `url(${joined})`);
            if (face.status === 'loaded') {
              fontSet.add(face);
              continue;
            }
          } catch {
            // fall through to fallback registration
          }
        }
        registeredFamilies.add(family);
      }
    };
    (this as any)._registerFontFaceCss = registerFontFaceCss;

    class ShimStyleElement extends ShimElement {
      type = 'text/css';
      sheet = {
        cssRules: [] as string[],
        insertRule: (rule: string, index = 0) => {
          this.sheet.cssRules.splice(index, 0, rule);
          registerFontFaceCss(rule);
          return index;
        },
      };
      constructor() {
        super('STYLE');
      }
    }

    const html = new ShimElement('HTML');
    html.clientWidth = scr?.width ?? 1280;
    html.clientHeight = scr?.height ?? 720;
    const head = new ShimElement('HEAD');
    const body = new ShimElement('BODY');
    html.appendChild(head);
    html.appendChild(body);

    const doc = Object.assign(new ShimElement('DOCUMENT'), {
      body,
      head,
      documentElement: html,
      title: manifest.game.title,
      location: g.location,
      currentScript: null as ShimElement | null,
      visibilityState: 'visible',
      hidden: false,
      fonts: fontSet,
      hasFocus: () => true,
      createElement(tag: string) {
        const t = tag.toLowerCase();
        if (t === 'canvas') return new ShimCanvas();
        if (t === 'img') return new ShimImage();
        if (t === 'video') return new ShimVideoElement();
        if (t === 'audio') return new ShimAudioElement();
        if (t === 'style') return new ShimStyleElement();
        if (t === 'input' || t === 'textarea') return new ShimInputElement();
        return new ShimElement(tag);
      },
      createTextNode(text: string) {
        const node = new ShimElement('#TEXT', 3);
        node.textContent = String(text ?? '');
        return node;
      },
      createDocumentFragment() {
        return new ShimElement('#DOCUMENT-FRAGMENT', 11);
      },
    });
    doc.children.push(html);
    html.parentNode = doc;

    g.document = doc;
    g.window = g;
    g.top = g;
    g.self = g;
    const windowTarget = new ShimEventTarget();
    if (typeof g.addEventListener !== 'function') {
      g.addEventListener = windowTarget.addEventListener.bind(windowTarget);
    }
    if (typeof g.removeEventListener !== 'function') {
      g.removeEventListener = windowTarget.removeEventListener.bind(windowTarget);
    }
    if (typeof g.dispatchEvent !== 'function') {
      g.dispatchEvent = windowTarget.dispatchEvent.bind(windowTarget);
    }
    if (!g.history) {
      g.history = {
        length: 1,
        state: null,
        pushState: () => {},
        replaceState: () => {},
        back: () => {},
        forward: () => {},
        go: () => {},
      };
    }
    if (!g.navigator) {
      g.navigator = {
        userAgent: 'nx.js/Horizon',
        platform: 'Horizon arm64',
        language: 'en-US',
        getGamepads: () => [],
      };
    }
    if (!g.webkitAudioContext && typeof g.AudioContext === 'function') {
      g.webkitAudioContext = g.AudioContext;
    }
    // nx.js exposes WebGL2RenderingContext (which has all WebGL1 constants/methods)
    // without defining WebGLRenderingContext; PixiJS v4/v5 check window.WebGLRenderingContext
    // and reference WebGLRenderingContext.SCISSOR_TEST / STENCIL_TEST directly.
    if (!g.WebGLRenderingContext && g.WebGL2RenderingContext) {
      g.WebGLRenderingContext = g.WebGL2RenderingContext;
    }
    // nx.js AudioContext implements createStereoPanner() but throws "Method not implemented."
    // for createPanner(); RPG Maker MV/MZ WebAudio._createNodes() calls createPanner() + setPosition().
    for (const Ctor of [g.AudioContext, g.BaseAudioContext]) {
      const proto = Ctor?.prototype;
      if (proto && typeof proto.createStereoPanner === 'function' && !proto.__nxrpgPannerShimmed) {
        const origCreatePanner = typeof proto.createPanner === 'function' ? proto.createPanner : null;
        proto.createPanner = function () {
          if (origCreatePanner) {
            try {
              return origCreatePanner.call(this);
            } catch {
              // Fall back to StereoPannerNode when native createPanner() is unimplemented.
            }
          }
          const panner = this.createStereoPanner();
          if (typeof panner.setPosition !== 'function') {
            panner.setPosition = (x: number, _y = 0, _z = 0) => {
              const clamped = Math.max(-1, Math.min(1, Number(x) || 0));
              if (panner.pan && typeof panner.pan.value === 'number') {
                panner.pan.value = clamped;
              }
            };
          }
          if (typeof panner.setOrientation !== 'function') {
            panner.setOrientation = () => {};
          }
          panner.panningModel ??= 'equalpower';
          panner.distanceModel ??= 'inverse';
          panner.refDistance ??= 1;
          panner.maxDistance ??= 10000;
          panner.rolloffFactor ??= 1;
          panner.coneInnerAngle ??= 360;
          panner.coneOuterAngle ??= 360;
          panner.coneOuterGain ??= 0;
          return panner;
        };
        proto.__nxrpgPannerShimmed = true;
      }
    }
    // RPG Maker MZ loads js/libs/vorbisdecoder.js which unconditionally constructs `new Worker(...)`
    // even when AudioContext.decodeAudioData + Audio.canPlayType('audio/ogg; codecs="vorbis"') work natively.
    if (typeof g.Worker === 'undefined') {
      g.Worker = class ShimWorker extends ShimEventTarget {
        onmessage: ((ev?: any) => void) | null = null;
        onerror: ((ev?: any) => void) | null = null;
        constructor(_scriptUrl?: string) {
          super();
        }
        postMessage(_msg: unknown): void {}
        terminate(): void {}
      };
    }
    // RPG Maker MZ SceneManager.checkBrowser() checks Utils.canUseIndexedDB() even when
    // StorageManager.isLocalMode() (NW.js fs) handles all saves on Switch.
    if (typeof g.indexedDB === 'undefined') {
      g.indexedDB = {
        open: () => {
          const req: any = {
            result: null,
            error: null,
            onsuccess: null,
            onerror: null,
            onupgradeneeded: null,
            addEventListener: () => {},
            removeEventListener: () => {},
          };
          queueMicrotask(() => {
            req.error = new Error('IndexedDB is disabled in Switch local fs mode');
            req.onerror?.({
              target: req,
              preventDefault: () => {},
              stopPropagation: () => {},
            });
          });
          return req;
        },
        deleteDatabase: () => ({ onsuccess: null, onerror: null }),
      };
    }
    g.Image = ShimImage;
    g.HTMLCanvasElement = ShimCanvas;
    g.HTMLImageElement = ShimImage;
    g.HTMLVideoElement = ShimVideoElement;
    g.HTMLAudioElement = ShimAudioElement;
    g.Element = g.Element ?? ShimElement;
    g.HTMLElement = g.HTMLElement ?? ShimElement;
    g.Node = g.Node ?? ShimElement;
    g.innerWidth = scr?.width ?? 1280;
    g.innerHeight = scr?.height ?? 720;
    g.outerWidth = g.outerWidth ?? g.innerWidth;
    g.outerHeight = g.outerHeight ?? g.innerHeight;
    g.screenX = g.screenX ?? 0;
    g.screenY = g.screenY ?? 0;
    g.devicePixelRatio = 1;
    g.scrollTo = g.scrollTo ?? (() => {});
    g.moveBy = g.moveBy ?? (() => {});
    g.resizeBy = g.resizeBy ?? (() => {});
    g.focus = g.focus ?? (() => {});
    if (typeof g.requestAnimationFrame !== 'function') {
      g.requestAnimationFrame = (cb: (t: number) => void) => setTimeout(() => cb(Date.now()), 16);
      g.cancelAnimationFrame = (id: number) => clearTimeout(id);
    }
    if (typeof g.alert !== 'function') {
      g.alert = (msg?: unknown) => {
        self.runLog?.(`[WARN] window.alert: ${String(msg ?? '')}`);
      };
    }
  }

  async evaluateIndexHtmlScripts(indexHtmlPath: string, beforeLoad?: () => void): Promise<void> {
    const asset = this.vfs.readAssetBytes(indexHtmlPath);
    if (!asset) {
      throw new Error(`Missing ${indexHtmlPath} in contentRoot`);
    }
    const html = new TextDecoder().decode(asset.bytes);
    const g = globalThis as any;

    // Load CSS stylesheets (e.g., fonts/gamefont.css) so @font-face GameFont is ready
    const linkRegex = /<link\b[^>]*\bhref\s*=\s*["']([^"']+\.css(?:\?[^"']*)?)["'][^>]*>/gi;
    let linkMatch: RegExpExecArray | null;
    while ((linkMatch = linkRegex.exec(html)) !== null) {
      const cssRel = linkMatch[1];
      const cssAsset = this.vfs.readAssetBytes(cssRel);
      if (cssAsset && typeof (this as any)._registerFontFaceCss === 'function') {
        const clean = cssRel.split(/[?#]/)[0];
        const slash = clean.lastIndexOf('/');
        const baseDir = slash >= 0 ? clean.slice(0, slash) : '';
        (this as any)._registerFontFaceCss(new TextDecoder().decode(cssAsset.bytes), baseDir);
      }
    }

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
      if (g.document) {
        const scriptEl = g.document.createElement('script');
        scriptEl._nxrpgStaticScript = true;
        scriptEl.src = scriptRel;
        g.document.head?.appendChild(scriptEl);
        g.document.currentScript = scriptEl;
      }
      const code = new TextDecoder().decode(scriptAsset.bytes);
      try {
        const evalFn = (this as any)._evalClassicScript as ((c: string) => void) | undefined;
        if (evalFn) {
          evalFn(code);
        } else {
          (0, eval)(code);
        }
      } finally {
        if (g.document) g.document.currentScript = null;
      }
      // Dynamic scripts with async=false must run before the page load handler.
      await new Promise<void>((resolve) => queueMicrotask(resolve));
    }
    // Drain multi-wave dynamic scripts (e.g. MZ main.js -> scriptUrls -> PluginManager.setup)
    // before wrapping SceneManager and firing DOMContentLoaded / window.load.
    let guard = 0;
    while (this.pendingDynamicScripts > 0 && guard++ < 512) {
      await new Promise<void>((resolve) => queueMicrotask(resolve));
    }
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
    beforeLoad?.();
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
