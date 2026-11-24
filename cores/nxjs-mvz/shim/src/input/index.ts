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

const BUTTON_NAME_TO_GAMEPAD_INDEX: Record<string, number> = {
  A: 0,
  B: 1,
  X: 2,
  Y: 3,
  LSTICK: 4,
  RSTICK: 5,
  L: 6,
  R: 7,
  ZL: 8,
  ZR: 9,
  PLUS: 10,
  MINUS: 11,
};

export function isExitComboPressed(
  pressedButtons: ReadonlyArray<{ pressed?: boolean } | undefined>,
  exitCombo: readonly string[],
): boolean {
  if (exitCombo.length < 2) return false;
  for (const name of exitCombo) {
    const idx = BUTTON_NAME_TO_GAMEPAD_INDEX[name];
    if (idx === undefined || !pressedButtons[idx]?.pressed) {
      return false;
    }
  }
  return true;
}

export class SwitchInputBridgeImpl implements SwitchInputBridge {
  private timerId: any = null;
  private exitTriggered = false;

  install(manifest: LaunchManifest, onExitRequested: () => void): void {
    const g = globalThis as any;
    const swapAB = manifest.input.swapConfirmCancel;
    const exitCombo = manifest.input.exitCombo?.length >= 2 ? manifest.input.exitCombo : ['L', 'R', 'PLUS'];

    const prevKeys = new Set<string>();
    const dispatchKey = (type: 'keydown' | 'keyup', key: string, keyCode: number) => {
      if (typeof g.document?.dispatchEvent === 'function') {
        g.document.dispatchEvent({ type, key, keyCode, which: keyCode, preventDefault: () => {} });
      }
      if (typeof g.dispatchEvent === 'function') {
        g.dispatchEvent({ type, key, keyCode, which: keyCode, preventDefault: () => {} });
      }
    };

    const syncKey = (active: boolean, key: string, keyCode: number) => {
      const wasActive = prevKeys.has(key);
      if (active && !wasActive) {
        prevKeys.add(key);
        dispatchKey('keydown', key, keyCode);
      } else if (!active && wasActive) {
        prevKeys.delete(key);
        dispatchKey('keyup', key, keyCode);
      }
    };

    // Two-finger touch tap -> RPG Maker cancel (Escape / right-click)
    const scr = g.screen;
    if (manifest.runtime.enableTouch && scr && typeof scr.addEventListener === 'function') {
      let twoFingerStart = 0;
      scr.addEventListener('touchstart', (ev: any) => {
        if (ev?.touches?.length === 2) {
          twoFingerStart = Date.now();
        }
      });
      scr.addEventListener('touchend', (ev: any) => {
        if (twoFingerStart > 0 && Date.now() - twoFingerStart < 450 && (ev?.touches?.length ?? 0) === 0) {
          twoFingerStart = 0;
          dispatchKey('keydown', 'Escape', 27);
          setTimeout(() => dispatchKey('keyup', 'Escape', 27), 50);
        }
      });
    }

    this.timerId = setInterval(() => {
      if (this.exitTriggered) return;
      const pads = typeof g.navigator?.getGamepads === 'function' ? g.navigator.getGamepads() : [];
      const pad = pads?.[0];
      if (!pad) return;

      if (isExitComboPressed(pad.buttons, exitCombo)) {
        this.exitTriggered = true;
        this.dispose();
        onExitRequested();
        return;
      }

      const btn = (idx: number) => Boolean(pad.buttons[idx]?.pressed);
      const axX = pad.axes?.[0] ?? 0;
      const axY = pad.axes?.[1] ?? 0;

      syncKey(btn(13) || axX < -0.45, 'ArrowLeft', 37);
      syncKey(btn(12) || axY < -0.45, 'ArrowUp', 38);
      syncKey(btn(14) || axX > 0.45, 'ArrowRight', 39);
      syncKey(btn(15) || axY > 0.45, 'ArrowDown', 40);

      const confirm = swapAB ? btn(1) : btn(0);
      const cancel = swapAB ? btn(0) : btn(1);
      syncKey(confirm, 'Enter', 13);
      syncKey(cancel, 'Escape', 27);
      syncKey(btn(3), 'Shift', 16); // Y -> Shift (dash)
      syncKey(btn(6), 'PageUp', 33); // L
      syncKey(btn(7), 'PageDown', 34); // R
      syncKey(btn(8), 'Control', 17); // ZL -> Ctrl (skip text)

      // In RPG Maker MV/MZ Scene_Name, pressing X (button 2) triggers native Switch swkbd
      if (btn(2) && !prevKeys.has('__SWKBD_X__') && manifest.runtime.enableSoftwareKeyboard) {
        prevKeys.add('__SWKBD_X__');
        const scene = g.SceneManager?._scene;
        const editWin = scene?._editWindow;
        if (editWin && typeof editWin.name === 'function' && typeof editWin.maxLength === 'function') {
          const initial = String(editWin.name() ?? '');
          const maxLen = Number(editWin.maxLength() ?? 12);
          void this.openSoftwareKeyboard(initial, maxLen).then((entered) => {
            if (entered !== null && entered.length > 0 && typeof editWin._name !== 'undefined') {
              editWin._name = entered.slice(0, maxLen);
              editWin._index = editWin._name.length;
              editWin.refresh?.();
            }
          });
        }
      } else if (!btn(2)) {
        prevKeys.delete('__SWKBD_X__');
      }
    }, 16);
  }

  dispose(): void {
    if (this.timerId !== null) {
      clearInterval(this.timerId);
      this.timerId = null;
    }
  }

  async openSoftwareKeyboard(initialText: string, maxLength: number): Promise<string | null> {
    const g = globalThis as any;
    const kbd = g.navigator?.virtualKeyboard;
    if (!kbd || typeof kbd.show !== 'function') {
      return initialText;
    }
    const clampedMax = Math.max(1, maxLength);
    return new Promise((resolve) => {
      let settled = false;
      let shown = false;
      const cleanup = () => {
        kbd.removeEventListener?.('submit', onSubmit);
        kbd.removeEventListener?.('geometrychange', onGeometryChange);
      };
      const finish = (val: string | null) => {
        if (settled) return;
        settled = true;
        cleanup();
        resolve(val === null ? null : val.slice(0, clampedMax));
      };
      const onSubmit = () => {
        finish(String(kbd.value ?? ''));
      };
      const onGeometryChange = () => {
        const w = kbd.boundingRect?.width ?? 0;
        if (w > 0) {
          shown = true;
        } else if (shown) {
          finish(null);
        }
      };
      try {
        kbd.maxLength = clampedMax;
        kbd.value = initialText.slice(0, clampedMax);
        kbd.addEventListener?.('submit', onSubmit);
        kbd.addEventListener?.('geometrychange', onGeometryChange);
        kbd.show();
      } catch {
        finish(initialText);
      }
    });
  }
}
