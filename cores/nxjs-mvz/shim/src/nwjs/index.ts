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
