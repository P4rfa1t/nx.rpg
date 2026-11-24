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
