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

// nx.js exposes standard Web Gamepad indices, independent of Switch HID bit order.
const BUTTON_NAME_TO_GAMEPAD_INDEX: Readonly<Record<string, number>> = {
  B: 0,
  A: 1,
  Y: 2,
  X: 3,
  L: 4,
  R: 5,
  ZL: 6,
  ZR: 7,
  MINUS: 8,
  PLUS: 9,
  LSTICK: 10,
  RSTICK: 11,
  UP: 12,
  DOWN: 13,
  LEFT: 14,
  RIGHT: 15,
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
  private frameId: number | null = null;
  private readonly preventRuntimeExit = (event: Event) => event.preventDefault();

  install(manifest: LaunchManifest, onExitRequested: () => void): void {
    const g = globalThis as any;
    // nx.js dispatches beforeunload on + before running this frame's callbacks.
    g.addEventListener?.('beforeunload', this.preventRuntimeExit);
    const swapAB = manifest.input.swapConfirmCancel;
    const confirmButton = swapAB ? 'B' : 'A';
    const cancelButton = swapAB ? 'A' : 'B';
    const configuredCombo = [...new Set(manifest.input.exitCombo)];
    const exitCombo = configuredCombo.length >= 2 ? configuredCombo : ['L', 'R', 'PLUS'];

    // The bridge emits keyboard events for gameplay. Polling the same raw pad
    // inside RPG Maker would apply a second, sometimes contradictory mapping.
    if (typeof g.Input?._pollGamepads === 'function') {
      g.Input._pollGamepads = () => {};
    }

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

    const pollFrame = () => {
      if (this.frameId === null) return;
      // install() runs before game startup. Keep input ahead of game updates
      // by queuing the next poll before dispatching this frame's key events.
      this.frameId = g.requestAnimationFrame(pollFrame);
      const pads = typeof g.navigator?.getGamepads === 'function' ? g.navigator.getGamepads() : [];
      const pad = pads?.find((candidate: Gamepad | null) => candidate?.connected);

      if (pad && isExitComboPressed(pad.buttons, exitCombo)) {
        this.dispose();
        onExitRequested();
        return;
      }

      const btn = (name: string) => Boolean(pad?.buttons[BUTTON_NAME_TO_GAMEPAD_INDEX[name]]?.pressed);
      const axX = pad?.axes?.[0] ?? 0;
      const axY = pad?.axes?.[1] ?? 0;

      syncKey(btn('LEFT') || axX < -0.45, 'ArrowLeft', 37);
      syncKey(btn('UP') || axY < -0.45, 'ArrowUp', 38);
      syncKey(btn('RIGHT') || axX > 0.45, 'ArrowRight', 39);
      syncKey(btn('DOWN') || axY > 0.45, 'ArrowDown', 40);

      const editWin = g.SceneManager?._scene?._editWindow;
      const useSwkbd = manifest.runtime.enableSoftwareKeyboard &&
        typeof editWin?.name === 'function' && typeof editWin.maxLength === 'function';
      syncKey(btn(confirmButton), 'Enter', 13);
      syncKey(btn(cancelButton) || (btn('X') && !useSwkbd), 'Escape', 27);
      syncKey(btn('Y'), 'Shift', 16);
      syncKey(btn('L'), 'PageUp', 33);
      syncKey(btn('R'), 'PageDown', 34);
      syncKey(btn('ZL'), 'Control', 17);

      if (btn('X') && !prevKeys.has('__SWKBD_X__')) {
        prevKeys.add('__SWKBD_X__');
        if (useSwkbd) {
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
      } else if (!btn('X')) {
        prevKeys.delete('__SWKBD_X__');
      }
    };
    this.frameId = g.requestAnimationFrame(pollFrame);
  }

  dispose(): void {
    if (this.frameId !== null) {
      cancelAnimationFrame(this.frameId);
      this.frameId = null;
    }
    globalThis.removeEventListener?.('beforeunload', this.preventRuntimeExit);
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
