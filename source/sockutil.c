#include "sockutil.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void describe(int code, char *err, int size) {
    switch (code) {
    case ECONNREFUSED:
        snprintf(err, size, "PC refused the connection (is the port right and the server running?)");
        break;
    case ETIMEDOUT:
        snprintf(err, size, "no reply from the PC (firewall, wrong IP, or different network)");
        break;
    case ENETUNREACH:
    case ENETDOWN:
        snprintf(err, size, "the Switch isn't connected to a network");
        break;
    case EHOSTUNREACH:
        snprintf(err, size, "PC not found on this network");
        break;
    default:
        snprintf(err, size, "connection failed (%s)", strerror(code));
        break;
    }
}

int tcp_connect(const char *host, int port, int timeout_ms, char *err, int err_size) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // Plain IPs skip the DNS service, which homebrew DNS blockers can break.
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        struct addrinfo hints = {0}, *res = NULL;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
            snprintf(err, err_size, "\"%s\" isn't a valid PC address", host);
            return -1;
        }
        addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        describe(errno, err, err_size);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) return fd;
    if (errno != EINPROGRESS && errno != EWOULDBLOCK && errno != EAGAIN) {
        describe(errno, err, err_size);
        close(fd);
        return -1;
    }
    struct pollfd pfd = {fd, POLLOUT, 0};
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0) {
        describe(pr == 0 ? ETIMEDOUT : errno, err, err_size);
        close(fd);
        return -1;
    }
    int so_err = 0;
    socklen_t len = sizeof(so_err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &len) == 0 && so_err != 0) {
        describe(so_err, err, err_size);
        close(fd);
        return -1;
    }
    return fd;
}

void local_ip(char *out, int size) {
#ifdef __SWITCH__
    struct in_addr a = {.s_addr = (in_addr_t)gethostid()};
    if (a.s_addr == 0 || a.s_addr == htonl(INADDR_LOOPBACK)) {
        snprintf(out, size, "not connected");
        return;
    }
    inet_ntop(AF_INET, &a, out, size);
#else
    snprintf(out, size, "desktop");
#endif
}
