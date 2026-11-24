import {
  encryptedCandidatesForPlainPath,
  mimeTypeForAssetPath,
  type RpgmAssetDecryptor,
} from '../crypto/rpgm-decryptor';
import {
  getDefaultRuntimeFs,
  isDirectoryMode,
  joinPosix,
  type RuntimeFs,
} from '../fs-adapter';

/**
 * Case-insensitive SD card Virtual File System & XHR/fetch router.
 *
 * RPG Maker MV/MZ games developed on NTFS (case-insensitive) frequently
 * request `img/Pictures/Hero.png` when the actual file on SD is `img/pictures/hero.PNG`.
 * Scans `contentRoot` once at startup into a lowercase -> real-path Map.
 */
export interface CaseInsensitiveVfs {
  buildIndex(contentRoot: string): Promise<void>;
  resolveAssetPath(relativeOrUrlPath: string): string | null;
  installNetworkInterceptors(): void;
}

export class CaseInsensitiveVfsImpl implements CaseInsensitiveVfs {
  private contentRoot = '';
  private index = new Map<string, string>();
  private readonly fs: RuntimeFs;
  private readonly decryptor: RpgmAssetDecryptor;

  constructor(decryptor: RpgmAssetDecryptor, fs?: RuntimeFs) {
    this.decryptor = decryptor;
    this.fs = fs ?? getDefaultRuntimeFs();
  }

  async buildIndex(contentRoot: string): Promise<void> {
    this.contentRoot = contentRoot.replace(/\\/g, '/').replace(/\/+$/, '');
    this.index.clear();
    this.walk(this.contentRoot, '');
  }

  private walk(absDir: string, relPrefix: string): void {
    const entries = this.fs.readDirSync(absDir);
    if (!entries) return;
    for (const name of entries) {
      if (!name || name === '.' || name === '..') continue;
      const absChild = joinPosix(absDir, name);
      const relChild = relPrefix ? `${relPrefix}/${name}` : name;
      const st = this.fs.statSync(absChild);
      if (st && isDirectoryMode(st.mode)) {
        this.walk(absChild, relChild);
      } else {
        this.index.set(relChild.toLowerCase(), absChild);
      }
    }
  }

  size(): number {
    return this.index.size;
  }

  normalizeRelativePath(relativeOrUrlPath: string): string {
    let raw = relativeOrUrlPath.trim();
    const qIdx = raw.indexOf('?');
    if (qIdx >= 0) raw = raw.slice(0, qIdx);
    const hIdx = raw.indexOf('#');
    if (hIdx >= 0) raw = raw.slice(0, hIdx);

    try {
      if (raw.includes('%')) {
        raw = decodeURIComponent(raw);
      }
    } catch {
      // Keep original string if malformed percent-encoding
    }

    raw = raw.replace(/\\/g, '/');

    if (raw.startsWith('http://') || raw.startsWith('https://')) {
      try {
        const u = new URL(raw);
        raw = u.pathname;
      } catch {
        // ignore
      }
    } else if (raw.startsWith('file://')) {
      raw = raw.slice('file://'.length);
    }

    if (this.contentRoot && raw.toLowerCase().startsWith(this.contentRoot.toLowerCase() + '/')) {
      raw = raw.slice(this.contentRoot.length + 1);
    }

    while (raw.startsWith('./')) raw = raw.slice(2);
    while (raw.startsWith('/')) raw = raw.slice(1);

    // Collapse redundant segments and reject `..` directory traversal
    const segments: string[] = [];
    for (const seg of raw.split('/')) {
      if (!seg || seg === '.') continue;
      if (seg === '..') {
        if (segments.length > 0) segments.pop();
        continue;
      }
      segments.push(seg);
    }
    return segments.join('/').toLowerCase();
  }

  resolveAssetPath(relativeOrUrlPath: string): string | null {
    const relLower = this.normalizeRelativePath(relativeOrUrlPath);
    if (!relLower) return null;

    const direct = this.index.get(relLower);
    if (direct) return direct;

    // Fallback to encrypted MV/MZ variants if plain `.png`/`.ogg`/`.m4a` was requested
    for (const encCandidate of encryptedCandidatesForPlainPath(relLower)) {
      const hit = this.index.get(encCandidate);
      if (hit) return hit;
    }

    return null;
  }

