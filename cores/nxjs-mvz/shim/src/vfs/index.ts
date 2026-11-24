/**
 * Case-insensitive SD card Virtual File System & XHR/fetch router.
 *
 * RPG Maker MV/MZ games developed on NTFS (case-insensitive) frequently
 * request `img/Pictures/Hero.png` when the actual file on SD is `img/pictures/hero.PNG`.
 * Scans `contentRoot` once at startup into a lowercase -> real-path Map.
 */
export interface CaseInsensitiveVfs {
  buildIndex(contentRoot: string): Promise<void>;
  resolveAssetPath(relativeOrUrlPath: string): string | null;
  installNetworkInterceptors(): void;
}
