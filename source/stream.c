#include "stream.h"

#include "sockutil.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <jpeglib.h>

// Wire format, PC to Switch: "SDFR" + uint32 little-endian length + JPEG.
// Switch to PC: one byte 'A' after each frame, so the PC never queues
// frames faster than the Switch can show them.
#define MAX_JPEG (2 * 1024 * 1024)
#define MAX_W 1920
#define MAX_H 1080
#define CONNECT_TIMEOUT_MS 5000
#define RETRY_DELAY_MS 1500
#define STALL_TIMEOUT_MS 5000

static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool running;
static char host[128];
static int port;

static NetState state = NET_IDLE;
static uint8_t *frame;  // newest decoded frame, guarded by lock
static int frame_w, frame_h;
static unsigned frame_seq;
static float fps;
static char last_error[128];

static void set_state(NetState s) {
    pthread_mutex_lock(&lock);
    state = s;
    pthread_mutex_unlock(&lock);
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int open_connection(void) {
    char err[128] = "";
    int fd = tcp_connect(host, port, CONNECT_TIMEOUT_MS, err, sizeof(err));
    pthread_mutex_lock(&lock);
    snprintf(last_error, sizeof(last_error), "%s", err);
    pthread_mutex_unlock(&lock);
    if (fd < 0) return -1;
    int big = 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &big, sizeof(big));
    return fd;
}

// Reads exactly len bytes. Returns false on disconnect, stall or stop.
static bool read_full(int fd, uint8_t *buf, int len) {
    int waited = 0;
    while (len > 0 && running) {
        struct pollfd pfd = {fd, POLLIN, 0};
        int pr = poll(&pfd, 1, 100);
        if (pr < 0) return false;
        if (pr == 0) {
            if ((waited += 100) >= STALL_TIMEOUT_MS) return false;
            continue;
        }
        int n = recv(fd, buf, len, 0);
        if (n == 0) return false;
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return false;
        }
        buf += n;
        len -= n;
        waited = 0;
    }
    return len == 0;
}

struct jpeg_err {
    struct jpeg_error_mgr mgr;
    jmp_buf jump;
};

static void on_jpeg_error(j_common_ptr cinfo) {
    longjmp(((struct jpeg_err *)cinfo->err)->jump, 1);
}

// Decodes into out (RGBA). Returns false for a broken or oversized frame.
static bool decode_jpeg(const uint8_t *data, int size, uint8_t *out, int *w, int *h) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_err err;
    cinfo.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = on_jpeg_error;
    if (setjmp(err.jump)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char *)data, size);
    jpeg_read_header(&cinfo, TRUE);
    if ((int)cinfo.image_width > MAX_W || (int)cinfo.image_height > MAX_H) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_EXT_RGBA;
    cinfo.dct_method = JDCT_IFAST;
    cinfo.do_fancy_upsampling = FALSE;
    jpeg_start_decompress(&cinfo);
    *w = cinfo.output_width;
    *h = cinfo.output_height;
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = out + (size_t)cinfo.output_scanline * *w * 4;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

static void receive_loop(int fd, uint8_t *jpeg, uint8_t *pixels) {
    double window_start = now_s();
    int window_frames = 0;
    while (running) {
        uint8_t header[8];
        if (!read_full(fd, header, 8) || memcmp(header, "SDFR", 4) != 0) return;
        uint32_t len = header[4] | header[5] << 8 | header[6] << 16 | (uint32_t)header[7] << 24;
        if (len == 0 || len > MAX_JPEG || !read_full(fd, jpeg, (int)len)) return;

        int w, h;
        if (decode_jpeg(jpeg, (int)len, pixels, &w, &h)) {
            pthread_mutex_lock(&lock);
            memcpy(frame, pixels, (size_t)w * h * 4);
            frame_w = w;
            frame_h = h;
            frame_seq++;
            pthread_mutex_unlock(&lock);
            window_frames++;
        }
        if (send(fd, "A", 1, 0) != 1 && errno != EAGAIN) return;

        double t = now_s();
        if (t - window_start >= 1.0) {
            pthread_mutex_lock(&lock);
            fps = (float)(window_frames / (t - window_start));
            pthread_mutex_unlock(&lock);
            window_start = t;
            window_frames = 0;
        }
    }
}

static void *thread_main(void *arg) {
    (void)arg;
    uint8_t *jpeg = malloc(MAX_JPEG);
    uint8_t *pixels = malloc((size_t)MAX_W * MAX_H * 4);
    while (running && jpeg && pixels) {
        set_state(NET_CONNECTING);
        int fd = open_connection();
        if (fd >= 0) {
            set_state(NET_CONNECTED);
            receive_loop(fd, jpeg, pixels);
            close(fd);
        }
        if (!running) break;
        set_state(NET_FAILED);
        pthread_mutex_lock(&lock);
        fps = 0;
        pthread_mutex_unlock(&lock);
        for (int waited = 0; waited < RETRY_DELAY_MS && running; waited += 100) {
            struct timespec ts = {0, 100 * 1000000L};
            nanosleep(&ts, NULL);
        }
    }
    free(jpeg);
    free(pixels);
    set_state(NET_IDLE);
    return NULL;
}

void stream_start(const char *h, int p) {
    if (running) return;
    snprintf(host, sizeof(host), "%s", h);
    port = p;
    if (!frame) frame = malloc((size_t)MAX_W * MAX_H * 4);
    frame_w = frame_h = 0;
    frame_seq = 0;
    fps = 0;
    running = true;
    pthread_create(&thread, NULL, thread_main, NULL);
}

void stream_stop(void) {
    if (!running) return;
    running = false;
    pthread_join(thread, NULL);
    free(frame);
    frame = NULL;
}

bool stream_get_frame(unsigned last_seq, void *dst, int dst_size, StreamInfo *info) {
    bool copied = false;
    pthread_mutex_lock(&lock);
    info->state = state;
    info->seq = frame_seq;
    info->width = frame_w;
    info->height = frame_h;
    info->fps = fps;
    snprintf(info->error, sizeof(info->error), "%s", last_error);
    if (frame && frame_seq != last_seq && frame_w * frame_h * 4 <= dst_size) {
        memcpy(dst, frame, (size_t)frame_w * frame_h * 4);
        copied = true;
    }
    pthread_mutex_unlock(&lock);
    return copied;
}
