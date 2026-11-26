# Microphone capture state

`core/lifecycle/mic_state.h` and `mic_state.cpp` (`lumora::lifecycle`) are the microphone capture state machine of "Microphone capture states" in `docs/android-lifecycle.md` section 10.7 (it supports section 9.3 item 5). It is a pure transition table: no audio calls, no service calls, no clock, no persistence. The platform layer feeds events and starts or stops the foreground service and the `AudioRecord` from the state flags.

Evidence label: unit tests only (CI-verified).

Capability gating (declared and granted) lives in `core/package/capabilities.h`, not here. The table can return `Active` while a separate gate refuses capture, for example after a `UserActivate` that follows a revoked permission. Only the gate decides whether capture really starts.

## States, events, flags

| State | Foreground service (`service_running`) | `AudioRecord` (`capturing`) |
| --- | --- | --- |
| `Inactive` | no | no |
| `Active` | yes | yes |
| `Paused` | yes | no |
| `Stopped` | no | no |

Events: `UserActivate` (the user turns capture on in the visible Lumora app), `Hidden` (wallpaper hidden, screen off, or the lock screen covers it), `Visible`, `ServiceKilled`, `CaptureRefused`, `PermissionRevoked`, `UserStop`. Capturing implies the service is running.

## Transition table

Rows are the current state, columns the event. A dash leaves the state unchanged.

| | UserActivate | Hidden | Visible | ServiceKilled | CaptureRefused | PermissionRevoked | UserStop |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Inactive | Active | - | - | - | - | Stopped | - |
| Active | - | Paused | - | Stopped | Stopped | Stopped | Stopped |
| Paused | - | - | Active | Stopped | Stopped | Stopped | Stopped |
| Stopped | Active | - | - | - | - | - | - |

Only `UserActivate` leaves `Stopped`. There is no automatic restart: `Visible`, or any other event, never restarts a stopped capture.

Differences from the plan's table: the plan lists "permission revoked" among the causes of `Stopped` without a source state, and says `Inactive` is left by user activation only. The header sends `Inactive` + `PermissionRevoked` to `Stopped`, which is slightly broader than the plan's wording. This code follows the header. Every other row matches the plan's "Entered by" and "Left by" columns, and the resume from `Paused` is marked in the plan as depending on the platform allowing it (unverified there).

## Out-of-range values

- An event outside the enum leaves a valid state unchanged.
- A state outside the enum is treated as `Stopped` (fail safe: not capturing), and then the event is applied to `Stopped`. An invalid state with `UserActivate` gives `Active`, with any other event, valid or not, gives `Stopped`.
- `service_running` and `capturing` give false for a state outside the enum.
- `to_string` gives "Inactive", "Active", "Paused", "Stopped" and "UserActivate", "Hidden", "Visible", "ServiceKilled", "CaptureRefused", "PermissionRevoked", "UserStop", and lowercase "unknown" (never null) for any other value.

## Tests

`tools/mic_state_ref.py` is an independent reference in Python. Its table is written first from the plan's table (the "Entered by" and "Left by" columns), then compared in the script with the header's transition list; the only difference is the `Inactive` + `PermissionRevoked` pair described above. It prints a generated block with the full table, flags, names, bad enum values and FNV-1a 64 digests, which the test recomputes.

```
python3 tools/mic_state_ref.py                                       # print the block
python3 tools/mic_state_ref.py --check tests/mic_state/mic_state_tests.cpp
```

`tests/mic_state/mic_state_tests.cpp` compares exact values only:

- all 28 state and event pairs written out by hand, compared again with the generated table;
- no restart out of `Stopped` for any event but `UserActivate`, including hide and show cycles;
- flags for every state and for invalid values, names for every value, and "unknown" for invalid ones;
- out-of-range states and events (4, 5, 127, 128, 255 for states, 7, 8, 127, 128, 255 for events), every state and event pair including them (9 x 12 = 108 cases, digest);
- every sequence of 4 events from every state (4 x 7^4 = 9,604 sequences, digest of each intermediate state).

## Mutation check

59 hand-written source mutants of `mic_state.cpp` (every transition changed or removed, extra transitions added including automatic restarts out of `Stopped`, the invalid-state rule, each flag, every name and the "unknown" text) were each compiled against the unchanged tests. 56 were killed and 3 survived.

Survivors, each argued:

- `valid_state` limited to `<= Paused`, and to `< Stopped`: `Stopped` is then treated as invalid, which maps to `Stopped` anyway. Same result for every input. Equivalent.
- the final `return` after the switch changed to `Active`: every valid state returns inside the switch, and the state was already replaced by `Stopped` if invalid, so the line cannot be reached. It exists only to satisfy the compilers' "control reaches end" warning. Unreachable.

Not run: Windows (CI only), Android, Vulkan/GLES, the editor, `tools/ci/test.py`, fuzzing, a full local build.
