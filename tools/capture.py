#!/usr/bin/env python3
"""Capture the LVGL framebuffer over serial without resetting a UART-bridge board.

`screenshot.sh` works on the native-USB S3/C6 boards, but on the WROOM-32
(CP2102/CH340 bridge) opening the port toggles DTR/RTS and resets the board, so
the script either grabs a mid-boot frame or hangs (it has no overall timeout).

This helper opens with DTR/RTS deasserted to suppress the auto-reset, settles
past the boot greeting, re-sends `screenshot` until the device answers, and
bails on an overall deadline. It writes the raw RGB565 + dims; thresholding to
the true 1-bit look is done by the caller (see tools/mini_sim.py for the
luminance formula, or just use the simulator and skip hardware entirely).

Usage:  python3 tools/capture.py <port> <out.raw> <out.dims> [settle_s]
Example: python3 tools/capture.py /dev/cu.usbserial-0001 /tmp/s.raw /tmp/s.dims
"""
import serial, sys, time

port_path, raw_path, dims_path = sys.argv[1], sys.argv[2], sys.argv[3]
settle = float(sys.argv[4]) if len(sys.argv) > 4 else 6.0  # outlast the 1.8s boot greeting

port = serial.Serial()
port.port = port_path
port.baudrate = 115200
port.timeout = 1
port.dtr = False   # suppress the bridge's auto-reset on open
port.rts = False
port.open()
time.sleep(settle)
port.reset_input_buffer()

deadline = time.time() + 25
last_send = 0.0
w = h = raw_size = None
while time.time() < deadline:
    if time.time() - last_send > 2.0:        # board may have reset anyway — keep asking
        port.write(b"screenshot\n"); port.flush()
        last_send = time.time()
    line = port.readline().decode("utf-8", errors="replace").strip()
    if line.startswith("SCREENSHOT_START"):
        p = line.split(); w, h, raw_size = int(p[1]), int(p[2]), int(p[3]); break
    if line in ("SCREENSHOT_ERR", "SCREENSHOT_UNSUPPORTED"):
        print("device:", line, file=sys.stderr); sys.exit(1)

if raw_size is None:
    print("timeout waiting for SCREENSHOT_START", file=sys.stderr); sys.exit(1)

data = b""
while len(data) < raw_size:
    chunk = port.read(min(4096, raw_size - len(data)))
    if not chunk:
        print(f"timeout: {len(data)}/{raw_size}", file=sys.stderr); sys.exit(1)
    data += chunk

open(raw_path, "wb").write(data)
open(dims_path, "w").write(f"{w}x{h}\n")
port.close()
print(f"captured {w}x{h} ({len(data)} bytes)")
