"""POSIX host tests for the Switch FTP/MTP transfer backends (no USB hardware)."""

import ftplib
import hashlib
import io
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import time
import unittest

TRANSFER = Path(__file__).resolve().parents[1] / 'launcher/src/transfer'
HOST = r'''
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ftpd.h"
#include "mtp_core.h"

struct mtp_ctx {
    const char *root;
    FILE *log;
    uint64_t reported;
    uint64_t bytes;
    uint64_t sent;
    unsigned files;
};

static double mono_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static const char *rel_path(const char *root, const char *path) {
    size_t n = strlen(root);
    if (n && strncmp(path, root, n) == 0 && path[n] == '/')
        return path + n;
    return path;
}

static void on_mtp_progress(void *user, const char *file, uint64_t bytes, uint64_t total,
                            bool finished, bool ok, bool upload) {
    struct mtp_ctx *ctx = user;
    if (!finished && bytes == 0) ctx->reported = 0;
    if (bytes > ctx->reported) {
        uint64_t delta = bytes - ctx->reported;
        ctx->reported = bytes;
        if (upload) ctx->bytes += delta;
        else ctx->sent += delta;
    }
    if (finished) {
        ctx->reported = 0;
        if (upload && ok) ++ctx->files;
    }
    if (ctx->log) {
        fprintf(ctx->log, "%.6f|%s|%llu|%llu|%llu|%llu|%u|%d|%d|%d\n",
                mono_sec(), rel_path(ctx->root, file),
                (unsigned long long)bytes, (unsigned long long)total,
                (unsigned long long)ctx->bytes, (unsigned long long)ctx->sent,
                ctx->files, finished ? 1 : 0, ok ? 1 : 0, upload ? 1 : 0);
        fflush(ctx->log);
    }
}

static bool receive(void *ctx, void *data, size_t n) {
    (void)ctx;
    return fread(data, 1, n, stdin) == n;
}
static bool send_data(void *ctx, const void *data, size_t n) {
    (void)ctx;
    return fwrite(data, 1, n, stdout) == n && fflush(stdout) == 0;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) return 2;
    signal(SIGPIPE, SIG_IGN);
    if (!strcmp(argv[1], "mtp")) {
        struct mtp_ctx ctx = { .root = argv[2], .log = argc == 4 ? fopen(argv[3], "w") : NULL };
        MtpCore *core = mtp_core_new(argv[2], receive, send_data, on_mtp_progress, &ctx);
        if (!core) return 1;
        while (mtp_core_step(core)) {}
        mtp_core_free(core);
        if (ctx.log) fclose(ctx.log);
    } else {
        char error[256];
        if (!rsl_ftpd_start(argv[2], 0, htonl(INADDR_LOOPBACK), error, sizeof error)) {
            fputs(error, stderr);
            return 1;
        }
        rsl_ftpd_status state;
        rsl_ftpd_get(&state);
        printf("%d\n", state.port);
        fflush(stdout);
        char cmd[32];
        while (fgets(cmd, sizeof cmd, stdin)) {
            if (cmd[0] == 'Q') break;
            if (cmd[0] == 'S') {
                rsl_ftpd_get(&state);
                printf("%d|%d|%u|%llu|%llu|%s|%s\n",
                       state.running ? 1 : 0, state.connected ? 1 : 0, state.files,
                       (unsigned long long)state.bytes, (unsigned long long)state.sent,
                       state.file, state.error);
                fflush(stdout);
            }
        }
        rsl_ftpd_stop();
    }
    return 0;
}
'''


def _mtp_packet(kind, op, tx, data=b''):
    return struct.pack('<IHHI', len(data) + 12, kind, op, tx) + data


def _mtp_cmd(op, tx, *args):
    return _mtp_packet(1, op, tx, struct.pack('<' + 'I' * len(args), *args))


def _mtp_name(value):
    return bytes([len(value) + 1]) + (value + '\0').encode('utf-16le')


def _mtp_upload_meta(filename, size, parent=0xFFFFFFFF):
    return (struct.pack('<IHHI', 0x10001, 0x3000, 0, size) + bytes(40)
            + _mtp_name(filename) + bytes(3))


def _read_mtp_container(stream):
    hdr = stream.read(12)
    if len(hdr) < 12:
        return None
    length, kind, code, tx = struct.unpack('<IHHI', hdr)
    body = stream.read(length - 12)
    assert len(body) == length - 12
    return kind, code, tx, body


