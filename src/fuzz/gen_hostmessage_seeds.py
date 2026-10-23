#!/usr/bin/env python3
"""Generate seed corpus files for hostmessage_fuzzer.

Emits serialized HostBuffers::HostMessage protobufs (see
src/protobufs/hostinput.proto) directly in wire format -- no protoc needed:

  HostMessage  { repeated Instruction instruction = 1; }  tag 0x0a
  Instruction  { extensions 2 to max; }
    hostbytes  = 2  (HostBytes { optional bytes hoststring = 4; }) tag 0x12
    resize     = 3  (ResizeMessage { int32 width = 5; int32 height = 6; })
    echoack    = 7  (EchoAck { uint64 echo_ack_num = 8; })

Each seed group targets a distinct instruction-handling path in
Terminal::Complete::apply_string(). Run from src/fuzz/:

    python3 gen_hostmessage_seeds.py hostmessage_corpus
"""

import os
import sys


def varint(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def field_bytes(field, payload):
    """Length-delimited field: tag, varint length, payload."""
    return bytes([field << 3 | 2]) + varint(len(payload)) + payload


def field_varint(field, value):
    return bytes([field << 3 | 0]) + varint(value)


def hostbytes(hoststring):
    inner = field_bytes(4, hoststring)          # HostBytes.hoststring = 4
    return field_bytes(2, inner)                # Instruction.hostbytes = 2


def resize(width, height):
    inner = field_varint(5, width) + field_varint(6, height)  # 5,6 = w,h
    return field_bytes(3, inner)                # Instruction.resize = 3


def echoack(num):
    inner = field_varint(8, num)                # EchoAck.echo_ack_num = 8
    return field_bytes(7, inner)                # Instruction.echoack = 7


def hostmessage(*instructions):
    out = b""
    for inst in instructions:
        out += field_bytes(1, inst)             # HostMessage.instruction = 1
    return out


SEEDS = {
    "01-hostbytes-text": hostmessage(
        hostbytes(b"Hello from the server!\r\nline two\r\n")),
    "02-hostbytes-escapes": hostmessage(
        hostbytes(b"\x1b[2J\x1b[5;10H\x1b[1;31;44mred on blue\x1b[0m"
                  b"\x1b[3@\x1b[2P\x1b[2L\x1b[3M\x1b[2S\x1b[3T")),
    "03-hostbytes-scroll": hostmessage(
        hostbytes(b"\x1b[5;20r\x1b[24;1Hbottom\nscroll\nmore\n\x1b[3S"
                  b"\x1b[r")),
    "04-hostbytes-osc": hostmessage(
        hostbytes(b"\x1b]0;server title\x07\x1b]8;;https://mosh.org\x07"
                  b"link\x1b]8;;\x07")),
    "05-hostbytes-utf8": hostmessage(
        hostbytes("\x1b[H漢字テスト wide\r\n"
                  "e\xcc\x81 combining\r\n".encode("utf-8"))),
    "06-hostbytes-empty": hostmessage(hostbytes(b"")),
    "07-resize-small": hostmessage(resize(1, 1)),
    "08-resize-grow": hostmessage(resize(200, 50)),
    "09-resize-shrink": hostmessage(resize(10, 5)),
    "10-echoack": hostmessage(echoack(7)),
    "11-combo-resize-bytes": hostmessage(
        resize(132, 43),
        hostbytes(b"\x1b[Hresized to 132x43\r\n"),
        echoack(9)),
    "12-combo-bytes-bytes": hostmessage(
        hostbytes(b"\x1b[2Jfirst chunk"),
        hostbytes(b"\x1b[10;10Hsecond chunk after first")),
    "13-multi-empty-instructions": hostmessage(b"", b""),
    "14-hostbytes-dec-modes": hostmessage(
        hostbytes(b"\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?7h\x1b[?25h"
                  b"\x1b[?1049l")),
    "15-hostbytes-tabs-controls": hostmessage(
        hostbytes(b"\ta\tb\t\tc\x1b[3I\x1b[2Z\x08\x0d\x0a\x0b\x0c\x07")),
}


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "hostmessage_corpus"
    os.makedirs(outdir, exist_ok=True)
    for name, payload in sorted(SEEDS.items()):
        with open(os.path.join(outdir, name), "wb") as f:
            f.write(payload)
        print("%s: %d bytes" % (name, len(payload)))


if __name__ == "__main__":
    main()
