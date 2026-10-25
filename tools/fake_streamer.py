#!/usr/bin/env python3
"""Pretends to be SimDash Streamer: sends animated JPEG frames so the
SimHub dash view can be tried without Windows. Needs Pillow.

Usage: python3 tools/fake_streamer.py [port]
"""
import io
import socket
import struct
import sys
import time

from PIL import Image, ImageDraw


def frame(t):
    im = Image.new("RGB", (1280, 720), (10, 20, 40))
    d = ImageDraw.Draw(im)
    x = int((t * 300) % 1280)
    d.rectangle([x, 300, x + 120, 420], fill=(240, 60, 60))
    d.text((40, 40), f"fake SimHub dash  t={t:6.2f}", fill=(255, 255, 255))
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=70)
    return buf.getvalue()


def serve(conn):
    start = time.time()
    try:
        while True:
            data = frame(time.time() - start)
            conn.sendall(b"SDFR" + struct.pack("<I", len(data)) + data)
            if conn.recv(1) != b"A":
                return
            time.sleep(1 / 30)
    except OSError:
        pass
    finally:
        conn.close()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 18083
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen()
    print(f"fake SimDash Streamer on port {port}")
    while True:
        conn, _ = srv.accept()
        serve(conn)


if __name__ == "__main__":
    main()
