#!/usr/bin/env python3
"""
MobileGL - scripts/bench_cs_e2e.py
Copyright (c) 2025-2026 MobileGL-Dev
Licensed under the GNU Lesser General Public License v3.0:
  https://www.gnu.org/licenses/gpl-3.0.txt
  https://www.gnu.org/licenses/lgpl-3.0.txt
SPDX-License-Identifier: LGPL-3.0-only

Round-trip latency benchmark for the C/S channel:
  Python FlatBuffers client -> AF_UNIX socket -> libMobileGL_FullServer.so
  (mobilegl_fullserver_run_socket) -> BackendObject vtable -> Response.
"""
from __future__ import annotations

import argparse
import ctypes
import os
import socket
import struct
import sys
import threading
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "MobileGL" / "MG_Protocol" / "gen" / "python"))

import flatbuffers
from MobileGL.Protocol.Wire import Command, GlClear, Message, Response

# MobileGLOpcode::glClear from generated_opcodes.h
GLCLEAR_OPCODE = 115
SESSION_ID = 5
SESSION_CREATE = 1_000_000
SESSION_DESTROY = 1_000_001


def build_control_message(session: int, opcode: int, token: int) -> bytes:
    builder = flatbuffers.Builder(0)
    Command.CommandStart(builder)
    Command.CommandAddOpcode(builder, opcode)
    Command.CommandAddSessionId(builder, session)
    Command.CommandAddToken(builder, token)
    command_off = Command.CommandEnd(builder)
    Message.MessageStart(builder)
    Message.MessageAddCommand(builder, command_off)
    message_off = Message.MessageEnd(builder)
    builder.Finish(message_off)
    return bytes(builder.Output())


def build_clear_message(session: int, token: int) -> bytes:
    builder = flatbuffers.Builder(0)
    GlClear.GlClearStart(builder)
    GlClear.GlClearAddMask(builder, 0)
    clear_off = GlClear.GlClearEnd(builder)
    Command.CommandStart(builder)
    Command.CommandAddOpcode(builder, GLCLEAR_OPCODE)
    Command.CommandAddSessionId(builder, session)
    Command.CommandAddToken(builder, token)
    Command.CommandAddClear(builder, clear_off)
    command_off = Command.CommandEnd(builder)
    Message.MessageStart(builder)
    Message.MessageAddCommand(builder, command_off)
    message_off = Message.MessageEnd(builder)
    builder.Finish(message_off)
    return bytes(builder.Output())


def roundtrip(sock: socket.socket, payload: bytes) -> Response.Response:
    sock.sendall(struct.pack("<I", len(payload)) + payload)
    header = b""
    while len(header) < 4:
        header += sock.recv(4 - len(header))
    size = struct.unpack("<I", header)[0]
    data = b""
    while len(data) < size:
        data += sock.recv(size - len(data))
    return Response.Response.GetRootAs(data, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--backend", default=None)
    args = parser.parse_args()

    build = REPO_ROOT / "build_agent"
    util = str(build / "MobileGL" / "MG_UtilRuntime" / "libMobileGL_UtilRuntime.so")
    backend = args.backend or str(build / "MobileGL" / "MG_Backend" / "BackendObject_DirectGLES.so")
    fullserver = str(build / "MobileGL" / "MG_FullServer" / "libMobileGL_FullServer.so")

    lib = ctypes.CDLL(fullserver)
    lib.mobilegl_fullserver_create.restype = ctypes.c_void_p
    lib.mobilegl_fullserver_create.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    lib.mobilegl_fullserver_start.argtypes = [ctypes.c_void_p]
    lib.mobilegl_fullserver_start.restype = ctypes.c_int
    lib.mobilegl_fullserver_run_socket.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
    lib.mobilegl_fullserver_run_socket.restype = ctypes.c_int
    lib.mobilegl_fullserver_destroy.argtypes = [ctypes.c_void_p]

    endpoint = "/tmp/mobilegl_bench.sock"
    try:
        os.unlink(endpoint)
    except FileNotFoundError:
        pass

    handle = lib.mobilegl_fullserver_create(util.encode(), backend.encode())
    if not handle:
        print("create failed", file=sys.stderr)
        return 1
    if lib.mobilegl_fullserver_start(handle) != 0:
        print("start failed", file=sys.stderr)
        return 1

    result = {}
    def server():
        result["code"] = lib.mobilegl_fullserver_run_socket(handle, endpoint.encode(),
                                                            args.iterations + 2)

    thread = threading.Thread(target=server)
    thread.start()

    # Wait for the server socket to appear.
    deadline = time.time() + 5.0
    while not os.path.exists(endpoint):
        if time.time() > deadline:
            print("server socket never appeared", file=sys.stderr)
            return 1
        time.sleep(0.01)

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(30)
    sock.connect(endpoint)

    # Session lifecycle: create before, destroy after the measured commands.
    created = roundtrip(sock, build_control_message(SESSION_ID, SESSION_CREATE, 9000))
    if created.Status() != 0:
        print("session create failed", file=sys.stderr)
        return 1

    latencies = []
    for i in range(args.iterations):
        payload = build_clear_message(SESSION_ID, i)
        started = time.perf_counter_ns()
        response = roundtrip(sock, payload)
        if response.Status() != 0:
            print(f"command {i} failed with status {response.Status()}", file=sys.stderr)
            return 1
        latencies.append(time.perf_counter_ns() - started)

    destroyed = roundtrip(sock, build_control_message(SESSION_ID, SESSION_DESTROY, 9001))
    if destroyed.Status() != 0:
        print("session destroy failed", file=sys.stderr)
        return 1

    sock.close()
    thread.join()
    if result.get("code") != 0:
        print("server failed", file=sys.stderr)
        return 1
    lib.mobilegl_fullserver_destroy(handle)

    avg_us = sum(latencies) / len(latencies) / 1000.0
    print(f"iterations={args.iterations}")
    print(f"avg_us={avg_us:.1f} min_us={min(latencies) / 1000.0:.1f} "
          f"max_us={max(latencies) / 1000.0:.1f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
