import type { LaunchManifest } from '../types';

/**
 * Bridges Switch Joy-Con / Pro Controller (`navigator.getGamepads()`), Handheld
 * Touchscreen (`screen` touch events), and Horizon Software Keyboard (`Switch.keyboard` / `swkbd`)
 * into RPG Maker MV/MZ `Input` and `TouchInput` events.
 * Also polls `manifest.input.exitCombo` every frame to trigger clean return to Launcher.
 */
export interface SwitchInputBridge {
  install(manifest: LaunchManifest, onExitRequested: () => void): void;
  openSoftwareKeyboard(initialText: string, maxLength: number): Promise<string | null>;
}
