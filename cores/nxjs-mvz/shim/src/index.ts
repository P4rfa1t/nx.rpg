import { RpgmAssetDecryptorImpl } from './crypto/rpgm-decryptor';
import { DomShimControllerImpl } from './dom';
import { getDefaultRuntimeFs, writeFileAtomicSync, type RuntimeFs } from './fs-adapter';
import { SwitchInputBridgeImpl } from './input';
import { NwjsSandboxShimImpl } from './nwjs';
import { PerformanceControllerImpl } from './perf';
import { IsolatedSaveStorageImpl } from './storage';
import type { LaunchManifest } from './types';
import { CaseInsensitiveVfsImpl } from './vfs';

export const DEFAULT_MANIFEST_PATH = 'sdmc:/switch/nx.rpg/runtime/launch.json';

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

    // 1. Apply CPU clock profile before loading heavy V8 scripts
    this.perf.applyCpuProfile(manifest.runtime.cpuProfile);
    this.perf.installMemoryPressureGuard(manifest.coreOptions?.mvz?.maxTextureSize ?? 4096);

    // 2. Build case-insensitive VFS index over contentRoot
    await this.vfs.buildIndex(manifest.game.contentRoot);

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
    await this.dom.evaluateIndexHtmlScripts('index.html');
  }

  async shutdownAndReturn(fatalError?: unknown): Promise<never> {
    try {
      this.storage.flushSync();
      this.input.dispose();
      this.perf.restoreStockClocks();

      if (fatalError && this.manifest?.logPath) {
        const msg = fatalError instanceof Error ? fatalError.message : String(fatalError);
        const line = `[nx.rpg][FATAL][core=nxjs-mvz][game=${this.manifest.game.id}][code=JS_ERR] ${msg}\n`;
        writeFileAtomicSync(this.fs, this.manifest.logPath, line);
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
