# JSON structural limits

Implements the "explicit nesting, size, string-length and duplicate-key limits" rule of `docs/security-model.md` for untrusted JSON (`scene.json`, `settings.json`) as `scan_json` in `core/package/json_limits.h` (`lumora::package`). It is a pre-scan: one pass over the text, an explicit stack, no recursion, no document built. A real parser runs only on text that passed. The tests in `tests/json_limits_tests.cpp` are the reference when this page and the code differ. Evidence label: unit tests only (CI-verified, no real packages are read).

## What is checked

The scan returns a verdict and a byte offset (0 when `Ok`). The first problem found while reading left to right wins, except that size is checked first.

| Verdict | Rule | Offset |
|---|---|---|
| `TooLarge` | text longer than `max_bytes`; checked before scanning | `max_bytes` |
| `TooDeep` | an array or object would nest deeper than `max_depth`; the top level container is depth 1; `max_depth = 0` rejects every container | the opening bracket that exceeds it |
| `StringTooLong` | decoded length of one string or key exceeds `max_string_bytes`; an escape counts as the length of what it decodes to (a `\uXXXX` is 1 to 3 bytes, a surrogate pair is 4) | the first byte that takes the length over the limit; the backslash for an escape |
| `TooManyKeys` | more than `max_keys_per_object` members in one object | the opening quote of the excess key |
| `TooManyArrayItems` | more than `max_array_items` items in one array | the first byte of the excess item |
| `TooManyValues` | more than `max_total_values` values; every scalar, array and object counts as one, keys do not | the first byte of the excess value |
| `DuplicateKey` | the same decoded key twice in one object, so `"a"` and `"\u0061"` collide | the opening quote of the second key |
| `Malformed` | see below | the offending byte, or the end of the text when it ends too early |

For a value that breaks several limits at once, the order at the value's first byte is: array item count, total values, depth.

## Malformed

- empty or whitespace-only text; any text after the single top-level value (other than whitespace); a UTF-8 byte order mark at the start.
- unbalanced or mismatched brackets, unterminated strings, a missing or extra comma or colon, a trailing comma, a key that is not a string, an unknown bare word, literals other than exactly `true`, `false` and `null`.
- a raw control character (below 0x20) inside a string; a bad escape; a short or non-hex `\u` escape; a lone high or low surrogate escape.

Whitespace is space, tab, line feed and carriage return only. A top-level scalar is accepted; the scan does not require an object.

## What is not checked

- Numbers are loose: after a leading `-` or digit, any run of the characters `0-9 - + . e E` is accepted, so `01`, `1.` and `-` pass. A leading `+` is Malformed. The later parser owns number syntax.
- UTF-8 validity of bytes above 0x7F inside strings is not checked. The later parser owns it.
- `\u0000` is a valid escape and decodes to a NUL byte that counts as one byte. Consumers must handle embedded NUL in strings and keys.
- Duplicate keys compare decoded bytes exactly: case-sensitive, no Unicode normalization. A surrogate pair escape equals the raw UTF-8 character.

## Memory and time

- Key storage: each open object keeps a table of one 64-bit hash and one source offset and length per key. No key text is copied. A duplicate is confirmed by decoding both keys again from the text, only when hash and decoded length are both equal.
- The hash is seeded with a per-process random value so a hostile text cannot be built to collide offline.
- A table holds at most `max_keys_per_object` entries and is released when its object closes. The stack holds at most `max_depth` frames. Worst case at the defaults is about 32 open objects with 4096 keys each, roughly 130 thousand entries, a few megabytes; in general it is bounded by the smaller of `max_depth * max_keys_per_object` and `max_total_values`, at a few dozen bytes per entry.
- Re-decoding uses two scratch strings of at most `max_string_bytes` each.
- The scan is linear in the text size, plus the cost of confirming equal keys.
