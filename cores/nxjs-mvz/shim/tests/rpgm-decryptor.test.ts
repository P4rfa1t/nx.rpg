import { describe, expect, it } from 'vitest';
import {
  RpgmAssetDecryptorImpl,
  mimeTypeForAssetPath,
  parseEncryptionKeyHex,
} from '../src/crypto/rpgm-decryptor';

describe('RpgmAssetDecryptorImpl', () => {
  const keyHex = '00112233445566778899aabbccddeeff';
  const fakeHeader = new Uint8Array([
    0x52, 0x50, 0x47, 0x4d, 0x56, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  ]);
  const pngHeaderAndBody = new Uint8Array([
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0xaa, 0xbb, 0xcc, 0xdd,
  ]);

  function makeEncrypted(plain: Uint8Array, hex: string): Uint8Array {
    const key = parseEncryptionKeyHex(hex);
    const out = new Uint8Array(fakeHeader.length + plain.length);
    out.set(fakeHeader, 0);
    out.set(plain, fakeHeader.length);
    for (let i = 0; i < Math.min(16, plain.length); i++) {
      out[fakeHeader.length + i] ^= key[i];
    }
    return out;
  }

  it('recognizes MV and MZ encrypted asset extensions', () => {
    const dec = new RpgmAssetDecryptorImpl();
    expect(dec.isEncryptedPath('img/actors/Actor1.rpgmvp')).toBe(true);
    expect(dec.isEncryptedPath('audio/bgm/Theme.rpgmvo?t=123')).toBe(true);
    expect(dec.isEncryptedPath('img/titles1/Title.png_')).toBe(true);
    expect(dec.isEncryptedPath('audio/se/Cursor.OGG_')).toBe(true);
    expect(dec.isEncryptedPath('img/titles1/Title.png')).toBe(false);
    expect(mimeTypeForAssetPath('img/titles1/Title.png_')).toBe('image/png');
    expect(mimeTypeForAssetPath('audio/bgm/Theme.rpgmvo')).toBe('audio/ogg');
  });

  it('strips 16-byte RPGMV header and XOR-decrypts first 16 payload bytes', () => {
    const dec = new RpgmAssetDecryptorImpl();
    dec.configureFromSystemJson(keyHex);

    const encrypted = makeEncrypted(pngHeaderAndBody, keyHex);
    const decrypted = dec.decryptBuffer(encrypted);

    expect(Array.from(decrypted)).toEqual(Array.from(pngHeaderAndBody));
  });

  it('passes through buffers without RPGMV fake header untouched', () => {
    const dec = new RpgmAssetDecryptorImpl();
    dec.configureFromSystemJson(keyHex);

    const plain = new Uint8Array(pngHeaderAndBody);
    expect(Array.from(dec.decryptBuffer(plain))).toEqual(Array.from(pngHeaderAndBody));
  });
});