  readAssetBytes(relativeOrUrlPath: string): { bytes: Uint8Array; mime: string; actualPath: string } | null {
    const actualPath = this.resolveAssetPath(relativeOrUrlPath);
    if (!actualPath) return null;
    const raw = this.fs.readFileSync(actualPath);
    if (!raw) return null;
    let bytes: Uint8Array = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
    if (this.decryptor.isEncryptedPath(actualPath)) {
      bytes = this.decryptor.decryptBuffer(bytes);
    }
    return {
      bytes,
      mime: mimeTypeForAssetPath(actualPath),
      actualPath,
    };
  }

  installNetworkInterceptors(): void {
    const vfs = this;
    const g = globalThis as any;
    const origFetch = typeof g.fetch === 'function' ? g.fetch.bind(g) : null;

    g.fetch = async (input: RequestInfo | URL, init?: RequestInit): Promise<Response> => {
      const urlStr = typeof input === 'string' ? input : input instanceof URL ? input.href : input.url;
      if (!urlStr.startsWith('http://') && !urlStr.startsWith('https://')) {
        const asset = vfs.readAssetBytes(urlStr);
        if (asset) {
          return new Response(asset.bytes as any, {
            status: 200,
            headers: { 'content-type': asset.mime },
          });
        }
        return new Response(null, { status: 404, statusText: 'Not Found' });
      }
      if (origFetch) return origFetch(input, init);
      return new Response(null, { status: 404 });
    };

    // Provide lightweight XMLHttpRequest shim for RPG Maker MV/MZ DataManager & WebAudio
    class NxRpgXMLHttpRequest {
      status = 0;
      statusText = '';
      readyState = 0;
      responseType: '' | 'text' | 'arraybuffer' | 'json' | 'blob' = '';
      response: any = null;
      responseText = '';
      mimeOverride = '';
      onload: (() => void) | null = null;
      onerror: ((err?: any) => void) | null = null;
      onreadystatechange: (() => void) | null = null;
      private method = 'GET';
      private url = '';
      private listeners = new Map<string, Array<(ev?: any) => void>>();

      addEventListener(type: string, fn: (ev?: any) => void): void {
        const list = this.listeners.get(type) ?? [];
        list.push(fn);
        this.listeners.set(type, list);
      }
      removeEventListener(type: string, fn: (ev?: any) => void): void {
        const list = this.listeners.get(type);
        if (!list) return;
        this.listeners.set(
          type,
          list.filter((f) => f !== fn),
        );
      }
      private emit(type: string): void {
        if (type === 'load' && this.onload) this.onload();
        if (type === 'error' && this.onerror) this.onerror();
        if (type === 'readystatechange' && this.onreadystatechange) this.onreadystatechange();
        for (const fn of this.listeners.get(type) ?? []) {
          fn({ target: this });
        }
      }
      overrideMimeType(mime: string): void {
        this.mimeOverride = mime;
      }
      setRequestHeader(): void {}
      open(method: string, url: string): void {
        this.method = method;
        this.url = url;
        this.readyState = 1;
      }
      send(): void {
        const run = () => {
          const asset = vfs.readAssetBytes(this.url);
          if (!asset) {
            this.status = 404;
            this.statusText = 'Not Found';
            this.readyState = 4;
            this.emit('readystatechange');
            this.emit('error');
            return;
          }
          this.status = 200;
          this.statusText = 'OK';
          this.readyState = 4;
          if (this.responseType === 'arraybuffer') {
            this.response = asset.bytes.buffer.slice(
              asset.bytes.byteOffset,
              asset.bytes.byteOffset + asset.bytes.byteLength,
            );
          } else {
            const text = new TextDecoder().decode(asset.bytes);
            this.responseText = text;
            if (this.responseType === 'json') {
              try {
                this.response = JSON.parse(text);
              } catch {
                this.response = null;
              }
            } else {
              this.response = text;
            }
          }
          this.emit('readystatechange');
          this.emit('load');
        };
        queueMicrotask(run);
      }
    }

    g.XMLHttpRequest = NxRpgXMLHttpRequest;
  }
}
