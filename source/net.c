#include "net.h"

#include "sockutil.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define CONNECT_TIMEOUT_MS 5000
#define RETRY_DELAY_MS 2000

static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool running;
static volatile bool retarget;
static char target_host[128];
static int target_port;
static NetState state = NET_IDLE;
static PropValue values[P_COUNT];
static char last_error[128];

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
        char err[128] = "";
        int fd = tcp_connect(host, port, CONNECT_TIMEOUT_MS, err, sizeof(err));
        if (fd >= 0 && subscribe_all(fd)) {
            set_state(NET_CONNECTED);
            read_loop(fd);
            snprintf(err, sizeof(err), "connection to the PC was lost");
        }
        pthread_mutex_lock(&lock);
        snprintf(last_error, sizeof(last_error), "%s", err);
        pthread_mutex_unlock(&lock);
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

void net_last_error(char *out, int size) {
    pthread_mutex_lock(&lock);
    snprintf(out, size, "%s", last_error);
    pthread_mutex_unlock(&lock);
}

NetState net_snapshot(PropValue *out) {
    pthread_mutex_lock(&lock);
    memcpy(out, values, sizeof(values));
    NetState s = state;
    pthread_mutex_unlock(&lock);
    return s;
}
