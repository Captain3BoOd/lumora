# Render gate

`core/lifecycle/render_gate.h` and `render_gate.cpp` (`lumora::lifecycle`) are a pure state machine over three reported facts (surface, visible, screen on) and a running flag. It decides when the frame loop starts and stops, as in `docs/android-lifecycle.md` section 10.1 item 3.

Evidence label: unit tests only (CI-verified).

Bool and integer only: no clock reads, no floating point, no libm, no wide types, no allocation. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<cstddef>`, `<cstdint>` and `<string_view>`.

## State

`RenderGate` holds `surface` (default false), `visible` (false), `screen_on` (true, the screen is assumed on until told otherwise) and `running` (false). All 16 bool combinations are valid objects; `running` need not agree with the facts in a hand-built object.

## should_render

`surface && visible && screen_on`. It never reads `running`.

## apply_event

Order of checks:

1. `g` null: return false, nothing written.
2. Event outside the enum (any value other than 0 to 5): return false, nothing written, including `*action`.
3. The matching fact is set: `SurfaceCreated` surface true, `SurfaceDestroyed` surface false, `VisibleTrue` visible true, `VisibleFalse` visible false, `ScreenOn` screen_on true, `ScreenOff` screen_on false. A repeat changes nothing.
4. `running` is recomputed as `should_render(*g)` on every valid event, even a repeat. A hand-built inconsistent `running` is therefore healed by any valid event.
5. The action is the edge from the `running` value given on entry: `Start` when it went false to true, `Stop` when true to false, otherwise `None`. When `action` is not null it is written on every success. A null `action` still applies the event and returns true.

Examples (from the header): running true with all facts true and a repeated `VisibleTrue` gives `None`; running false with all facts true and a repeated `VisibleTrue` gives `Start`. Running true with a fact false and any valid event gives `Stop`.

## to_string

Static strings, never null: `"SurfaceCreated"`, `"SurfaceDestroyed"`, `"VisibleTrue"`, `"VisibleFalse"`, `"ScreenOn"`, `"ScreenOff"` and `"None"`, `"Start"`, `"Stop"`. Any other value gives lowercase `"unknown"`.

## Bounds

| Item | Bound |
| --- | --- |
| facts and running | 4 bools, 16 combinations |
| valid events | 6 (values 0 to 5) |
| invalid events | 6 and above, tested with 6, 7 and 255 |
| actions | 3 (values 0 to 2); 3 is tested as invalid in `to_string` |

## Tests

`tools/render_gate_ref.py` is an independent reference in Python (a dict of facts, the event table, an edge by comparing was and now). It prints a generated block with FNV-1a 64 digests (return value, action and the whole object after each call) that the test recomputes.

```
python3 tools/render_gate_ref.py                                          # print the block
python3 tools/render_gate_ref.py --check tests/render_gate/render_gate_tests.cpp
```

`tests/render_gate/render_gate_tests.cpp` has 11 test cases and 161 assertions:

- written cases: defaults and enum values, the `should_render` truth table (16 combos, running ignored), each event on its fact, a 12-step lifecycle with every Start, Stop and None, the hand-built healing examples, null action, null gate and invalid events (6, 7, 255) with `*action` pre-loaded and checked untouched, every `to_string` value and the unknown cases;
- exhaustive: all 16 combos x 6 events against an in-test naive oracle (an array of three ints and an edge table that shares no code with the implementation), each also run with a null action;
- digests: the grid of 16 combos x (6 events + 3 invalid) = 144 cases, the `should_render` table, and 4 scripted sequences of 600 events (2,400 cases, about one in 17 an invalid event) checked against both the oracle and the digest.

## Mutation check

57 hand-written source mutants of `render_gate.cpp` (each term of `should_render`, the null check, the value, fact and fallthrough of every event case, the invalid-event path, the order of reading `running`, the recompute, each part of the Start and Stop condition, the action null check and write, every string and the unknown values) were compiled against the unchanged tests. 55 were killed by a plain build and 2 survived. Three kills are crashes only (the null `g` check removed, the null `action` check removed or inverted).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented, the test object unchanged), the same 55 are killed and the same 2 survive; no mutant was hidden from the plain build. The sanitizers explain the crash-only kills with observed messages:

- null `g` check removed: `member access within null pointer of type 'struct RenderGate'`.
- null `action` check removed or inverted: `store to null pointer of type 'GateAction'`.

The unmutated source passes all tests under the same flags.

Survivors in both builds, each argued:

- `running` recomputed as `if (running != should_render && running) running = false; else running = should_render`: if `running` is true and `should_render` is false it sets false, which is `should_render`; in every other case it sets `should_render`. The result is always `should_render`. Equivalent.
- `*action` set to `None` before the event is applied: the later write on the same success path overwrites it, and a failed call returns before that line. Equivalent.

Not run: Windows (CI only), Android, the editor, `tools/ci/test.py`, fuzzing, a full local build.
