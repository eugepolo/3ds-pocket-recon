#include "scanner.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static uint64_t now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static ScanState complete(Scanner *s) {
    struct timespec delay = { .tv_nsec = 1000000 };
    while (scanner_tick(s, now()) == SCAN_PENDING) nanosleep(&delay, NULL);
    return s->state;
}

int main(void) {
    Scanner s;
    scanner_init(&s);
    assert(scanner_valid_ipv4("127.0.0.1"));
    assert(!scanner_valid_ipv4("999.0.0.1"));
    assert(!scanner_valid_ipv4("127.1"));
    scanner_start(&s, "bad", 80, now(), 1000);
    assert(s.state == SCAN_ERROR && s.fd == -1);
    scanner_start(&s, "127.0.0.1", 0, now(), 1000);
    assert(s.state == SCAN_ERROR);

    int server = socket(AF_INET, SOCK_STREAM, 0);
    assert(server >= 0);
    struct sockaddr_in address = { .sin_family = AF_INET };
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(bind(server, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t size = sizeof(address);
    assert(getsockname(server, (struct sockaddr *)&address, &size) == 0);
    unsigned port = ntohs(address.sin_port);
    assert(listen(server, 8) == 0);
    scanner_start(&s, "127.0.0.1", port, now(), 1000);
    assert(complete(&s) == SCAN_OPEN && s.fd == -1);
    int accepted = accept(server, NULL, NULL);
    assert(accepted >= 0);
    close(accepted);

    scanner_start(&s, "127.0.0.1", port, now(), 1000);
    int pending = s.fd;
    scanner_cancel(&s);
    assert(s.state == SCAN_IDLE && s.fd == -1);
    if (pending >= 0) {
        assert(close(pending) == -1 && errno == EBADF);
    }
    close(server);
    scanner_start(&s, "127.0.0.1", port, now(), 1000);
    assert(complete(&s) == SCAN_CLOSED && s.fd == -1);

    // A socket with no writable data deterministically exercises the deadline.
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    scanner_init(&s);
    s.fd = pair[0]; s.state = SCAN_PENDING; s.deadline = 10;
    // poll asks POLLOUT, so fill the outgoing socket first.
    char data[4096] = {0};
    assert(fcntl(pair[0], F_SETFL, O_NONBLOCK) == 0);
    while (send(pair[0], data, sizeof(data), 0) > 0) {}
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
    assert(scanner_tick(&s, 11) == SCAN_TIMEOUT && s.fd == -1);
    close(pair[1]);
    puts("PASS: validation, open, refused, cancellation, timeout, socket cleanup");
}
