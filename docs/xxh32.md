# xxHash32

`core/xxh32/xxh32.h` and `xxh32.cpp` (`lumora::xxh32`) are xxHash32, a 32-bit non-cryptographic hash for cache keys and content ids, as a one-shot `hash` and a stream (`reset`, `update`, `digest`). Not for security.

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation, no global state. The source includes `<cstdint>` only (words are assembled with explicit byte shifts, and no `memcpy` is used); the tests add `<catch2/...>`, `<cstddef>` and `<vector>`. All arithmetic is `std::uint32_t` modulo 2^32.

## Algorithm

As written in the header: `P1 = 2654435761`, `P2 = 2246822519`, `P3 = 3266489917`, `P4 = 668265263`, `P5 = 374761393`.

1. Total length at least 16: `v1 = seed + P1 + P2`, `v2 = seed + P2`, `v3 = seed`, `v4 = seed - P1`. Each 16-byte stripe is four little-endian words `w1..w4`, and `vi = rotl(vi + wi * P2, 13) * P1`. After the last whole stripe `h = rotl(v1,1) + rotl(v2,7) + rotl(v3,12) + rotl(v4,18)`. Below 16 bytes `h = seed + P5`.
2. `h += total length`. Then each whole 4-byte tail word gives `h = rotl(h + w * P3, 17) * P4`, and each remaining byte `b` gives `h = rotl(h + b * P5, 11) * P1`.
3. `h ^= h >> 15; h *= P2; h ^= h >> 13; h *= P3; h ^= h >> 16`.

Known values from the header: the empty input hashes to `0x02CC5D05` for seed 0 and `0x36B78AE7` for seed `P1`.

## State

`State { uint32 v[4]; uint32 total_len; uint32 seed; uint8 buf[16]; uint32 buf_len; }`. `buf[0 .. buf_len)` holds the pending bytes (`buf_len` is 0 to 15). A state is corrupt when `buf_len > 15`. No other field is checked.

## Check order

- `hash(data, len, seed, out)`: `out` null, or `data` null with `len > 0`: false, `*out` unchanged. Null `data` with `len == 0` hashes the empty input. Otherwise the lanes are set up, whole stripes are consumed from `data`, the rest (under 16 bytes) is the tail, and true.
- `reset(state, seed)`: a null state is ignored. Otherwise the four lanes are set as above, `total_len = 0`, `seed` is stored, `buf` is zero-filled and `buf_len = 0`, whatever the state held before (a corrupt state too).
- `update(state, data, len)`, in this order:
  1. null or corrupt state: false.
  2. `data` null with `len > 0`: false.
  3. `total_len + len > 0xFFFFFFFF` (64-bit sum): false. Exactly `0xFFFFFFFF` is accepted.
  4. `len == 0`: true, nothing changed (also with null `data`).
  5. `total_len += len`. If `buf_len > 0`, the buffer is topped up to 16 bytes from `data` and consumed as a stripe when it is full (`buf_len` back to 0). Then whole stripes are consumed directly from `data`, and the rest goes into `buf`. A stripe completed exactly at the end of a call is consumed in that call, so `buf_len` is 0 and never 16.
  On every false return the whole state is unchanged, buffer bytes included.
- `digest(state, out)`: null state, null `out` or a corrupt state: false, `*out` unchanged. Otherwise the hash of everything added so far, from the stored fields: the four-lane rule when `total_len >= 16`, `h += total_len`, and the tail is `buf[0 .. buf_len)`. The state is not changed, so the stream can continue. There is no check between `total_len` and `buf_len`; a hand-built state (for example `total_len` 5 with `buf_len` 9) is evaluated literally.

## Bounds

| Item | Bound |
| --- | --- |
| `seed` | any `uint32_t` |
| `len` of one call | any `uint32_t`, limited by the stream total |
| stream total | at most `0xFFFFFFFF` bytes (`kMaxStreamLength`), tested with a hand-built `total_len`, not by feeding 4 GiB |
| `buf_len` | 0 to 15; above 15 is corrupt |
| aliasing of `data` with the state's buffer | not supported, not tested |

## Tests

`tools/xxh32_ref.py` is an independent reference in Python. It was written from the algorithm text in the header only, with Python integers, and not from any xxHash source or package. A stream is modelled as the hash of all bytes added so far, so it shares no stripe or buffer logic with the C++. It prints a generated block with the known values, FNV-1a 64 digests that the tests recompute, and the hash of a 1 MiB input. It runs in under a second.

```
python3 tools/xxh32_ref.py                                           # print the block
python3 tools/xxh32_ref.py --check tests/xxh32/xxh32_tests.cpp
```

External checks: only the two empty-input values from the header (seed 0 and seed `P1`) were checked, and the reference, the C++ and the oracle all produce them. The official `xxhash` Python package is not installed in the environment that built this, and nothing was installed, so no other value was compared with an external implementation. Every other expected value comes from the reference and the oracle, which both follow the header text.

`tests/xxh32/xxh32_tests.cpp` has 11 test cases and 136 assertions (the sweeps count mismatches and assert once per sweep). Written cases: the two known values through `hash`, a null data pointer, a zero length and a stream; null arguments for every function with `*out` unchanged; `reset` over garbage (lanes, zero-filled `buf`, a corrupt `buf_len` repaired, seed `0xFFFFFFFF`); `update` with null and corrupt states (`buf_len` 16, 17, 255, `0xFFFFFFFF`) checked before `len == 0`, null data, `len == 0`; the buffer length after each call (a stripe completed exactly at the end of a call, 10 + 22, 10 + 25, one byte at a time); the stream limit with a hand-built `total_len` (exactly `0xFFFFFFFF` accepted, one more refused with the state unchanged, `0xFFFFFFF0 + 17` refused and not wrapped); `digest` with null arguments, repeated, without changing the state, and with the stream continuing.

