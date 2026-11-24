import type { CpuProfile } from '../types';

/** PcvModuleId.CpuBus from @nx.js/clkrst */
const PCV_MODULE_CPU_BUS = 0x40000001;

/**
 * Switch PCV CPU clock controller (via `clkrst` service) & memory pressure monitor
 * (via `Switch.memoryUsage()` + PixiJS texture cache eviction on map transitions).
 */
export interface PerformanceController {
  applyCpuProfile(profile: CpuProfile): void;
  restoreStockClocks(): void;
  installMemoryPressureGuard(maxTextureSize: number): void;
}

const PROFILE_HZ: Record<CpuProfile, number> = {
  stock_1020: 1020000000,
  boost_1581: 1581000000,
  boost_1785: 1785000000,
};

export class PerformanceControllerImpl implements PerformanceController {
  private sessionSrv: any = null;
  private monitorId: any = null;

  applyCpuProfile(profile: CpuProfile): void {
    const hz = PROFILE_HZ[profile] ?? PROFILE_HZ.stock_1020;
    if (hz === PROFILE_HZ.stock_1020) return;

    try {
      const sw = (globalThis as any).Switch;
      if (!sw || typeof sw.Service !== 'function') return;
      const clkrst = new sw.Service('clkrst');
      const sessionSrv = new sw.Service();
      const inArr = new Uint32Array([PCV_MODULE_CPU_BUS, 3]);
      clkrst.dispatchIn(0, inArr.buffer, { outObjects: [sessionSrv] });
      sessionSrv.dispatchIn(7, new Uint32Array([hz]).buffer);
      this.sessionSrv = sessionSrv;
    } catch {
      // Non-fatal if clkrst service is unavailable in emulator
    }
  }

  restoreStockClocks(): void {
    if (this.monitorId !== null) {
      clearInterval(this.monitorId);
      this.monitorId = null;
    }
    try {
      if (this.sessionSrv) {
        this.sessionSrv.dispatchIn(7, new Uint32Array([PROFILE_HZ.stock_1020]).buffer);
        this.sessionSrv.close?.();
        this.sessionSrv = null;
      }
    } catch {
      // ignore on exit
    }
  }

  installMemoryPressureGuard(maxTextureSize: number): void {
    const g = globalThis as any;
    if (maxTextureSize > 0) {
      g.__nxrpgMaxTextureSize = maxTextureSize;
    }
    const sw = g.Switch;
    if (!sw || typeof sw.memoryUsage !== 'function') return;

    this.monitorId = setInterval(() => {
      try {
        const mem = sw.memoryUsage();
        if (mem && mem.heapSizeLimit > 0 && mem.usedHeapSize / mem.heapSizeLimit > 0.85) {
          const utils = g.PIXI?.utils;
          if (utils?.clearTextureCache) {
            utils.clearTextureCache();
          }
        }
      } catch {
        // ignore
      }
    }, 5000);
  }
}
