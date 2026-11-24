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
