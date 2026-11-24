/**
 * Stream/buffer decryptor for RPG Maker MV & MZ encrypted assets:
 * - MV extensions: `.rpgmvp` (PNG), `.rpgmvo` (OGG), `.rpgmvm` (M4A)
 * - MZ extensions: `.png_`, `.ogg_`, `.m4a_`
 *
 * Format specification:
 * - 16-byte fake header (`52 50 47 4D 56 00 00 00 00 03 01 00 00 00 00 00` = "RPGMV...")
 * - Followed by payload whose first 16 bytes are XORed with the 16-byte hex key
 *   decoded from `data/System.json -> encryptionKey`.
 */
export interface RpgmAssetDecryptor {
  configureFromSystemJson(encryptionKeyHex?: string): void;
  isEncryptedPath(path: string): boolean;
  decryptBuffer(raw: Uint8Array): Uint8Array;
}

const ENCRYPTED_EXT_MIME: Record<string, string> = {
  '.rpgmvp': 'image/png',
  '.rpgmvo': 'audio/ogg',
  '.rpgmvm': 'audio/mp4',
  '.png_': 'image/png',
  '.ogg_': 'audio/ogg',
  '.m4a_': 'audio/mp4',
};

const PLAIN_TO_ENCRYPTED_EXTS: Record<string, string[]> = {
  '.png': ['.rpgmvp', '.png_'],
  '.ogg': ['.rpgmvo', '.ogg_'],
  '.m4a': ['.rpgmvm', '.m4a_'],
};

const HEADER_LEN = 16;
const RPGMV_MAGIC = [0x52, 0x50, 0x47, 0x4d, 0x56]; // "RPGMV"

export function parseEncryptionKeyHex(hex?: string): Uint8Array {
  if (!hex) return new Uint8Array(0);
  const clean = hex.trim().replace(/[^0-9a-fA-F]/g, '');
  if (clean.length < 32) return new Uint8Array(0);
  const key = new Uint8Array(16);
  for (let i = 0; i < 16; i++) {
    key[i] = parseInt(clean.slice(i * 2, i * 2 + 2), 16);
  }
  return key;
}

export function encryptedCandidatesForPlainPath(relLowerPath: string): string[] {
  const dot = relLowerPath.lastIndexOf('.');
  if (dot < 0) return [];
  const ext = relLowerPath.slice(dot);
  const candidates = PLAIN_TO_ENCRYPTED_EXTS[ext];
  if (!candidates) return [];
  const stem = relLowerPath.slice(0, dot);
  return candidates.map((ce) => stem + ce);
}

export function mimeTypeForAssetPath(path: string): string {
  const lower = path.split('?')[0].split('#')[0].toLowerCase();
  const dot = lower.lastIndexOf('.');
  if (dot >= 0) {
    const ext = lower.slice(dot);
    if (ENCRYPTED_EXT_MIME[ext]) return ENCRYPTED_EXT_MIME[ext];
    if (ext === '.png') return 'image/png';
    if (ext === '.jpg' || ext === '.jpeg') return 'image/jpeg';
    if (ext === '.ogg') return 'audio/ogg';
    if (ext === '.m4a') return 'audio/mp4';
    if (ext === '.wav') return 'audio/wav';
    if (ext === '.json') return 'application/json';
    if (ext === '.js') return 'application/javascript';
    if (ext === '.wasm') return 'application/wasm';
  }
  return 'application/octet-stream';
}

export class RpgmAssetDecryptorImpl implements RpgmAssetDecryptor {
  private key: Uint8Array = new Uint8Array(0);

  configureFromSystemJson(encryptionKeyHex?: string): void {
    this.key = parseEncryptionKeyHex(encryptionKeyHex);
  }

  hasKey(): boolean {
    return this.key.length === 16;
  }

  isEncryptedPath(path: string): boolean {
    const clean = path.split('?')[0].split('#')[0].toLowerCase();
    const dot = clean.lastIndexOf('.');
    if (dot < 0) return false;
    return clean.slice(dot) in ENCRYPTED_EXT_MIME;
  }

  decryptBuffer(raw: Uint8Array): Uint8Array {
    if (raw.byteLength <= HEADER_LEN) {
      return raw;
    }

    // Verify RPGMV 5-byte prefix when present; MZ `.png_` uses the exact same 16-byte fake header
    let hasFakeHeader = true;
    for (let i = 0; i < RPGMV_MAGIC.length; i++) {
      if (raw[i] !== RPGMV_MAGIC[i]) {
        hasFakeHeader = false;
        break;
      }
    }
    if (!hasFakeHeader) {
      return raw;
    }

    const payload = raw.slice(HEADER_LEN);
    if (this.key.length === 16) {
      const n = Math.min(16, payload.byteLength);
      for (let i = 0; i < n; i++) {
        payload[i] = payload[i] ^ this.key[i];
      }
    }
    return payload;
  }
}
