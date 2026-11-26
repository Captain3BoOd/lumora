#!/usr/bin/env python3
"""Independent reference for core/package/extract_plan (not run in CI).

A second implementation of the plan in plain Python, written from the rules in
core/package/extract_plan.h and not from the C++ code. It prints the expected result of a fixed set
of sample inputs as a C++ block, and the Latin-1 composition pairs taken from Python's own
unicodedata (the C++ fold table is checked against them in the tests).

  python3 tools/extract_plan_ref.py            print the generated C++ block
  python3 tools/extract_plan_ref.py --check F  fail unless the block in F matches
"""
import sys
import unicodedata

BEGIN = "// BEGIN GENERATED REFERENCE (tools/extract_plan_ref.py)"
END = "// END GENERATED REFERENCE"
M = 2**64 - 1
MARKS = [0x300, 0x301, 0x302, 0x303, 0x308, 0x30A, 0x327]
LETTERS = [chr(c) for c in list(range(65, 91)) + list(range(97, 123))]


def pairs():
    """letter + mark -> precomposed Latin-1 Supplement letter, from unicodedata."""
    out = {}
    for b in LETTERS:
        for m in MARKS:
            c = unicodedata.normalize("NFC", b + chr(m))
            if len(c) == 1 and 0xC0 <= ord(c) <= 0xFF:
                out[b + chr(m)] = c
    return out


PAIRS = pairs()


def norm(s):
    out, i = "", 0
    while i < len(s):
        if s[i:i + 2] in PAIRS:
            out += PAIRS[s[i:i + 2]]
            i += 2
        else:
            out += s[i]
            i += 1
    return out


def plan(entries, free, depth=16, path=1024, dirs=4096, reserve=64 << 20):
    names = [n for n, _ in entries]
    for i, n in enumerate(names):
        if len(n.encode()) > path:
            return ("PathTooLong", i, [], 0, 0)
        if n.count("/") + 1 > depth:
            return ("PathTooDeep", i, [], 0, 0)
    seen, order = set(), []
    for i, n in enumerate(names):
        parts = n.split("/")
        for k in range(1, len(parts)):
            d = "/".join(parts[:k])
            if d and d not in seen:
                seen.add(d)
                order.append((d, i))
    for i, n in enumerate(names):
        for d in seen:
            if n == d or n.encode().lower() == d.encode().lower() or norm(n) == norm(d):
                return ("FileDirectoryConflict", i, [], 0, 0)
    if len(order) > dirs:
        return ("TooManyDirectories", order[dirs][1], [], 0, 0)
    total = 0
    for i, (_, s) in enumerate(entries):
        if s > M - total:
            return ("SizeOverflow", i, [], 0, 0)
        total += s
    if total + reserve > M:
        return ("SizeOverflow", 0, [], 0, 0)
    need = total + reserve
    if free < need:
        return ("InsufficientSpace", 0, [], total, need)
    ds = sorted((d for d, _ in order), key=lambda d: (d.count("/"), d.encode()))
    return ("Ok", 0, ds, total, need)


def cleanup(entries, ds, completed, partial):
    n = len(entries)
    done = min(completed, n)
    out = []
    if partial and done < n:
        out.append(entries[done][0])
    out += [entries[k][0] for k in range(done - 1, -1, -1)]
    out += list(reversed(ds))
    return out


E, D = "\u00e9", "e\u0301"  # precomposed e-acute, decomposed e + combining acute
CASES = [
    ("empty", [], 100, {}),
    ("empty_exact_space", [], 64 << 20, {}),
    ("empty_short_space", [], (64 << 20) - 1, {}),
    ("one_top_level_file", [("a.txt", 10)], 1 << 30, {}),
    ("nested_dirs_depth_order", [("b/x", 1), ("a/c/d.txt", 2), ("a/y", 3), ("z", 4), ("a/c/e", 5)],
     1 << 30, {}),
    ("byte_order_within_depth", [("B/x", 1), ("a/x", 1), ("_/x", 1), ("\u00e9/x", 1)], 1 << 30, {}),
    ("case_distinct_directories", [("A/x", 1), ("a/y", 1)], 1 << 30, {}),
    ("file_is_dir_exact", [("a", 1), ("a/b", 1)], 1 << 30, {}),
    ("file_is_dir_exact_dir_first", [("a/b", 1), ("a", 1)], 1 << 30, {}),
    ("file_is_dir_nested", [("x/y/z", 1), ("q", 1), ("x/y", 1)], 1 << 30, {}),
    ("file_is_dir_ascii_fold", [("ok", 1), ("README", 1), ("readme/doc", 1)], 1 << 30, {}),
    ("file_is_dir_latin1_fold", [("caf" + D, 1), ("caf" + E + "/x", 1)], 1 << 30, {}),
    ("file_is_dir_latin1_fold_reverse", [("caf" + E, 1), ("caf" + D + "/x", 1)], 1 << 30, {}),
    ("folds_not_composed", [("E\u0301", 1), ("\u00e9/x", 1)], 1 << 30, {}),
    ("depth_equal_limit", [("a/b/c", 1)], 1 << 30, {"depth": 3}),
    ("depth_over_limit", [("a/b/c", 1)], 1 << 30, {"depth": 2}),
    ("depth_zero", [("a", 1)], 1 << 30, {"depth": 0}),
    ("path_equal_limit", [("a/bcd", 1)], 1 << 30, {"path": 5}),
    ("path_over_limit", [("ok", 1), ("a/bcde", 1)], 1 << 30, {"path": 5}),
    ("path_counts_bytes", [("\u00e9", 1)], 1 << 30, {"path": 1}),
    ("long_wins_over_deep_same_entry", [("a/b/c/d", 1)], 1 << 30, {"path": 3, "depth": 2}),
    ("first_failing_entry_wins", [("a/b/c", 1), ("abcdefgh", 1)], 1 << 30, {"path": 5, "depth": 2}),
    ("dirs_equal_limit", [("a/x", 1), ("b/y", 1)], 1 << 30, {"dirs": 2}),
    ("dirs_over_limit", [("a/x", 1), ("a/b/y", 1), ("c/z", 1)], 1 << 30, {"dirs": 2}),
    ("dirs_zero_top_level_only", [("a", 1), ("b", 1)], 1 << 30, {"dirs": 0}),
    ("dirs_zero_nested", [("a", 1), ("b/c", 1)], 1 << 30, {"dirs": 0}),
    ("dirs_prefixes_count", [("a/b/c/d", 1)], 1 << 30, {"dirs": 2}),
    ("sum_exactly_fits_space", [("a", 40), ("b", 60)], 100, {"reserve": 0}),
    ("space_short_by_one", [("a", 40), ("b", 60)], 99, {"reserve": 0}),
    ("reserve_counts", [("a", 40)], 140, {"reserve": 100}),
    ("reserve_short_by_one", [("a", 40)], 139, {"reserve": 100}),
    ("size_max_alone", [("a", M)], M, {"reserve": 0}),
    ("size_max_alone_with_reserve", [("a", M)], M, {"reserve": 1}),
    ("size_sum_overflow_second", [("a", M), ("b", 1)], M, {"reserve": 0}),
    ("size_sum_overflow_index", [("a", 1), ("b", M - 1), ("c", 1), ("d", 5)], M, {"reserve": 0}),
    ("reserve_max_empty", [], M, {"reserve": M}),
    ("reserve_max_one_byte", [("a", 1)], M, {"reserve": M}),
    ("free_zero_zero_reserve", [("a", 0)], 0, {"reserve": 0}),
    ("many_names_same_dir", [("d/f%d" % i, i) for i in range(5)], 1 << 30, {}),
    ("conflict_before_dir_count", [("a/x", 1), ("b/y", 1), ("a", 1)], 1 << 30, {"dirs": 1}),
    ("dir_count_before_size", [("a/x", M), ("b/y", M)], 1, {"dirs": 1}),
    ("size_before_space", [("a", M), ("b", 2)], 0, {}),
]

