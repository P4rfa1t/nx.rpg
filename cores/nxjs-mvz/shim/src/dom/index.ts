import type { LaunchManifest } from '../types';

/**
 * Minimal HTML5 DOM & Canvas/WebGL bridge over `nx.js` globals.
 *
 * Key `nx.js` runtime constraints handled here:
 * - `nx.js` `screen` (1280x720) allows only ONE context kind (`'2d'` OR `'webgl2'`),
 *   and does not expose `'webgl'` / `'experimental-webgl'` directly.
 * - RPG Maker MV (PixiJS v4) probes `canvas.getContext('webgl')` first:
 *   this shim maps `'webgl'` -> `screen.getContext('webgl2')` with GLSL 100
 *   compatibility shims when `renderer !== 'canvas2d'`.
 * - Offscreen `document.createElement('canvas')` delegates to `nx.js` `Canvas` / `OffscreenCanvas`
 *   (Skia 2D) for RPG Maker `Bitmap` text and window skin rendering.
 */
export interface DomShimController {
  install(manifest: LaunchManifest): void;
  evaluateIndexHtmlScripts(indexHtmlPath: string): Promise<void>;
}
