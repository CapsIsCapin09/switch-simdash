#!/usr/bin/env python3
"""Pretends to be the SimHub Property Server so SimDash can be tried without a game.

Usage: python3 tools/fake_simhub.py [port]
"""
import math
import socket
import sys
import threading
import time


def fmt_span(sec):
    m, s = divmod(sec, 60)
    h, m = divmod(int(m), 60)
    return f"{h:02d}:{m:02d}:{s:010.7f}"


def values(t):
    rpm_pct = (t * 0.35) % 1.0
    max_rpm = 8500
    gear = str(int(t * 0.35) % 6 + 1)
    lap = t % 92.0
    return {
        "dcp.GameRunning": ("boolean", "True"),
        "dcp.gd.Gear": ("string", gear),
        "dcp.gd.SpeedLocal": ("double", f"{80 + 140 * rpm_pct:.2f}"),
        "dcp.gd.SpeedLocalUnit": ("string", "km/h"),
        "dcp.gd.Rpms": ("double", f"{max_rpm * rpm_pct:.1f}"),
        "dcp.gd.CarSettings_MaxRPM": ("double", str(max_rpm)),
        "dcp.gd.CarSettings_RedLineRPM": ("double", "8100"),
        "dcp.gd.CarSettings_CurrentDisplayedRPMPercent": ("double", f"{rpm_pct * 100:.1f}"),
        "dcp.gd.CurrentLapTime": ("timespan", fmt_span(lap)),
        "dcp.gd.LastLapTime": ("timespan", fmt_span(91.234)),
        "dcp.gd.BestLapTime": ("timespan", fmt_span(90.876)),
        "dcp.gd.Position": ("integer", "3"),
        "dcp.gd.OpponentsCount": ("integer", "20"),
        "dcp.gd.CurrentLap": ("integer", "5"),
        "dcp.gd.TotalLaps": ("integer", "12"),
        "dcp.gd.Fuel": ("double", f"{42 - t * 0.05:.2f}"),
        "dcp.gd.FuelPercent": ("double", f"{(42 - t * 0.05) / 60 * 100:.1f}"),
        "dcp.gd.FuelUnit": ("string", "Liters"),
        "dcp.gd.TCLevel": ("integer", "3"),
        "dcp.gd.ABSLevel": ("integer", "2"),
        "dcp.gd.BrakeBias": ("double", "56.5"),
        "dcp.gd.PitLimiterOn": ("integer", "0"),
        "dcp.gd.Flag_Name": ("string", "Yellow" if int(t / 10) % 3 == 1 else ""),
        "dcp.gd.Throttle": ("double", f"{100 * rpm_pct:.0f}"),
        "dcp.gd.Brake": ("double", f"{max(0, 100 * math.sin(t)) :.0f}"),
        "dcp.gd.WaterTemperature": ("double", "88.5"),
        "dcp.gd.OilTemperature": ("double", "102.1"),
        "dcp.gd.TyreTemperatureFrontLeft": ("double", "86.0"),
        "dcp.gd.TyreTemperatureFrontRight": ("double", "91.0"),
        "dcp.gd.TyreTemperatureRearLeft": ("double", "62.0"),
        "dcp.gd.TyreTemperatureRearRight": ("double", "106.0"),
        "dcp.gd.TyrePressureFrontLeft": ("double", "26.1"),
        "dcp.gd.TyrePressureFrontRight": ("double", "26.3"),
        "dcp.gd.TyrePressureRearLeft": ("double", "25.8"),
        "dcp.gd.TyrePressureRearRight": ("double", "25.9"),
        "dcp.gd.TyrePressureUnit": ("string", "psi"),
        "dcp.gd.SessionTimeLeft": ("timespan", fmt_span(max(0, 1500 - t))),
    }


def client(conn):
    start = time.time()
    subs = set()
    sent = {}
    conn.sendall(b"SimHub Property Server\r\n")
    conn.settimeout(0.1)
    buf = b""
    try:
        while True:
            try:
                data = conn.recv(4096)
                if not data:
                    return
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    parts = line.decode().strip().split(" ", 1)
                    if parts[0] == "subscribe" and len(parts) == 2:
                        subs.add(parts[1])
                        conn.sendall(f"Property {parts[1]} double (null)\r\n".encode())
                    elif parts[0] == "disconnect":
                        return
            except socket.timeout:
                pass
            vals = values(time.time() - start)
            for name in subs:
                if name in vals and sent.get(name) != vals[name]:
                    typ, val = vals[name]
                    conn.sendall(f"Property {name} {typ} {val}\r\n".encode())
                    sent[name] = vals[name]
    except OSError:
        pass
    finally:
        conn.close()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 18082
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen()
    print(f"fake SimHub Property Server on port {port}")
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=client, args=(conn,), daemon=True).start()


if __name__ == "__main__":
    main()