class _ThrottledReader:
    """File-like object that yields chunks at a target bytes/sec rate."""
    def __init__(self, payload: bytes, rate_bps: float):
        self._buf = memoryview(payload)
        self._pos = 0
        self._rate = rate_bps
        self._t0 = time.monotonic()

    def read(self, size: int = 65536) -> bytes:
        if self._pos >= len(self._buf):
            return b''
        chunk = bytes(self._buf[self._pos:self._pos + size])
        self._pos += len(chunk)
        target = self._t0 + self._pos / self._rate
        delay = target - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        return chunk


@unittest.skipUnless(os.name == 'posix', 'Requires a POSIX C compiler')
class TransferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        work = Path(cls.temp.name)
        source = work / 'host.c'
        source.write_text(HOST)
        cls.executable = work / 'transfer-host'
        subprocess.run([os.environ.get('CC', 'cc'), '-O2', '-Wall', '-Wextra',
                        '-std=gnu11', '-pthread', '-I', str(TRANSFER),
                        str(source), str(TRANSFER / 'ftpd.c'),
                        str(TRANSFER / 'mtp_core.c'), '-o', str(cls.executable)],
                       check=True, timeout=30)

    def _poll_ftp(self, proc):
        proc.stdin.write('S\n')
        proc.stdin.flush()
        line = proc.stdout.readline().rstrip('\n')
        running, connected, files, rx, tx, filename, err = line.split('|', 6)
        return {
            'running': bool(int(running)),
            'connected': bool(int(connected)),
            'files': int(files),
            'bytes': int(rx),
            'sent': int(tx),
            'file': filename,
            'error': err,
        }

    def test_ftp_credentials_scope_and_upload(self):
        with tempfile.TemporaryDirectory() as tmp:
            parent = Path(tmp)
            root = parent / 'nx.rpg'
            (root / 'games').mkdir(parents=True)
            (root / 'saves').mkdir()
            (root / 'cores').mkdir()
            (root / 'saves/data').write_bytes(b'local save')
            (parent / 'other').mkdir()
            (parent / 'other/data').write_bytes(b'untouched')
            (root / 'games/link').symlink_to(parent / 'other', target_is_directory=True)
            with subprocess.Popen([self.executable, 'ftp', root], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True) as proc:
                try:
                    port = int(proc.stdout.readline())
                    with ftplib.FTP() as ftp:
                        ftp.connect('127.0.0.1', port, timeout=5)
                        with self.assertRaises(ftplib.error_perm):
                            ftp.sendcmd('AUTH TLS')
                        ftp.login()
                        self.assertCountEqual(ftp.nlst(), ['games', 'saves', 'cores'])
                        saved = bytearray()
                        ftp.retrbinary('RETR /saves/data', saved.extend)
                        self.assertEqual(saved, b'local save')
                        ftp.storbinary('STOR /cores/new.nro', io.BytesIO(b'new core'))
                        for folder in ('/other', '/../other', '/games/../../other', '/games/link'):
                            with self.assertRaises(ftplib.error_perm):
                                ftp.cwd(folder)
                        with self.assertRaises(ftplib.error_perm):
                            ftp.sendcmd('RNFR /')
                        ftp.cwd('..')
                        self.assertEqual(ftp.pwd(), '/')
                        ftp.cwd('/games')
                        ftp.mkd('Example')
                        ftp.storbinary('STOR Example/Game.ini', io.BytesIO(b'fixture'))
                        with self.assertRaises(ftplib.error_perm):
                            ftp.storbinary('STOR Example/Game.ini', io.BytesIO(b'overwrite'))
                        data = bytearray()
                        ftp.retrbinary('RETR Example/Game.ini', data.extend)
                        self.assertEqual(data, b'fixture')
                        ftp.rename('Example', 'Ready')
                        self.assertEqual(ftp.nlst(), ['Ready'])
                    with ftplib.FTP() as again:
                        again.connect('127.0.0.1', port, timeout=5)
                        again.login('nxrpg', 'any-password-works')
                    self.assertEqual((root / 'games/Ready/Game.ini').read_bytes(), b'fixture')
                    self.assertEqual((root / 'cores/new.nro').read_bytes(), b'new core')
                    self.assertEqual((root / 'saves/data').read_bytes(), b'local save')
                    self.assertEqual((parent / 'other/data').read_bytes(), b'untouched')
                finally:
                    proc.stdin.write('Q\n')
                    proc.stdin.flush()
                    proc.stdin.close()
                    proc.wait(timeout=5)
                self.assertEqual(proc.returncode, 0)

    def test_ftp_live_filename_rate_and_throughput(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'nx.rpg'
            (root / 'games').mkdir(parents=True)
            # 6 MB payload throttled at ~4 MB/s (~1.5s) -> >=2 live 500ms UI ticks
            stream_payload = os.urandom(6 * 1024 * 1024)
            stream_sha = hashlib.sha256(stream_payload).hexdigest()

            with subprocess.Popen([self.executable, 'ftp', root], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True) as proc:
                try:
                    port = int(proc.stdout.readline())
                    with ftplib.FTP() as ftp:
                        ftp.connect('127.0.0.1', port, timeout=15)
                        ftp.login()

                        # 1. Live upload polling (simulating 500ms TransferTab refresh)
                        up_done = threading.Event()
                        up_err = []

                        def do_upload():
                            try:
                                reader = _ThrottledReader(stream_payload, 4 * 1024 * 1024)
                                ftp.storbinary('STOR /games/stream_up.bin', reader, blocksize=65536)
                            except Exception as exc:
                                up_err.append(exc)
                            finally:
                                up_done.set()

                        t = threading.Thread(target=do_upload)
                        t.start()
                        up_samples = []
                        prev_bytes = 0
                        prev_t = time.monotonic()
                        while not up_done.is_set():
                            time.sleep(0.45)
                            st = self._poll_ftp(proc)
                            now = time.monotonic()
                            rate = (st['bytes'] - prev_bytes) / (now - prev_t)
                            prev_bytes, prev_t = st['bytes'], now
                            up_samples.append((st, rate))
                        t.join()
                        self.assertFalse(up_err, up_err)

                        st_after_up = self._poll_ftp(proc)
                        self.assertEqual(st_after_up['files'], 1)
                        self.assertEqual(st_after_up['bytes'], len(stream_payload))
                        self.assertEqual(st_after_up['file'], '/games/stream_up.bin')
                        mid_up_rates = [r for s, r in up_samples if s['bytes'] > 0 and r > 0]
                        self.assertTrue(mid_up_rates, 'Expected live upload rate samples > 0')
                        self.assertTrue(any(s['file'] == '/games/stream_up.bin' for s, _ in up_samples))
                        avg_up_rate = sum(mid_up_rates) / len(mid_up_rates)
                        self.assertGreater(avg_up_rate, 2.0 * 1024 * 1024)
                        self.assertLess(avg_up_rate, 7.0 * 1024 * 1024)
                        self.assertEqual(
                            hashlib.sha256((root / 'games/stream_up.bin').read_bytes()).hexdigest(),
                            stream_sha)

                        # 2. Live download polling (must increment `sent` mid-transfer!)
                        down_done = threading.Event()
                        down_err = []
                        down_hash = hashlib.sha256()

                        def do_download():
                            try:
                                t0 = time.monotonic()
                                got = 0

                                def sink(chunk):
                                    nonlocal got
                                    down_hash.update(chunk)
                                    got += len(chunk)
                                    target = t0 + got / (4 * 1024 * 1024)
                                    delay = target - time.monotonic()
                                    if delay > 0:
                                        time.sleep(delay)

                                ftp.retrbinary('RETR /games/stream_up.bin', sink, blocksize=65536)
                            except Exception as exc:
                                down_err.append(exc)
                            finally:
                                down_done.set()

                        t = threading.Thread(target=do_download)
                        t.start()
                        down_samples = []
                        prev_sent = st_after_up['sent']
                        prev_t = time.monotonic()
                        while not down_done.is_set():
                            time.sleep(0.45)
                            st = self._poll_ftp(proc)
                            now = time.monotonic()
                            rate = (st['sent'] - prev_sent) / (now - prev_t)
                            prev_sent, prev_t = st['sent'], now
                            down_samples.append((st, rate))
                        t.join()
                        self.assertFalse(down_err, down_err)
                        self.assertEqual(down_hash.hexdigest(), stream_sha)

                        st_after_down = self._poll_ftp(proc)
                        self.assertEqual(st_after_down['sent'], len(stream_payload))
                        mid_down_rates = [r for s, r in down_samples if 0 < s['sent'] < len(stream_payload) and r > 0]
                        self.assertTrue(mid_down_rates,
                                        f'Expected mid-download rate > 0, got samples: {down_samples}')
                        avg_down_rate = sum(mid_down_rates) / len(mid_down_rates)
                        self.assertGreater(avg_down_rate, 1.5 * 1024 * 1024)

                        # 3. Unthrottled 32 MB loopback throughput benchmark
                        bench_data = b'NXRPG_FTP_BENCH_' * (2 * 1024 * 1024)  # 32 MB
                        t0 = time.monotonic()
                        ftp.storbinary('STOR /games/bench32.bin', io.BytesIO(bench_data), blocksize=262144)
                        up_mb_s = len(bench_data) / (1024 * 1024) / max(time.monotonic() - t0, 1e-6)

                        t0 = time.monotonic()
                        got_bytes = 0
                        ftp.retrbinary('RETR /games/bench32.bin',
                                       lambda b: None, blocksize=262144)
                        down_mb_s = len(bench_data) / (1024 * 1024) / max(time.monotonic() - t0, 1e-6)
                        self.assertEqual((root / 'games/bench32.bin').stat().st_size, len(bench_data))
                        self.assertGreater(up_mb_s, 20.0, f'FTP upload unexpectedly slow: {up_mb_s:.1f} MB/s')
                        self.assertGreater(down_mb_s, 20.0, f'FTP download unexpectedly slow: {down_mb_s:.1f} MB/s')
                        print(f'\n[FTP BENCH] 32MB upload={up_mb_s:.1f} MB/s, download={down_mb_s:.1f} MB/s, '
                              f'throttled_up_sampled={avg_up_rate / (1024*1024):.2f} MB/s, '
                              f'throttled_down_sampled={avg_down_rate / (1024*1024):.2f} MB/s')
                finally:
                    proc.stdin.write('Q\n')
                    proc.stdin.flush()
                    proc.stdin.close()
                    proc.wait(timeout=5)
                self.assertEqual(proc.returncode, 0)

    def test_mtp_upload_download_interrupted_and_rate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'nx.rpg'
            root.mkdir()
            log_path = Path(tmp) / 'mtp_progress.log'

            # 8 MB payload (4 x 2MB chunks) to verify chunk-by-chunk progress & rate
            payload = os.urandom(8 * 1024 * 1024)
            payload_sha = hashlib.sha256(payload).hexdigest()

            def build_upload(filename, data, tx, complete=True):
                meta = _mtp_upload_meta(filename, len(data))
                pkt = _mtp_packet(2, 0x100D, tx + 1, data)
                return (_mtp_cmd(0x100C, tx, 0x10001, 0xFFFFFFFF)
                        + _mtp_packet(2, 0x100C, tx, meta)
                        + _mtp_cmd(0x100D, tx + 1)
                        + (pkt if complete else pkt[:-5000]))

            # Interactive pipe so we can time throttled upload + download + interrupted upload
            with subprocess.Popen([self.executable, 'mtp', root, log_path],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE) as proc:
                def send_and_expect_ok(cmd_bytes):
                    proc.stdin.write(cmd_bytes)
                    proc.stdin.flush()
                    kind, code, _, _ = _read_mtp_container(proc.stdout)
                    self.assertEqual((kind, code), (3, 0x2001))

                # GetDeviceInfo + OpenSession
                proc.stdin.write(_mtp_cmd(0x1001, 0))
                proc.stdin.flush()
                self.assertEqual(_read_mtp_container(proc.stdout)[0], 2)
                self.assertEqual(_read_mtp_container(proc.stdout)[1], 0x2001)
                send_and_expect_ok(_mtp_cmd(0x1002, 1, 1))

                # 1. Throttled 8 MB upload (~6 MB/s -> ~1.3s)
                proc.stdin.write(_mtp_cmd(0x100C, 2, 0x10001, 0xFFFFFFFF)
                                 + _mtp_packet(2, 0x100C, 2, _mtp_upload_meta('big.bin', len(payload))))
                proc.stdin.flush()
                kind, code, _, body = _read_mtp_container(proc.stdout)
                self.assertEqual((kind, code), (3, 0x2001))
                handle = struct.unpack('<III', body)[2]

                proc.stdin.write(_mtp_cmd(0x100D, 3) + struct.pack('<IHHI', len(payload) + 12, 2, 0x100D, 3))
                reader = _ThrottledReader(payload, 6 * 1024 * 1024)
                while True:
                    chunk = reader.read(256 * 1024)
                    if not chunk:
                        break
                    proc.stdin.write(chunk)
                    proc.stdin.flush()
                kind, code, _, _ = _read_mtp_container(proc.stdout)
                self.assertEqual((kind, code), (3, 0x2001))
                self.assertEqual(hashlib.sha256((root / 'big.bin').read_bytes()).hexdigest(), payload_sha)

                # 2. Download back via GetObject (0x1009) — must report progress & sent bytes!
                t0 = time.monotonic()
                proc.stdin.write(_mtp_cmd(0x1009, 4, handle))
                proc.stdin.flush()
                kind, code, tx, data = _read_mtp_container(proc.stdout)
                self.assertEqual((kind, code, tx), (2, 0x1009, 4))
                self.assertEqual(hashlib.sha256(data).hexdigest(), payload_sha)
                kind, code, _, _ = _read_mtp_container(proc.stdout)
                self.assertEqual((kind, code), (3, 0x2001))
                mtp_down_mb_s = len(data) / (1024 * 1024) / max(time.monotonic() - t0, 1e-6)

                # 3. Unthrottled 32 MB upload benchmark
                bench = b'NXRPG_MTP_BENCH_' * (2 * 1024 * 1024)
                t0 = time.monotonic()
                proc.stdin.write(_mtp_cmd(0x100C, 5, 0x10001, 0xFFFFFFFF)
                                 + _mtp_packet(2, 0x100C, 5, _mtp_upload_meta('bench32.bin', len(bench))))
                proc.stdin.flush()
                self.assertEqual(_read_mtp_container(proc.stdout)[1], 0x2001)
                proc.stdin.write(_mtp_cmd(0x100D, 6) + _mtp_packet(2, 0x100D, 6, bench))
                proc.stdin.flush()
                self.assertEqual(_read_mtp_container(proc.stdout)[1], 0x2001)
                mtp_up_mb_s = len(bench) / (1024 * 1024) / max(time.monotonic() - t0, 1e-6)

                # 4. Interrupted upload (must NOT decrement bytes, NOT increment files, NO temp file left)
                proc.stdin.write(build_upload('incomplete.bin', payload[:4 * 1024 * 1024], 7, complete=False))
                proc.stdin.flush()
                proc.stdin.close()
                proc.wait(timeout=10)

            self.assertEqual(sorted(p.name for p in root.iterdir()), ['bench32.bin', 'big.bin'])
            self.assertEqual((root / 'bench32.bin').stat().st_size, len(bench))

            # Inspect every MTP progress record
            events = []
            for line in log_path.read_text().splitlines():
                ts, fname, cur, tot, rx, tx, files, fin, ok, up = line.split('|')
                events.append({
                    'ts': float(ts), 'file': fname, 'cur': int(cur), 'tot': int(tot),
                    'bytes': int(rx), 'sent': int(tx), 'files': int(files),
                    'finished': bool(int(fin)), 'ok': bool(int(ok)), 'upload': bool(int(up)),
                })

            # Monotonic counters (critical for SpeedSampler)
            for a, b in zip(events, events[1:]):
                self.assertGreaterEqual(b['bytes'], a['bytes'], 'MTP bytes went backwards!')
                self.assertGreaterEqual(b['sent'], a['sent'], 'MTP sent went backwards!')

            up_big = [e for e in events if e['file'] == '/big.bin' and e['upload']]
            down_big = [e for e in events if e['file'] == '/big.bin' and not e['upload']]
            inc_ev = [e for e in events if e['file'] == '/incomplete.bin']

            self.assertGreaterEqual(len(up_big), 5, 'Expected chunk-by-chunk upload progress')
            self.assertGreaterEqual(len(down_big), 5, 'Expected chunk-by-chunk download progress')
            self.assertEqual(down_big[-1]['sent'], len(payload))
            self.assertEqual(events[-1]['files'], 2, 'Interrupted upload must not increment files')
            self.assertTrue(inc_ev and inc_ev[-1]['finished'] and not inc_ev[-1]['ok'])

            # Check throttled MTP upload rate from progress timestamps
            dt = up_big[-1]['ts'] - up_big[0]['ts']
            mtp_throttled_rate = len(payload) / max(dt, 1e-6)
            self.assertGreater(mtp_throttled_rate, 3.0 * 1024 * 1024)
            self.assertLess(mtp_throttled_rate, 10.0 * 1024 * 1024)
            self.assertGreater(mtp_up_mb_s, 30.0, f'MTP upload unexpectedly slow: {mtp_up_mb_s:.1f} MB/s')
            self.assertGreater(mtp_down_mb_s, 30.0, f'MTP download unexpectedly slow: {mtp_down_mb_s:.1f} MB/s')
            print(f'\n[MTP BENCH] 32MB upload={mtp_up_mb_s:.1f} MB/s, 8MB download={mtp_down_mb_s:.1f} MB/s, '
                  f'throttled_up_measured={mtp_throttled_rate / (1024*1024):.2f} MB/s')


if __name__ == '__main__':
    unittest.main()
