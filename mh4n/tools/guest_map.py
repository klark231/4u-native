"""Where the app's own addresses (config/guest_addrs.txt) are in a code mod that replaces the whole
code.bin with another build of the game.

Functions: the exact twin version_map found, else the similarity search's (build_game.map_to_mod).
Variables: the code that uses a variable has it in a literal pool, so the same pool word in the
update's code and in the mod's (the function at the same place, tools/version_map.py's shifts) gives
the variable's address in both. Every pool word of the code votes; a variable with no pool word of
its own takes the shift of its nearest neighbours when they agree.
"""
from __future__ import annotations

import bisect
import collections
import struct


def load(path: str) -> list[tuple[int, str, str]]:
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].split()
            if len(line) >= 3:
                out.append((int(line[0], 0), line[1], line[2]))
    return out


def _segment_end(header) -> int:
    end = header.data.address + header.data.padded_size + header.bss_size
    return (end + 0xFFF) & ~0xFFF


def data_votes(src_header, src_code: bytes, dst_header, dst_code: bytes, shifts) -> dict[int, collections.Counter]:
    """{variable address in the update: Counter of the addresses the mod's code has for it}."""
    lo = src_header.ro.address
    src_hi, dst_hi = _segment_end(src_header), _segment_end(dst_header)
    base = src_header.text.address
    intervals, starts = shifts.intervals, shifts.starts
    votes: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    limit = min(src_header.text.padded_size, len(src_code)) - 4
    for p in range(0, limit + 1, 4):
        v = struct.unpack_from("<I", src_code, p)[0]
        if not (lo <= v < src_hi):
            continue
        i = bisect.bisect_right(starts, base + p) - 1
        if i < 0 or (i + 1 < len(intervals) and intervals[i + 1][2] != intervals[i][2]):
            continue      # not inside one function's unchanged stretch: no sure twin
        q = p + intervals[i][2]
        if q < 0 or q + 4 > len(dst_code):
            continue
        w = struct.unpack_from("<I", dst_code, q)[0]
        if lo <= w < dst_hi:
            votes[v][w] += 1
    return votes


def build(src_header, src_code, dst_header, dst_code, shifts, rescued: dict, wanted: list) -> tuple[list, list[str]]:
    """[(address in the update, address in the mod's code)] for what config/guest_addrs.txt lists, and
    the lines to print. A variable with no sure place is left out (the app then uses the update's address); a
    function with none is paired with 0 (the app then doesn't call it)."""
    report, pairs = [], []
    votes = data_votes(src_header, src_code, dst_header, dst_code, shifts)
    evidence = sorted(votes)
    best = {v: c.most_common(1)[0][0] for v, c in votes.items()}
    for addr, kind, name in wanted:
        to, how = None, ""
        if kind == "fn":
            to = shifts.translate(addr)
            how = "exact twin"
            if to is None and addr in rescued:
                to, score = rescued[addr]
                how = f"similarity {score:.0%}"
        else:
            if addr in best:
                c = votes[addr]
                to = best[addr]
                how = f"{c[to]} of {sum(c.values())} pool words"
                if c[to] * 2 <= sum(c.values()):
                    to, how = None, "the pool words disagree"
            else:
                i = bisect.bisect_left(evidence, addr)
                lo = evidence[i - 1] if i > 0 else None
                hi = evidence[i] if i < len(evidence) else None
                if lo is not None and hi is not None and hi - lo <= 0x2000:
                    d_lo, d_hi = best[lo] - lo, best[hi] - hi
                    if d_lo == d_hi:
                        to, how = addr + d_lo, f"as its neighbours 0x{lo:08x} / 0x{hi:08x}"
                    else:
                        how = "its neighbours move differently"
                else:
                    how = "no pool word near it"
        if to is None and kind == "fn":
            # the app must not call into the middle of whatever is at the update's address in this code: 0 tells it
            # the function is not there (hooks.cpp CallGame leaves the call out, and with it what needs it)
            report.append(f"  warning: {name} 0x{addr:08x} not placed in the mod's code ({how}): the app won't call it")
            pairs.append((addr, 0))
            continue
        if to is None:
            report.append(f"  warning: {name} 0x{addr:08x} not placed in the mod's code ({how}): the app uses 0x{addr:08x}")
            continue
        report.append(f"  {name} 0x{addr:08x} -> 0x{to:08x} ({to - addr:+d}; {how})")
        if to != addr:
            pairs.append((addr, to))
    report.insert(0, f"addresses the app uses, placed in the mod's code ({len(pairs)} of {len(wanted)} moved):")
    return sorted(pairs), report
