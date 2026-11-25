import { RpgmAssetDecryptorImpl } from './crypto/rpgm-decryptor';
import { DomShimControllerImpl } from './dom';
import { getDefaultRuntimeFs, type RuntimeFs } from './fs-adapter';
import { SwitchInputBridgeImpl } from './input';
import { NwjsSandboxShimImpl } from './nwjs';
import { PerformanceControllerImpl } from './perf';
import { IsolatedSaveStorageImpl } from './storage';
import type { LaunchManifest } from './types';
import { CaseInsensitiveVfsImpl } from './vfs';

export const DEFAULT_MANIFEST_PATH = 'sdmc:/switch/nx.rpg/runtime/launch.json';

/** Append-only run log shared with the Launcher Diagnostics tab. */
function appendRunLog(fs: RuntimeFs, logPath: string, line: string): void {
  try {
    const stamp = new Date().toISOString();
    const parent = logPath.replace(/\\/g, '/').replace(/\/+$/, '').replace(/\/[^/]*$/, '');
    if (parent && !parent.endsWith(':')) fs.mkdirSync(parent, 0o777);
    const bytes = new TextEncoder().encode(`[${stamp}] ${line}\n`);
    const prev = fs.readFileSync(logPath);
    const merged = new Uint8Array((prev?.byteLength ?? 0) + bytes.byteLength);
    if (prev) merged.set(prev instanceof Uint8Array ? prev : new Uint8Array(prev), 0);
    merged.set(bytes, prev?.byteLength ?? 0);
    fs.writeFileSync(logPath, merged);
  } catch {
    // Logging must never break game launch/exit
  }
}

export interface MvzBootstrap {
  boot(manifestPath?: string): Promise<void>;
  shutdownAndReturn(fatalError?: unknown): Promise<never>;
}

export class MvzBootstrapImpl implements MvzBootstrap {
  private readonly fs: RuntimeFs;
  private manifest: LaunchManifest | null = null;
  private readonly perf = new PerformanceControllerImpl();
  private readonly storage: IsolatedSaveStorageImpl;
  private readonly input = new SwitchInputBridgeImpl();
  private readonly decryptor = new RpgmAssetDecryptorImpl();
  private readonly vfs: CaseInsensitiveVfsImpl;
  private readonly nwjs: NwjsSandboxShimImpl;
  private readonly dom: DomShimControllerImpl;

  constructor(fs?: RuntimeFs) {
    this.fs = fs ?? getDefaultRuntimeFs();
    this.storage = new IsolatedSaveStorageImpl(this.fs);
    this.vfs = new CaseInsensitiveVfsImpl(this.decryptor, this.fs);
    this.nwjs = new NwjsSandboxShimImpl(this.fs);
    this.dom = new DomShimControllerImpl(this.vfs, (init, maxLen) =>
      this.input.openSoftwareKeyboard(init, maxLen),
    );
  }

  async boot(manifestPath: string = DEFAULT_MANIFEST_PATH): Promise<void> {
    const raw = this.fs.readFileSync(manifestPath);
    if (!raw) {
      throw new Error(`Launch manifest not found at ${manifestPath}`);
    }
    const bytes = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
    const manifest = JSON.parse(new TextDecoder().decode(bytes)) as LaunchManifest;
    this.manifest = manifest;

    // 0. Session banner in the shared run log (Diagnostics tab shows the tail)
    if (manifest.logPath) {
      appendRunLog(this.fs, manifest.logPath, `[INFO] nxjs-mvz session start game=${manifest.game.id} engine=${manifest.game.engine}`);
    }

    // 1. Apply CPU clock profile before loading heavy V8 scripts
    this.perf.applyCpuProfile(manifest.runtime.cpuProfile);
    this.perf.installMemoryPressureGuard(manifest.coreOptions?.mvz?.maxTextureSize ?? 4096);

    // 2. Build case-insensitive VFS index over contentRoot
    await this.vfs.buildIndex(manifest.game.contentRoot);
    if (manifest.logPath) {
      appendRunLog(this.fs, manifest.logPath, `[INFO] cpuProfile=${manifest.runtime.cpuProfile} vfs index built`);
    }

    // 3. Configure asset decryptor from data/System.json before installing XHR/fetch hooks
    const sysAsset = this.vfs.readAssetBytes('data/System.json');
    if (sysAsset) {
      try {
        const sysJson = JSON.parse(new TextDecoder().decode(sysAsset.bytes)) as {
          encryptionKey?: string;
        };
        this.decryptor.configureFromSystemJson(sysJson.encryptionKey);
      } catch {
        // ignore invalid System.json during key probe
      }
    }

    // 4. Install isolated storage, NW.js sandbox, VFS network hooks, DOM/WebGL2 bridge, and input
    await this.storage.install(manifest);
    this.nwjs.install(manifest);
    this.vfs.installNetworkInterceptors();
    this.dom.install(manifest);
    this.input.install(manifest, () => {
      void this.shutdownAndReturn();
    });

    // 5. Evaluate RPG Maker MV / MZ index.html scripts
    if (manifest.logPath) {
      appendRunLog(this.fs, manifest.logPath, '[INFO] evaluating game scripts (index.html)');
    }
    await this.dom.evaluateIndexHtmlScripts('index.html');
  }

  async shutdownAndReturn(fatalError?: unknown): Promise<never> {
    try {
      this.storage.flushSync();
      this.input.dispose();
      this.perf.restoreStockClocks();

      if (this.manifest?.logPath) {
        if (fatalError) {
          const msg = fatalError instanceof Error ? fatalError.message : String(fatalError);
          appendRunLog(this.fs, this.manifest.logPath, `[FATAL][core=nxjs-mvz][game=${this.manifest.game.id}][code=JS_ERR] ${msg}`);
        } else {
          appendRunLog(this.fs, this.manifest.logPath, `[INFO] clean exit game=${this.manifest.game.id}`);
        }
      }
    } finally {
      const sw = (globalThis as any).Switch;
      if (sw && this.manifest?.returnTo && typeof sw.setNextLoad === 'function') {
        try {
          sw.setNextLoad(this.manifest.returnTo, `${this.manifest.returnTo} --from-core`);
        } catch {
          // ignore if envHasNextLoad() is false
        }
      }
      if (sw && typeof sw.exit === 'function') {
        sw.exit();
      }
    }
    throw fatalError ?? new Error('nxjs-mvz exited');
  }
}

// Auto-boot when running inside nx.js (`Switch` global present and not under Node test runner)
if (typeof (globalThis as any).Switch !== 'undefined' && typeof process === 'undefined') {
  const bootstrap = new MvzBootstrapImpl();
  bootstrap.boot().catch((err) => bootstrap.shutdownAndReturn(err));
}