Sweeps use a naive oracle that shares no code with the implementation (`std::uint64_t` arithmetic reduced with a remainder after every step, a plain byte vector, no stripe buffer):

- hash and stream: 5 seeds x 89 lengths (0 to 80, 95, 96, 97, 255, 256, 257, 1000, 4096) = 445 cases. Each is hashed in one shot, streamed whole, streamed byte by byte, and (for lengths up to 80) streamed with every single split point, all compared with the oracle and the reference digest;
- 2,000 random inputs (length 0 to 306, seeds including 0) streamed in random chunks of 1 to 40 bytes, compared with the one-shot hash and the reference digest;
- one 1 MiB input, one shot and in 4,097-byte chunks, compared with the reference value `0xA8DF4468`;
- forged states: 224 cases (7 `total_len` values from 0 to `0xFFFFFFFF`, 2 seeds, `buf_len` 0 to 15, lanes and buffer from a fixed pattern) with `digest` compared with the oracle and the reference digest, and the state checked unchanged.

## Mutation check

152 hand-written source mutants of `xxh32.cpp` (each constant, the rotate, the byte assembly, each lane start value, the stripe rounds, the lane merge and its threshold, the length addition, the tail word and byte loops and their bounds and constants, the final mix, the corrupt condition, every check in the order of `hash`, `reset`, `update` and `digest`, the 64-bit limit, the buffer top-up, the direct stripes and the tail copy) were compiled against the unchanged tests. 145 were killed by a plain build and 7 survived. Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), 146 are killed and 6 survive: mutant 106 (`reset` clears 17 bytes) survives the plain build and is killed by `index 16 out of bounds for type 'unsigned char [16]'`; it writes `buf[16]`, which in practice overlaps `buf_len` and is then set to 0 anyway, so the plain build cannot see it. It is undefined behavior, killed by the sanitizer only.

Kills by a crash alone were run individually. Plain builds, SIGSEGV (17): tail words `>= 3`, tail bytes `<=` and one short, the `hash` out check removed, data check removed, `len > 1` and `&&` variants, `reset`, `update` and `digest` null checks removed or weakened, the top-up without a length bound or with `<=`, the top-up consume `>`, and the tail copy from the start (one run of the last gave a 25 s timeout instead). Sanitizer builds report (31 mutants):

- `heap-buffer-overflow`: stripe step 15, tail words `>= 3`, the `hash` tail length `len`, the 32-bit overflow check and its removal, the top-up without a length bound, with `<=`, consuming `data`, and the tail loop with `<=`;
- `index 16` or `index 17 out of bounds for type 'unsigned char [16]'`: stripe 17, the corrupt check never true or removed, `reset` clearing 17 bytes, top-up never, `<=`, stopping at 15, consuming at `>`, `buf_len` not reset, and the tail copy from the start;
- `load of null pointer of type 'const uint8_t'`, `store to null pointer of type 'uint32_t'` and `member access within null pointer`: the removed or weakened null checks in `hash`, `reset`, `update` and `digest`.

6 of the 7 survivors of the plain build are equivalent on everything the header defines, with the arguments below. The 7th (`reset` clearing 17 bytes) is undefined behavior that the plain build cannot see; it is not equivalent and the sanitizer build kills it.

- `rotl` with `^` or `+` instead of `|` (2 mutants): `x << r` has its low `r` bits zero and `x >> (32 - r)` has only its low `r` bits set, for every `r` used (1, 7, 11, 12, 13, 17, 18, all between 1 and 31). The two values share no set bit, so or, xor and add give the same result.
- `read32` with `+` for the first `|`, or `^` for all of them (2 mutants): the four terms occupy the disjoint bit ranges 0 to 7, 8 to 15, 16 to 23 and 24 to 31, so or, add and xor are equal.
- `update` without the `len == 0` check: with `len == 0` and a valid state, `total_len += 0` changes nothing; the top-up loop does not run (`pos < len` is false) and `buf_len` stays below 16; the stripe loop and the tail loop do not run; the call returns true with nothing changed, as before.
- `update` with `buf_len >= 0` (always true for an unsigned value, so the top-up runs also when `buf_len == 0`): with `buf_len == 0` the top-up copies up to 16 bytes of `data` into `buf` and consumes them as a stripe when it holds 16, which is the same stripe the direct loop would consume; a shorter run stays in `buf[0 .. len)`, as the tail loop would put it. The lanes, `total_len`, `seed` and `buf_len` end up equal, and so do `buf[0 .. buf_len)`. The only difference is in `buf` at and above `buf_len` after a stripe was consumed (the mutant leaves that stripe's bytes there, the original leaves the old bytes). The header calls only `buf[0 .. buf_len)` used, every read of `buf` is below `buf_len`, and every write fills `buf` from index `buf_len` upward before it is read, so the result of every function is the same. The tests do not assert the unused bytes.

## Not run

- Windows (MSVC) is covered by CI only; it was not run locally.
- No test of threads, and no test of `data` pointing into the state's buffer.
- No external implementation was available, see Tests.
