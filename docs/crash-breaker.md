# Crash breaker

`core/lifecycle/crash_breaker.h` and `crash_breaker.cpp` (`lumora::lifecycle`) are the crash-loop breaker of `docs/android-lifecycle.md` section 10.7, item 2. The caller keeps a crash log per scene and backend and persists it. After N crashes inside a time window the scene is disabled, and a scene whose Vulkan runs crashed repeatedly is moved to GLES before the scene itself is disabled.

Evidence label: unit tests only (CI-verified).

Pure functions on small values: no clock reads, no I/O, no floating point, no persistence. The caller passes the current time in milliseconds from any wall clock that is monotonic enough. N and the window are pinned at M2; the defaults below are the plan's provisional proposal.

## Types and policy

| Type | Field | Meaning |
| --- | --- | --- |
| `CrashLog` | `count` | valid entries, 0 to `kMaxCrashes` (16) |
| | `times_ms[16]` | crash times, oldest inserted first (not sorted) |
| `Policy` | `max_crashes` | crashes in the window, both backends together, that disable the scene (default 3) |
| | `window_ms` | window length (default 86400000, 24 hours) |
| | `backend_crashes` | Vulkan crashes in the window that move the scene to GLES (default 2) |

`valid(policy)` is true when `1 <= max_crashes <= 16`, `1 <= backend_crashes <= max_crashes` and `window_ms >= 1`.

## Window

A crash at time `t` is inside the window at `now` when `age < window_ms`, where `age = now - t` and `age` is 0 when `t > now`. A clock that moved backwards therefore never hides a crash. A crash exactly `window_ms` old is outside. A zero window counts nothing, future times included. A log whose `count` is above 16 is read as having 16 entries.

`recent_crashes(log, now, window)` counts the entries inside the window. It does no policy validation.

## record_crash

Returns false and touches nothing when `log` is null or the policy is invalid. A corrupted count is not repaired on failure. Otherwise, in this order (the numbering in the header is not the order):

1. A count above 16 is clamped to 16.
2. Entries outside the window are dropped. The order of the rest is kept.
3. If the log is still full (16 entries), entry 0 is dropped.
4. `now_ms` is appended at the end, even if it is older than the last entry (a backwards clock). Entries are never sorted.
5. Every unused slot (index >= count) is set to 0, so a persisted log is canonical.

The function has no backend argument: the caller passes the Vulkan or the GLES log.

## decide

Returns false and leaves `out` untouched when `out` is null or the policy is invalid. A null log counts as empty. With `recent(x)` the count in the policy window:

1. `recent(vulkan) + recent(gles) >= max_crashes`: `Disable`, backend `Vulkan`.
2. Else if `gles_available` and `recent(vulkan) >= backend_crashes`: `Run` on `Gles`.
3. Else `Run` on `Vulkan`.

GLES crashes count toward the total only, never toward the Vulkan threshold. When GLES is unavailable and the Vulkan threshold is reached with the total below `max_crashes`, the answer is `Run` on `Vulkan`. The total is at most 32, so the sum cannot overflow.

`to_string` gives "Run", "Disable", "Vulkan", "Gles", and "unknown" for any other value (never null).

## Bounds

| Item | Bound |
| --- | --- |
| log capacity | 16 entries |
| times | any 64-bit value including `UINT64_MAX`; ages use a guarded subtraction, so nothing wraps |
| `count` on input | any 32-bit value; read as at most 16 |
| `window_ms` | 1 to `UINT64_MAX` for record and decide; any value for `recent_crashes` |

## Tests

`tools/crash_breaker_ref.py` is an independent reference in Python. It prints a generated block with the sweep parameters and FNV-1a 64 digests over the full result of every case (for `record_crash`, the return value, the count and all 16 slots), which the test recomputes from the C++ functions.

```
python3 tools/crash_breaker_ref.py                                              # print the block
python3 tools/crash_breaker_ref.py --check tests/crash_breaker/crash_breaker_tests.cpp
```

`tests/crash_breaker/crash_breaker_tests.cpp` compares exact values only:

- window edges, a backwards clock, `UINT64_MAX` times, full and nearly full logs, canonical zero slots, a corrupted count above 16, invalid policies, null pointers, and that nothing is written on failure;
- exhaustive sweeps checked against a vector-based oracle that shares no code with the implementation: `record_crash` over every log of up to 4 entries with values 0..6 for `now` 0..8 and windows 1..4 (100,836 cases), `recent_crashes` over the same logs with windows 0..5 (151,254 cases), `decide` over every pair of logs of up to 3 entries with values 0..2 plus null, `now` 0..4, windows 1..3, every valid `(max_crashes <= 4, backend_crashes)` and both GLES availabilities (504,300 cases), and `valid()` for `max_crashes` and `backend_crashes` 0..18 with three windows (1,083 cases);
- 30,000 seeded random logs with extreme times and counts up to `UINT32_MAX`, against the oracle and a digest.

The provisional defaults are asserted in one test case ("provisional default policy"). Every other test passes an explicit policy, so pinning N and the window at M2 means editing that one case.

## Mutation check

65 hand-written source mutants of `crash_breaker.cpp` (window comparison and future-time rule, count clamp, each `valid()` bound, loop bounds, pruning, the full-log drop, append, sorting, count, slot zeroing, writes on failure, each `decide` rule, null handling and the `to_string` text) were each compiled against the unchanged tests. 62 were killed and 3 survived. Five of the kills are crashes only (null checks removed): they die on a null dereference, not on a failed assertion.

Survivors, each argued:

- future test `t >= now` instead of `t > now`: at `t == now` both give age 0. Equivalent.
- count clamp `>=` instead of `>`: at a count of 16 both give 16. Equivalent.
- `max_crashes >= 0` instead of `>= 1`: an unsigned value is always `>= 0`, and `backend_crashes >= 1` with `backend_crashes <= max_crashes` already forces `max_crashes >= 1`. Redundant check, equivalent.

Not run: Windows (CI only), Android, Vulkan/GLES, the editor, `tools/ci/test.py`, fuzzing, a full local build.
