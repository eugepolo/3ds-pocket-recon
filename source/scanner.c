#include "scanner.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

bool scanner_valid_ipv4(const char *target) {
    struct in_addr addr;
    return inet_pton(AF_INET, target, &addr) == 1;
}

void scanner_init(Scanner *s) {
    *s = (Scanner){ .fd = -1, .state = SCAN_IDLE };
}

void scanner_cancel(Scanner *s) {
    if (s->fd >= 0) close(s->fd);
    scanner_init(s);
}

static void finish(Scanner *s, ScanState state, int error) {
    if (s->fd >= 0) close(s->fd);
    s->fd = -1;
    s->state = state;
    s->error = error;
}

static void failed(Scanner *s, int error) {
    finish(s, error == ECONNREFUSED ? SCAN_CLOSED :
              error == ETIMEDOUT ? SCAN_TIMEOUT : SCAN_ERROR, error);
}

void scanner_start(Scanner *s, const char *target, uint16_t port,
                   uint64_t now_ms, unsigned timeout_ms) {
    scanner_cancel(s);
    struct sockaddr_in address = { .sin_family = AF_INET,
                                   .sin_port = htons(port) };
    if (!port || inet_pton(AF_INET, target, &address.sin_addr) != 1) {
        finish(s, SCAN_ERROR, EINVAL);
        return;
    }
    s->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->fd < 0) { failed(s, errno); return; }
    int flags = fcntl(s->fd, F_GETFL, 0);
    if (flags < 0 || fcntl(s->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        failed(s, errno);
        return;
    }
    s->deadline = now_ms + timeout_ms;
    if (connect(s->fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
        finish(s, SCAN_OPEN, 0);
    } else if (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EALREADY) {
        s->state = SCAN_PENDING;
    } else {
        failed(s, errno);
    }
}

ScanState scanner_tick(Scanner *s, uint64_t now_ms) {
    if (s->state != SCAN_PENDING) return s->state;
    struct pollfd pfd = { .fd = s->fd, .events = POLLOUT };
    int ready = poll(&pfd, 1, 0);
    if (ready < 0 && errno != EINTR) failed(s, errno);
    else if (ready > 0) {
        int error = 0;
        socklen_t size = sizeof(error);
        if (pfd.revents & POLLNVAL) failed(s, EBADF);
        else if (getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0)
            failed(s, errno);
        else {
#ifdef __3DS__
            // SOC may retain -26 (EINPROGRESS) after connection succeeds.
            // Its SO_ERROR payload is not translated by libctru.
            // https://github.com/devkitPro/libctru/issues/412
            struct sockaddr_in peer;
            socklen_t peer_size = sizeof(peer);
            if (getpeername(s->fd, (struct sockaddr *)&peer, &peer_size) == 0) {
                finish(s, SCAN_OPEN, 0);
                return s->state;
            }
            if (error == -26 || error == -7 || error == -6 || !error) {
                if (now_ms >= s->deadline) finish(s, SCAN_TIMEOUT, ETIMEDOUT);
                return s->state;
            }
            if (error == -14) error = ECONNREFUSED;
            else if (error == -76) error = ETIMEDOUT;
#endif
            if (error) failed(s, error);
            else if (pfd.revents & POLLOUT) finish(s, SCAN_OPEN, 0);
            else finish(s, SCAN_ERROR, EIO);
        }
    }
    if (s->state == SCAN_PENDING && now_ms >= s->deadline)
        finish(s, SCAN_TIMEOUT, ETIMEDOUT);
    return s->state;
}

const char *scanner_label(ScanState state) {
    switch (state) {
        case SCAN_IDLE: return "idle";
        case SCAN_PENDING: return "connecting";
        case SCAN_OPEN: return "open";
        case SCAN_CLOSED: return "closed";
        case SCAN_TIMEOUT: return "no response";
        default: return "error";
    }
}
