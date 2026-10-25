#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define CONNECT_TIMEOUT_MS 3000
#define RETRY_DELAY_MS 2000

static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool running;
static volatile bool retarget;
static char target_host[128];
static int target_port;
static NetState state = NET_IDLE;
static PropValue values[P_COUNT];

static void set_state(NetState s) {
    pthread_mutex_lock(&lock);
    state = s;
    if (s != NET_CONNECTED) memset(values, 0, sizeof(values));
    pthread_mutex_unlock(&lock);
}

static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static int open_connection(const char *host, int port) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    int rc = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc < 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    if (rc < 0) {
        struct pollfd pfd = {fd, POLLOUT, 0};
        if (poll(&pfd, 1, CONNECT_TIMEOUT_MS) <= 0) {
            close(fd);
            return -1;
        }
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            close(fd);
            return -1;
        }
    }
    return fd;
}

static bool send_all(int fd, const char *buf, int len) {
    while (len > 0) {
        int n = send(fd, buf, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd = {fd, POLLOUT, 0};
                poll(&pfd, 1, 500);
                continue;
            }
            return false;
        }
        buf += n;
        len -= n;
    }
    return true;
}

static bool subscribe_all(int fd) {
    char line[128];
    for (int i = 0; i < P_COUNT; i++) {
        int len = snprintf(line, sizeof(line), "subscribe %s\r\n", prop_name(i));
        if (!send_all(fd, line, len)) return false;
    }
    return true;
}

// Reads lines until the connection drops or the client is stopped/retargeted.
static void read_loop(int fd) {
    char buf[8192];
    int used = 0;
    while (running && !retarget) {
        struct pollfd pfd = {fd, POLLIN, 0};
        int pr = poll(&pfd, 1, 250);
        if (pr < 0) return;
        if (pr == 0) continue;  // the server only sends changes, silence is normal
        int n = recv(fd, buf + used, sizeof(buf) - 1 - used, 0);
        if (n == 0) return;
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return;
        }
        used += n;
        buf[used] = 0;

        char *start = buf;
        char *nl;
        pthread_mutex_lock(&lock);
        while ((nl = strchr(start, '\n'))) {
            *nl = 0;
            if (nl > start && nl[-1] == '\r') nl[-1] = 0;
            prop_parse_line(start, values);
            start = nl + 1;
        }
        pthread_mutex_unlock(&lock);

        used -= (int)(start - buf);
        memmove(buf, start, used);
        if (used >= (int)sizeof(buf) - 1) used = 0;  // absurdly long line, drop it
    }
    send_all(fd, "disconnect\r\n", 12);
}

static void *thread_main(void *arg) {
    (void)arg;
    while (running) {
        char host[128];
        int port;
        pthread_mutex_lock(&lock);
        snprintf(host, sizeof(host), "%s", target_host);
        port = target_port;
        retarget = false;
        pthread_mutex_unlock(&lock);

        set_state(NET_CONNECTING);
        int fd = open_connection(host, port);
        if (fd >= 0 && subscribe_all(fd)) {
            set_state(NET_CONNECTED);
            read_loop(fd);
        }
        if (fd >= 0) close(fd);
        if (!running) break;
        if (retarget) continue;
        set_state(NET_FAILED);
        for (int waited = 0; waited < RETRY_DELAY_MS && running && !retarget; waited += 100) sleep_ms(100);
    }
    set_state(NET_IDLE);
    return NULL;
}

void net_start(const char *host, int port) {
    snprintf(target_host, sizeof(target_host), "%s", host);
    target_port = port;
    running = true;
    pthread_create(&thread, NULL, thread_main, NULL);
}

void net_stop(void) {
    if (!running) return;
    running = false;
    pthread_join(thread, NULL);
}

void net_set_target(const char *host, int port) {
    pthread_mutex_lock(&lock);
    snprintf(target_host, sizeof(target_host), "%s", host);
    target_port = port;
    retarget = true;
    pthread_mutex_unlock(&lock);
}

NetState net_snapshot(PropValue *out) {
    pthread_mutex_lock(&lock);
    memcpy(out, values, sizeof(values));
    NetState s = state;
    pthread_mutex_unlock(&lock);
    return s;
}
