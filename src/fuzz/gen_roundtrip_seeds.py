#!/usr/bin/env python3
"""Generate seed corpus files for terminal_roundtrip_fuzzer.

The harness consumes with FuzzedDataProvider: two ConsumeIntegralInRange
scalars and one ConsumeBool from the BACK of the buffer, then the remaining
front bytes as the host display instruction stream:

    [host display instructions ...][resize byte][height byte][width byte]

(ConsumeBool: odd byte = true. ConsumeIntegralInRange(1, 200): value is
1 + (byte % 200).)

Run from src/fuzz/:

    python3 gen_roundtrip_seeds.py terminal_roundtrip_corpus
"""

import os
import sys

# Host display instruction payloads that build interesting terminal states
# for the frame encoder (see also hostbytes_corpus/).
PAYLOADS = {
    "text": b"Hello, mosh!\r\nline two\r\nline three\r\n",
    "escapes": (b"\x1b[2J\x1b[5;10H\x1b[1;31;44mred on blue\x1b[0m"
                b"\x1b[3@\x1b[2P\x1b[2L\x1b[3M"),
    # NOTE: a "scroll region + resize to 10x5 + 100 chars" payload was removed
    # from this generator: the state it builds deterministically fails the
    # round-trip invariant (cursor column off by one after diff/apply -- see
    # the finding recorded in the fuzzing run report). It is kept out of the
    # shipped corpus so the target stays fuzzable; the reproducer is
    # preserved with the run report.
    "sgr": b"\x1b[1;4;7;31;42mbold underline reverse\x1b[38;5;196mfg\x1b[0m",
    "osc": (b"\x1b]0;round trip title\x07\x1b]8;;https://mosh.org\x07"
            b"link\x1b]8;;\x07"),
    "wide": "\x1b[H漢字テスト wide\r\n".encode("utf-8") + b"y" * 79 + "国".encode("utf-8"),
    "combining": "e\xcc\x81comb\r\n\xcc\x80lead\r\n".encode("utf-8"),
    "modes": b"\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[?7h\x1b[?25h\x1b[?1049l",
    "decstbm-cursor": b"\x1b[10;40r\x1b[10;1Hinside region\nlines\n\x1bM\x1bD",
    "empty": b"",
}


def tail(do_resize, width, height):
    """FDP tail bytes: [resize byte][height byte][width byte]."""
    return bytes([1 if do_resize else 0, (height - 1) % 200, (width - 1) % 200])


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "terminal_roundtrip_corpus"
    os.makedirs(outdir, exist_ok=True)
    n = 0
    for name, payload in sorted(PAYLOADS.items()):
        variants = [(False, 80, 24), (True, 81, 25), (True, 10, 5), (True, 132, 43)]
        for do_resize, w, h in variants:
            fn = "%02d-%s-%s%dx%d" % (n, name, "r" if do_resize else "n", w, h)
            with open(os.path.join(outdir, fn), "wb") as f:
                f.write(payload + tail(do_resize, w, h))
            n += 1
    print("wrote %d seeds to %s" % (n, outdir))


if __name__ == "__main__":
    main()
