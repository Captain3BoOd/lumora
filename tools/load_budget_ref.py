#!/usr/bin/env python3
"""Independent reference for core/package/load_budget (not run in CI).

A second implementation in plain Python, written from the rules in core/package/load_budget.h and not
from the C++ code. Python integers do not overflow, so every 64-bit condition is an explicit comparison
with 2**64 - 1. It builds sample scenes at the boundary of every limit and prints the expected results
as a C++ block that the tests assert against.

  python3 tools/load_budget_ref.py            print the generated C++ block
  python3 tools/load_budget_ref.py --check F  fail unless the block in F matches
"""
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/load_budget_ref.py)"
END = "// END GENERATED REFERENCE"
M64 = 2**64 - 1
M32 = 2**32 - 1

FORMATS = {"R8": ("R8", 1, False), "Rg8": ("Rg8", 2, False), "Rgba8": ("Rgba8", 4, False),
           "Bc1": ("Bc1", 8, True), "Bc7": ("Bc7", 16, True), "Bad": ("Bad", 0, False)}

DEFAULTS = dict(max_textures=1024, dim=8192, tex_each=128 << 20, tex_total=512 << 20,
                scr_each=1 << 20, scr_total=4 << 20, nodes=100000, aud_each=32 << 20,
                aud_total=128 << 20, aud_ms=10 * 60 * 1000)
LIMIT_ORDER = ["max_textures", "dim", "tex_each", "tex_total", "scr_each", "scr_total", "nodes",
               "aud_each", "aud_total", "aud_ms"]


def chain(w, h):
    return max(w, h).bit_length()


