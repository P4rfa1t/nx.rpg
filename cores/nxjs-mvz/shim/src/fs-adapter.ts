export interface FsStat {
  size: number;
  mtime: number;
  mode: number;
}

export interface FsDirEntry {
  name: string;
  isFile: boolean;
  isDirectory: boolean;
}

export interface RuntimeFs {
  readFileSync(path: string): ArrayBuffer | Uint8Array | null;
  writeFileSync(path: string, data: ArrayBuffer | Uint8Array): void;
  renameSync(fromPath: string, toPath: string): void;
  removeSync(path: string): void;
  mkdirSync(path: string, mode?: number): number;
  statSync(path: string): FsStat | null;
  readDirSync(path: string): string[] | null;
}

const S_IFDIR = 0o040000;

export function isDirectoryMode(mode: number): boolean {
  return (mode & S_IFDIR) !== 0;
}

export function getDefaultRuntimeFs(): RuntimeFs {
  const sw = (globalThis as any).Switch;
  if (sw && typeof sw.readFileSync === 'function') {
    return {
      readFileSync: (p) => sw.readFileSync(p),
      writeFileSync: (p, d) => {
        const buf =
          d instanceof ArrayBuffer
            ? d
            : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
        sw.writeFileSync(p, buf);
      },
      renameSync: (a, b) => sw.renameSync(a, b),
      removeSync: (p) => sw.removeSync(p),
      mkdirSync: (p, m = 0o777) => sw.mkdirSync(p, m),
      statSync: (p) => sw.statSync(p),
      readDirSync: (p) => sw.readDirSync(p),
    };
  }
  throw new Error('No RuntimeFs available outside Switch runtime unless injected.');
}

export function dirnamePosix(p: string): string {
  const norm = p.replace(/\\/g, '/').replace(/\/+$/, '');
  const idx = norm.lastIndexOf('/');
  if (idx <= 0) return idx === 0 ? '/' : '.';
  return norm.slice(0, idx);
}

export function joinPosix(...parts: string[]): string {
  const cleaned = parts
    .filter((p) => p.length > 0)
    .map((p) => p.replace(/\\/g, '/'));
  if (cleaned.length === 0) return '';
  let out = cleaned[0].replace(/\/+$/, '');
  for (let i = 1; i < cleaned.length; i++) {
    const seg = cleaned[i].replace(/^\/+/, '').replace(/\/+$/, '');
    if (!seg) continue;
    out = out ? `${out}/${seg}` : seg;
  }
  return out;
}

/**
 * Atomic `<target>.tmp` -> write -> rename pattern for Horizon OS FAT32/exFAT safety.
 */
export function writeFileAtomicSync(
  fs: RuntimeFs,
  targetPath: string,
  data: Uint8Array | string,
): void {
  const bytes =
    typeof data === 'string' ? new TextEncoder().encode(data) : data;
  const parent = dirnamePosix(targetPath);
  if (parent && parent !== '.' && !parent.endsWith(':')) {
    fs.mkdirSync(parent, 0o777);
  }
  const tmpPath = `${targetPath}.tmp`;
  fs.writeFileSync(tmpPath, bytes);
  fs.renameSync(tmpPath, targetPath);
}