CLEANUPS = [(0, False), (0, True), (1, False), (1, True), (2, True), (3, True), (3, False), (9, True)]


def cs(s):
    out = ""
    for b in s.encode():
        if 32 <= b < 127 and chr(b) not in '"\\?':
            out += chr(b)
        else:
            out += "\\%03o" % b
    return '"%s"' % out


def render():
    lines = [BEGIN]
    lines.append("struct RefEntry {")
    lines.append("  const char* name;")
    lines.append("  std::uint64_t size;")
    lines.append("};")
    lines.append("struct RefCleanup {")
    lines.append("  std::size_t completed;")
    lines.append("  bool partial;")
    lines.append("  std::vector<const char*> paths;")
    lines.append("};")
    lines.append("struct RefCase {")
    lines.append("  const char* label;")
    lines.append("  std::vector<RefEntry> entries;")
    lines.append("  std::uint64_t free_bytes;")
    lines.append("  std::uint32_t max_depth;")
    lines.append("  std::uint64_t max_path_bytes;")
    lines.append("  std::uint64_t max_directories;")
    lines.append("  std::uint64_t reserve_bytes;")
    lines.append("  const char* verdict;")
    lines.append("  std::size_t entry_index;")
    lines.append("  std::vector<const char*> directories;")
    lines.append("  std::uint64_t total_bytes;")
    lines.append("  std::uint64_t required_free_bytes;")
    lines.append("  std::vector<RefCleanup> cleanups;")
    lines.append("};")
    lines.append("const RefCase kRefCases[] = {")
    for label, entries, free, kw in CASES:
        depth = kw.get("depth", 16)
        path = kw.get("path", 1024)
        dirs = kw.get("dirs", 4096)
        reserve = kw.get("reserve", 64 << 20)
        verdict, idx, ds, total, need = plan(entries, free, depth, path, dirs, reserve)
        ents = ", ".join("{%s, %du}" % (cs(n), s) if s < 2**63 else "{%s, %dull}" % (cs(n), s)
                         for n, s in entries)
        dl = ", ".join(cs(d) for d in ds)
        cl = []
        if verdict == "Ok":
            for completed, partial in CLEANUPS:
                paths = ", ".join(cs(p) for p in cleanup(entries, ds, completed, partial))
                cl.append("{%du, %s, {%s}}" % (completed, "true" if partial else "false", paths))
        else:
            for completed, partial in CLEANUPS[:2]:
                cl.append("{%du, %s, {}}" % (completed, "true" if partial else "false"))

        def n64(v):
            return "%dull" % v if v >= 2**31 else "%du" % v
        lines.append("    {%s,\n     {%s}, %s,\n     %du, %s, %s, %s,\n     \"%s\", %du, {%s}, %s, %s,\n"
                     "     {%s}}," % (cs(label), ents, n64(free), depth, n64(path), n64(dirs),
                                       n64(reserve), verdict, idx, dl, n64(total), n64(need),
                                       ", ".join(cl)))
    lines.append("};")
    lines.append("struct RefPair {")
    lines.append("  const char* decomposed;")
    lines.append("  const char* composed;")
    lines.append("};")
    lines.append("const RefPair kRefPairs[] = {")
    for k in sorted(PAIRS):
        lines.append("    {%s, %s}," % (cs(k), cs(PAIRS[k])))
    lines.append("};")
    lines.append(END)
    return "\n".join(lines) + "\n"


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