def tex_status(t):
    """(status, bytes): status is ok, invalid or overflow."""
    w, h, fmt, mips, layers = t
    if w == 0 or h == 0 or layers == 0 or mips == 0 or fmt == "Bad" or mips > chain(w, h):
        return "invalid", 0
    _, unit, block = FORMATS[fmt]
    total = 0
    for level in range(mips):
        lw, lh = max(1, w >> level), max(1, h >> level)
        if block:
            lw, lh = -(-lw // 4), -(-lh // 4)
        total += lw * lh * unit
    total *= layers
    if total > M64:
        return "overflow", 0
    return "ok", total


def evaluate(scene):
    tex, scripts, nodes, audio, lim = scene
    tt = st = at = 0
    R = lambda v, i=0: (v, i, tt, st, at)
    if len(tex) > lim["max_textures"]:
        return ("TooManyTextures", 0, 0, 0, 0)
    for i, t in enumerate(tex):
        status, b = tex_status(t)
        if status == "invalid":
            return ("BadTexture", i, tt, 0, 0)
        if t[0] > lim["dim"] or t[1] > lim["dim"]:
            return ("TextureDimension", i, tt, 0, 0)
        if status == "overflow":
            return ("SizeOverflow", i, tt, 0, 0)
        if b > lim["tex_each"]:
            return ("TextureTooLarge", i, tt, 0, 0)
        if tt + b > M64:
            return ("SizeOverflow", i, tt, 0, 0)
        if tt + b > lim["tex_total"]:
            return ("TextureBudget", i, tt, 0, 0)
        tt += b
    for i, s in enumerate(scripts):
        if s > lim["scr_each"]:
            return ("ScriptTooLarge", i, tt, st, 0)
        if st + s > M64:
            return ("SizeOverflow", i, tt, st, 0)
        if st + s > lim["scr_total"]:
            return ("ScriptBudget", i, tt, st, 0)
        st += s
    if nodes > lim["nodes"]:
        return ("TooManyNodes", 0, tt, st, 0)
    for i, (b, ms) in enumerate(audio):
        if b > lim["aud_each"]:
            return ("AudioTooLarge", i, tt, st, at)
        if ms > lim["aud_ms"]:
            return ("AudioTooLong", i, tt, st, at)
        if at + b > M64:
            return ("SizeOverflow", i, tt, st, at)
        if at + b > lim["aud_total"]:
            return ("AudioBudget", i, tt, st, at)
        at += b
    return ("Ok", 0, tt, st, at)


def L(**kw):
    d = dict(DEFAULTS)
    d.update(kw)
    return d


BIG = L(max_textures=M64, dim=M32, tex_each=M64, tex_total=M64, scr_each=M64, scr_total=M64,
        nodes=M64, aud_each=M64, aud_total=M64, aud_ms=M64)


def vals_around(v, hi):
    """0 limit handled by the caller; items just below, at, just above, and at the maximum."""
    out = []
    for x in (v - 1, v, v + 1, hi):
        if 0 <= x <= hi and x not in out:
            out.append(x)
    return out


def scenes():
    S = []
    add = lambda name, tex=(), scripts=(), nodes=0, audio=(), lim=DEFAULTS: S.append(
        (name, (list(tex), list(scripts), nodes, list(audio), lim)))
    # Counts.
    for lv in (0, 1, 3, M64):
        for n in sorted({max(lv - 1, 0), lv, lv + 1} & set(range(0, 6))):
            add("textures %d limit %d" % (n, lv), tex=[(1, 1, "R8", 1, 1)] * n, lim=BIG if lv == M64 else L(max_textures=lv))
    for lv in (0, 1, 100000, M64):
        for n in vals_around(lv, M64):
            add("nodes %d limit %d" % (n, lv), nodes=n, lim=L(nodes=lv))
    # Dimension.
    for lv in (0, 1, 8192, M32):
        for d in vals_around(lv, M32):
            add("dim width %d limit %d" % (d, lv), tex=[(d, 1, "R8", 1, 1)], lim=L(dim=lv, tex_each=M64, tex_total=M64))
            add("dim height %d limit %d" % (d, lv), tex=[(1, d, "R8", 1, 1)], lim=L(dim=lv, tex_each=M64, tex_total=M64))
    # One texture size: 4x4 Rgba8 is 64 bytes. R8 w x 1 with layers L is w * L bytes.
    for lv in (0, 63, 64, 65, M64):
        for b in (0, 1, 63, 64, 65):
            if b >= 1:
                add("tex each %d limit %d" % (b, lv), tex=[(b, 1, "R8", 1, 1)], lim=L(tex_each=lv, tex_total=M64))
    add("tex each at max", tex=[(M32, M32, "R8", 1, 1)], lim=L(dim=M32, tex_each=M64, tex_total=M64))
    add("tex each max minus", tex=[(M32, M32, "R8", 1, 1)], lim=L(dim=M32, tex_each=M64 - 2**33, tex_total=M64))
    # Texture total.
    for lv in (0, 10, M64):
        for t in vals_around(lv, 30):
            add("tex total %d limit %d" % (t, lv), tex=[(t - t // 2, 1, "R8", 1, 1), (t // 2, 1, "R8", 1, 1)] if t > 1 else [(1, 1, "R8", 1, 1)] * t,
                lim=L(tex_each=M64, tex_total=lv))
    add("tex total overflow second", tex=[(M32, M32, "R8", 1, 1)] * 2, lim=BIG)
    add("tex total overflow third", tex=[(M32, M32, "R8", 1, 1)] * 3, lim=BIG)
    add("tex total exact max", tex=[(M32, M32, "R8", 1, 1), (M32, 1, "R8", 1, 2)], lim=BIG)
    add("tex total max plus one", tex=[(M32, M32, "R8", 1, 1), (M32, 1, "R8", 1, 2), (1, 1, "R8", 1, 1)], lim=BIG)
    # Scripts.
    for lv in (0, 100, M64):
        for b in vals_around(lv, M64):
            add("script each %d limit %d" % (b, lv), scripts=[b], lim=L(scr_each=lv, scr_total=M64))
    for lv in (0, 10, M64):
        for t in vals_around(lv, 30):
            add("script total %d limit %d" % (t, lv), scripts=[t - t // 2, t // 2] if t > 1 else [t], lim=L(scr_each=M64, scr_total=lv))
    add("script total overflow", scripts=[M64, 1], lim=BIG)
    add("script total overflow late", scripts=[5, M64 - 5, 1, 0], lim=BIG)
    add("script total exact max", scripts=[M64 - 5, 5], lim=BIG)
    # Audio.
    for lv in (0, 100, M64):
        for b in vals_around(lv, M64):
            add("audio each %d limit %d" % (b, lv), audio=[(b, 0)], lim=L(aud_each=lv, aud_total=M64))
    for lv in (0, 1000, 600000, M64):
        for ms in vals_around(lv, M64):
            add("audio ms %d limit %d" % (ms, lv), audio=[(1, ms)], lim=L(aud_ms=lv, aud_total=M64))
    for lv in (0, 10, M64):
        for t in vals_around(lv, 30):
            add("audio total %d limit %d" % (t, lv), audio=[(t - t // 2, 1), (t // 2, 1)] if t > 1 else [(t, 1)], lim=L(aud_each=M64, aud_total=lv))
    add("audio total overflow", audio=[(M64, 0), (1, 0)], lim=BIG)
    add("audio total exact max", audio=[(M64 - 5, 0), (5, 0)], lim=BIG)
    add("audio too large before too long", audio=[(101, M64)], lim=L(aud_each=100, aud_ms=5))
    add("audio too long before budget", audio=[(10, 6)], lim=L(aud_each=100, aud_ms=5, aud_total=1))
    # Order of stages and totals carried into later failures.
    ok_t = [(4, 4, "Rgba8", 1, 1)]
    add("texture failure hides script failure", tex=[(0, 1, "R8", 1, 1)], scripts=[M64], nodes=M64, audio=[(M64, M64)], lim=L())
    add("script failure keeps texture total", tex=ok_t, scripts=[7, 1 << 21], nodes=M64, audio=[(M64, 0)], lim=L())
    add("node failure keeps both totals", tex=ok_t, scripts=[7, 8], nodes=100001, audio=[(M64, 0)], lim=L())
    add("audio failure keeps all totals", tex=ok_t, scripts=[7, 8], nodes=5, audio=[(3, 4), (33 << 20, 0)], lim=L())
    add("ok carries totals", tex=ok_t + [(2, 2, "R8", 1, 1)], scripts=[7, 8], nodes=5, audio=[(3, 4), (5, 6)], lim=L())
    add("empty scene", lim=L())
    add("zero values pass", tex=[], scripts=[0], nodes=0, audio=[(0, 0)], lim=L(scr_each=0, scr_total=0, nodes=0, aud_each=0, aud_total=0, aud_ms=0))
    add("failed texture reports accepted total", tex=ok_t * 3 + [(1, 1, "R8", 2, 1)], lim=L())
    add("budget failure reports accepted total", tex=[(4, 4, "Rgba8", 1, 1)] * 3, lim=L(tex_total=150))
    # Texture validity, order and sizes.
    bad = [(0, 4, "R8", 1, 1), (4, 0, "R8", 1, 1), (4, 4, "R8", 0, 1), (4, 4, "R8", 1, 0), (4, 4, "Bad", 1, 1),
           (4, 4, "R8", 4, 1), (1, 1, "R8", 2, 1), (8, 3, "R8", 5, 1), (M32, M32, "R8", 33, 1),
           (0, 9000, "R8", 1, 1), (9000, 9000, "R8", 15, 1), (9000, 1, "R8", 0, 1), (9000, 9000, "Bad", 1, 1)]
    for t in bad:
        add("bad texture %s" % (t,), tex=[t], lim=L())
    add("dimension after valid", tex=[(9000, 9000, "R8", 14, 1)], lim=L())
    add("dimension limit 0", tex=[(1, 1, "R8", 1, 1)], lim=L(dim=0))
    add("chain edge ok", tex=[(M32, 1, "R8", 32, 1)], lim=L(dim=M32, tex_each=M64, tex_total=M64))
    add("chain edge 4096", tex=[(4096, 4096, "R8", 13, 1)], lim=L(tex_each=M64, tex_total=M64))
    add("chain edge 4096 plus one", tex=[(4096, 4096, "R8", 14, 1)], lim=L())
    for fmt in ("R8", "Rg8", "Rgba8", "Bc1", "Bc7"):
        for (w, h, m, ly) in [(1, 1, 1, 1), (4, 4, 1, 1), (5, 5, 3, 1), (9, 3, 4, 2), (256, 256, 9, 1), (256, 256, 1, 6),
                              (7, 13, 4, 3), (4096, 2048, 13, 1), (8192, 8192, 14, 1), (3, 100, 7, 1)]:
            add("size %s %dx%d m%d l%d" % (fmt, w, h, m, ly), tex=[(w, h, fmt, m, ly)], lim=L(tex_each=M64, tex_total=M64))
    for fmt in ("R8", "Rg8", "Rgba8", "Bc1", "Bc7"):
        add("overflow level %s" % fmt, tex=[(M32, M32, fmt, 1, 1)], lim=BIG)
        add("overflow chain %s" % fmt, tex=[(M32, M32, fmt, 32, 1)], lim=BIG)
        add("overflow layers %s" % fmt, tex=[(M32, 1, fmt, 1, M32)], lim=BIG)
        add("overflow before each %s" % fmt, tex=[(M32, M32, fmt, 32, M32)], lim=L(dim=M32, tex_each=5))
    add("overflow after dimension", tex=[(M32, M32, "Rgba8", 1, 1)], lim=L(dim=M32 - 1))
    add("overflow fits 64 bits barely", tex=[(M32, 1, "R8", 1, M32)], lim=BIG)
    add("texture too large with small limit", tex=[(100, 100, "Rgba8", 1, 1)], lim=L(tex_each=39999))
    add("texture at each limit", tex=[(100, 100, "Rgba8", 1, 1)], lim=L(tex_each=40000))
    return S


def cs(n):
    return str(n) + "ull"


def render():
    L_ = [BEGIN]
    L_.append("struct RefLimits {")
    L_.append("  std::uint64_t max_textures;")
    L_.append("  std::uint32_t dim;")
    L_.append("  std::uint64_t tex_each, tex_total, scr_each, scr_total, nodes, aud_each, aud_total, aud_ms;")
    L_.append("};")
    L_.append("struct RefTex {")
    L_.append("  std::uint32_t w, h;")
    L_.append("  TextureFormat fmt;")
    L_.append("  std::uint32_t mips, layers;")
    L_.append("};")
    L_.append("struct RefAudio {")
    L_.append("  std::uint64_t bytes, ms;")
    L_.append("};")
    L_.append("struct RefScene {")
    L_.append("  const char* name;")
    L_.append("  std::vector<RefTex> tex;")
    L_.append("  std::vector<std::uint64_t> scripts;")
    L_.append("  std::uint64_t nodes;")
    L_.append("  std::vector<RefAudio> audio;")
    L_.append("  RefLimits lim;")
    L_.append("  BudgetVerdict verdict;")
    L_.append("  std::size_t index;")
    L_.append("  std::uint64_t tex_bytes, script_bytes, audio_bytes;")
    L_.append("};")
    L_.append("struct RefTexSize {")
    L_.append("  RefTex t;")
    L_.append("  int status;  // 0 ok, 1 invalid, 2 overflow")
    L_.append("  std::uint64_t bytes;")
    L_.append("};")
    L_.append("const RefLimits kRefDefaults = {%s, %d, %s};" % (
        cs(DEFAULTS["max_textures"]), DEFAULTS["dim"],
        ", ".join(cs(DEFAULTS[k]) for k in LIMIT_ORDER[2:])))
    L_.append("const RefScene kRefScenes[] = {")
    seen = set()
    sizes = []
    for name, sc in scenes():
        tex, scripts, nodes, audio, lim = sc
        v, i, a, b, c = evaluate(sc)
        for t in tex:
            if t not in seen and len(tex) == 1:
                seen.add(t)
                st, by = tex_status(t)
                sizes.append((t, st, by))
        fm = lambda f: "static_cast<TextureFormat>(200)" if f == "Bad" else "TextureFormat::" + f
        tx = ", ".join("{%du, %du, %s, %du, %du}" % (w, h, fm(f), m, l) for (w, h, f, m, l) in tex)
        ax = ", ".join("{%s, %s}" % (cs(x), cs(y)) for (x, y) in audio)
        sx = ", ".join(cs(s) for s in scripts)
        lm = "{%s, %du, %s}" % (cs(lim["max_textures"]), lim["dim"], ", ".join(cs(lim[k]) for k in LIMIT_ORDER[2:]))
        L_.append('    {"%s", {%s}, {%s}, %s, {%s}, %s, BudgetVerdict::%s, %d, %s, %s, %s},' % (
            name, tx, sx, cs(nodes), ax, lm, v, i, cs(a), cs(b), cs(c)))
    L_.append("};")
    L_.append("const RefTexSize kRefSizes[] = {")
    for (w, h, f, m, l), st, by in sizes:
        fm = "static_cast<TextureFormat>(200)" if f == "Bad" else "TextureFormat::" + f
        L_.append("    {{%du, %du, %s, %du, %du}, %d, %s}," % (w, h, fm, m, l, {"ok": 0, "invalid": 1, "overflow": 2}[st], cs(by)))
    L_.append("};")
    L_.append(END)
    return "\n".join(L_) + "\n"


def main(argv):
    block = render()
    if len(argv) == 3 and argv[1] == "--check":
        text = open(argv[2]).read()
        a, b = text.index(BEGIN), text.index(END) + len(END) + 1
        if text[a:b] != block:
            print("reference block differs; regenerate it", file=sys.stderr)
            return 1
        return 0
    sys.stdout.write(block)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
