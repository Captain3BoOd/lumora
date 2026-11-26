#!/usr/bin/env python3
"""Independent reference for core/lifecycle/mic_state (not run in CI).

The table is written from the plan's "Microphone capture states" table in docs/android-lifecycle.md section 10.7
(columns "Entered by" and "Left by"), then compared with the transition list in the header. They differ in one
pair only: the header sends Inactive + PermissionRevoked to Stopped, which is slightly broader than the plan's
wording (the plan lists "permission revoked" as an entry into Stopped without a source state, and says nothing
about Inactive). The reference follows the header there and says so below. It prints a C++ block with the full
table, the flags, the names and FNV-1a 64 digests over exhaustive sweeps, which the tests recompute.

  python3 tools/mic_state_ref.py            print the generated C++ block
  python3 tools/mic_state_ref.py --check F  fail unless the block in F matches
"""
import itertools
import sys

BEGIN = "// BEGIN GENERATED REFERENCE (tools/mic_state_ref.py)"
END = "// END GENERATED REFERENCE"

STATES = ["Inactive", "Active", "Paused", "Stopped"]
EVENTS = ["UserActivate", "Hidden", "Visible", "ServiceKilled", "CaptureRefused", "PermissionRevoked", "UserStop"]

# From the plan's table. Each row: (state, event, new state).
# Inactive: "Left by: user activation".
# Active: "Left by: hidden (to Paused); service killed or capture refused (to Stopped)"; "Stopped entered by: process
#   killed, permission revoked, user stop in the Task Manager, start refused".
# Paused: "Left by: wallpaper visible again (to Active)"; Stopped entered by the same four causes.
# Stopped: "Left by: user-driven activation only; there is never an automatic restart loop".
PLAN = [
    ("Inactive", "UserActivate", "Active"),
    ("Active", "Hidden", "Paused"),
    ("Active", "ServiceKilled", "Stopped"),
    ("Active", "CaptureRefused", "Stopped"),
    ("Active", "PermissionRevoked", "Stopped"),
    ("Active", "UserStop", "Stopped"),
    ("Paused", "Visible", "Active"),
    ("Paused", "ServiceKilled", "Stopped"),
    ("Paused", "CaptureRefused", "Stopped"),
    ("Paused", "PermissionRevoked", "Stopped"),
    ("Paused", "UserStop", "Stopped"),
    ("Stopped", "UserActivate", "Active"),
]
# The header's list adds exactly one pair beyond the plan's table.
HEADER_EXTRA = [("Inactive", "PermissionRevoked", "Stopped")]
HEADER = {
    "Inactive": {"UserActivate": "Active", "PermissionRevoked": "Stopped"},
    "Active": {"Hidden": "Paused", "ServiceKilled": "Stopped", "CaptureRefused": "Stopped",
               "PermissionRevoked": "Stopped", "UserStop": "Stopped"},
    "Paused": {"Visible": "Active", "ServiceKilled": "Stopped", "CaptureRefused": "Stopped",
               "PermissionRevoked": "Stopped", "UserStop": "Stopped"},
    "Stopped": {"UserActivate": "Active"},
}

TABLE = {}
for s, e, n in PLAN + HEADER_EXTRA:
    assert (s, e) not in TABLE
    TABLE[(s, e)] = n
flat = {(s, e): n for s, row in HEADER.items() for e, n in row.items()}
assert flat == TABLE, "header list and plan table differ beyond the one documented pair"

# Flags from the plan's "Foreground service" and "AudioRecord" columns.
SERVICE = {"Inactive": False, "Active": True, "Paused": True, "Stopped": False}
CAPTURING = {"Inactive": False, "Active": True, "Paused": False, "Stopped": False}

BAD_STATES = [4, 5, 127, 128, 255]
BAD_EVENTS = [7, 8, 127, 128, 255]


def nxt(state, event):
    """state and event are integers; out-of-range values follow the header."""
    if state >= len(STATES):
        state = STATES.index("Stopped")
    if event >= len(EVENTS):
        return state
    return STATES.index(TABLE.get((STATES[state], EVENTS[event]), STATES[state]))


class Fnv:
    def __init__(self):
        self.h = 0xcbf29ce484222325

    def byte(self, b):
        self.h ^= b & 0xFF
        self.h = (self.h * 0x100000001b3) % 2**64


def digests():
    f = Fnv()
    n = 0
    for s in list(range(4)) + BAD_STATES:
        for e in list(range(7)) + BAD_EVENTS:
            f.byte(nxt(s, e))
            n += 1
    pairs = f.h
    f = Fnv()
    m = 0
    for s in range(4):
        for seq in itertools.product(range(7), repeat=4):
            cur = s
            for e in seq:
                cur = nxt(cur, e)
                f.byte(cur)
            m += 1
    seqs = f.h
    return {"pairs": pairs, "pair_cases": n, "seqs": seqs, "seq_cases": m}


def render():
    d = digests()
    L = [BEGIN]
    L.append("// Expected next state, rows are states (Inactive, Active, Paused, Stopped), columns are events (%s)." % ", ".join(EVENTS))
    L.append("const std::uint8_t kRefNext[4][7] = {")
    for s in range(4):
        L.append("    {%s}," % ", ".join(str(nxt(s, e)) for e in range(7)))
    L.append("};")
    L.append("const bool kRefService[4] = {%s};" % ", ".join("true" if SERVICE[s] else "false" for s in STATES))
    L.append("const bool kRefCapturing[4] = {%s};" % ", ".join("true" if CAPTURING[s] else "false" for s in STATES))
    L.append("const char* const kRefStateNames[4] = {%s};" % ", ".join('"%s"' % s for s in STATES))
    L.append("const char* const kRefEventNames[7] = {%s};" % ", ".join('"%s"' % e for e in EVENTS))
    L.append("const std::uint8_t kRefBadStates[] = {%s};" % ", ".join(str(v) for v in BAD_STATES))
    L.append("const std::uint8_t kRefBadEvents[] = {%s};" % ", ".join(str(v) for v in BAD_EVENTS))
    L.append("const std::uint64_t kRefDigest_pairs = 0x%016Xull;" % d["pairs"])
    L.append("const std::uint64_t kRefDigest_sequences = 0x%016Xull;" % d["seqs"])
    L.append("const std::size_t kRefPairCases = %d;" % d["pair_cases"])
    L.append("const std::size_t kRefSequenceCases = %d;" % d["seq_cases"])
    L.append(END)
    return "\n".join(L) + "\n"


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
