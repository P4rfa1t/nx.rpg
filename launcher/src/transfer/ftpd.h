/* Anonymous passive FTP server. Only the shared root is accessible.
 * See LICENSE.rpynx for upstream attribution. */
#ifndef RSL_FTPD_H
#define RSL_FTPD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool running;          /* listening; false once stopped or after an error */
    int port;              /* the port it listens on */
    bool connected;        /* a client is connected */
    char client[16];       /* its IPv4 address, "" without one */
    char file[1024];        /* the last file sent or received, "" before one */
    unsigned files;        /* files received since the start */
    uint64_t bytes;        /* bytes received since the start */
    uint64_t sent;         /* bytes sent to the client since the start */
    uint64_t recv_ms;      /* time spent receiving them, for the speed */
    /* The importer's upload of one game: ALLO announces its size (bytes then
     * counts from expect_from), and renaming its .uploading-<id> folder into
     * place (importer/rsl/exporter.py) finishes it. */
    uint64_t expect;       /* bytes announced, 0 when none is under way */
    uint64_t expect_from;
    char done[64];         /* the id of the last game moved into place, "" if none */
    char error[128];       /* why it stopped by itself, "" if it did not */
} rsl_ftpd_status;

/* Listen on port (0: any free one) and serve in a new thread. Anyone on the
 * network who can reach the port is accepted, so only enable it on a trusted
 * network. The sockets must be usable already (libnx: socketInitialize).
 * False with err filled in if it cannot listen. */
bool rsl_ftpd_start(const char *root, int port, uint32_t bind_addr,
                    char *err, size_t errlen);

/* Stop serving and wait for the thread; a transfer in progress is cut off. */
void rsl_ftpd_stop(void);

/* A copy of the current state, for the screen. */
void rsl_ftpd_get(rsl_ftpd_status *st);

#endif
