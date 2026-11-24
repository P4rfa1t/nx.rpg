/**
 * Entry point bundled as `romfs:/main.js` inside `cores/nxjs-mvz.nro`.
 *
 * Boot sequence (Phase 2 implementation target):
 *   1. Read `sdmc:/switch/nx.rpg/runtime/launch.json` BEFORE any `console.log`
 *      touches `screen` (preserving WebGL2 acquisition on `nx.js` `Screen`).
 *   2. Apply CPU boost profile via `perf/index.ts` (`@nx.js/clkrst`).
 *   3. Build case-insensitive VFS index over `manifest.game.contentRoot`.
 *   4. Install SD-backed `localStorage`, `NW.js` sandbox, DOM/WebGL1->2 shim,
 *      decryptor hooks (`.rpgmvp`/`.rpgmvo`/`.png_`/`.ogg_`), and input bridge.
 *   5. Parse `index.html` (or standard `js/libs/*` + `js/plugins.js` + `js/main.js`)
 *      and execute the RPG Maker MV / MZ boot sequence.
 *   6. On exit or fatal error, flush saves & `manifest.logPath`, invoke
 *      `Switch.setNextLoad(manifest.returnTo, ...)` and `Switch.exit()`.
 */

export const DEFAULT_MANIFEST_PATH = 'sdmc:/switch/nx.rpg/runtime/launch.json';

export interface MvzBootstrap {
  boot(manifestPath?: string): Promise<void>;
  shutdownAndReturn(fatalError?: unknown): Promise<never>;
}
