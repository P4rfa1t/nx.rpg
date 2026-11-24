import type { CpuProfile } from '../types';

/**
 * Switch PCV CPU clock controller (via `@nx.js/clkrst`) & memory pressure monitor
 * (via `Switch.memoryUsage()` + PixiJS texture cache eviction on map transitions).
 */
export interface PerformanceController {
  applyCpuProfile(profile: CpuProfile): void;
  restoreStockClocks(): void;
  installMemoryPressureGuard(maxTextureSize: number): void;
}
