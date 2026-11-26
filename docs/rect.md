# Rect

`core/rect/rect.h` and `rect.cpp` (`lumora::rect`) give integer rectangles for damage regions and clip boxes: validity, intersection, bounding union, a point test and the area.

Evidence label: unit tests only (CI-verified).

Integer only: no floating point, no libm, no 128-bit types (MSVC has none), no wide character types, no allocation, no global state. The source includes `<cstdint>`; the tests add `<catch2/...>`, `<cstddef>` and `<vector>`. All edges are computed in `std::int64_t`, so no sum can overflow.

## State

None. `Rect { int32 x, y, w, h }` covers `x <= px < x + w` and `y <= py < y + h`. A default `Rect` is all zeros (valid and empty).

## Validity and emptiness

`is_valid(r)` is `w >= 0 && h >= 0 && x + w <= INT32_MAX && y + h <= INT32_MAX` with the sums in 64 bits. So a right or bottom edge of exactly `INT32_MAX` is valid and one above is invalid; a zero width or height is valid; a negative width or height is invalid; negative `x` and `y` are allowed. `is_empty(r)` is `!is_valid(r) || w == 0 || h == 0`. Every other function treats an invalid rectangle exactly like an empty one.

## intersect and overlaps

`intersect(a, b)`, in this order:

1. `a` or `b` empty (including invalid): all zeros, even when the other is valid.
2. `left = max(a.x, b.x)`, `top = max(a.y, b.y)`, `right = min` of the right edges, `bottom = min` of the bottom edges.
3. `right <= left` or `bottom <= top` (no shared pixel, touching edges included): all zeros.
4. Otherwise `{left, top, right - left, bottom - top}`. The width and height are at least 1 and fit in `int32`.

`overlaps(a, b)` is `!is_empty(intersect(a, b))`.

## bounds_union

`bounds_union(a, b, out)`, in this order:

1. `out == nullptr`: return false first, before looking at `a` or `b`, even when both are empty.
2. Both empty: the result is all zeros, return true (whatever coordinates the inputs have).
3. One empty: the result is the other rectangle exactly as given (it is valid and non-empty, so no size check applies).
4. Otherwise `left` and `top` are the smaller left and top edges, `right` and `bottom` the larger right and bottom edges, all in 64 bits. If `right - left` or `bottom - top` is greater than `INT32_MAX`, return false. Equal to `INT32_MAX` is accepted. So far apart valid rectangles can fail although each edge fits in `int32`. When the sizes pass, the result `{left, top, right - left, bottom - top}` is valid, because every right and bottom edge is at most `INT32_MAX`.

On every false return `*out` is unchanged. On success all four fields are computed first and `*out` is assigned once at the end, so `out` may be `&a` or `&b` (both are tested).

## contains_point

False for an empty or invalid rectangle. Otherwise `x <= px`, `px < x + w`, `y <= py` and `py < y + h`, with the right and bottom edges computed in 64 bits. A valid rectangle never contains a point with `px == INT32_MAX` or `py == INT32_MAX`, since its right and bottom edges are at most `INT32_MAX`.

## area

`w * h` as `uint64_t`, computed from the unsigned 64-bit product; 0 for an empty or invalid rectangle. The maximum is `(2^31 - 1)^2 = 4611686014132420609`.

## Bounds

| Item | Bound |
| --- | --- |
| `x`, `y`, `px`, `py` | any `int32_t` |
| `w`, `h` | any `int32_t`; a negative value is invalid |
| right and bottom edge of a valid rectangle | at most `INT32_MAX` |
| union width and height | at most `INT32_MAX` (equal accepted) |
| `area` | at most `(2^31 - 1)^2` |
| aliasing of `out` with `a` or `b` in `bounds_union` | supported and tested |

## Tests

`tools/rect_ref.py` is an independent reference in Python. It first checks the rules against a pixel-set model (a rectangle is the set of points it covers; the intersection is the common set; the union is the bounding box of the covered set) on small coordinates, then applies explicit edge arithmetic to boundary and random rectangles. It prints a generated block with the boundary table, the point list and FNV-1a 64 digests that the tests recompute.

```
python3 tools/rect_ref.py                                          # print the block
python3 tools/rect_ref.py --check tests/rect/rect_tests.cpp
```

`tests/rect/rect_tests.cpp` has 16 test cases and 261 assertions. Written cases: defaults, validity edges (right and bottom edge at `INT32_MAX` and one above, zero and negative sizes, `INT32_MIN` coordinates and sizes), emptiness, known intersections (contained, one shared pixel, touching edges in x and y, empty and invalid inputs, large coordinates), `overlaps`, known unions (empty and invalid inputs ignored, both empty with nonzero coordinates, a valid single rectangle returned unchanged), the width and height limit at exactly `INT32_MAX` and `INT32_MAX + 1` for both axes, far apart rectangles, a null `out` for every combination of empty inputs, `*out` unchanged on every false return, `out` aliasing `a` and `b` (also with an empty input, with `a` and `b` the same object, and on a failure), `contains_point` at every edge and at the 32-bit limits, `area` at the 32-bit and 64-bit limits, and an invalid-acts-as-empty case for every function with six invalid shapes.

Sweeps use two naive oracles that share no code with the implementation: a pixel grid for small coordinates (6,000 random pairs, with the intersection, union, point test and area read from the grid) and an edge-by-edge `long long` oracle for large coordinates (30,000 random pairs from three generators, with the union failure and success counts checked to be non-trivial). Digests from the reference:

- intersect and union: 66,561 pairs (all 81 x 81 pairs of the boundary table, then 20,000 each of small, large random and near-`INT32_MAX` pairs), with the intersection, the overlap flag, the union result and the union return value;
- contains_point: 42,225 checks (81 boundary rectangles against 15 x 15 boundary points, then four points per random rectangle for 6,000 random rectangles);
- is_valid, is_empty and area: 6,081 rectangles.

## Mutation check

106 hand-written source mutants of `rect.cpp` (every comparison, edge and limit in `is_valid` and `is_empty`, `right_of` and `bottom_of`, the empty checks, the min and max choices, the edge comparisons and the output fields of `intersect`, the negation in `overlaps`, the null check and its position, each empty-input rule, the min and max choices, the size limits and the output fields of `bounds_union`, the write order for aliasing, every comparison of `contains_point`, and the empty rule and product of `area`) were compiled against the unchanged tests. 100 were killed by a plain build and 6 survived. Two kills (the null check of `bounds_union` removed, or moved after the empty cases) lost the plain report to a crash (SIGSEGV).

Rebuilt with `-fsanitize=address,undefined -fno-sanitize-recover=all` (only the mutated file instrumented), the same 100 are killed and the same 6 survive. Run directly under the sanitizers, the crash-only kills report:

- the null check removed, or moved after the empty cases (2): `store to null pointer of type 'struct Rect'`;
- `overlaps` written with plain 32-bit edge sums (killed by assertions in the plain build): `signed integer overflow: 2147483619 + 29 cannot be represented in type 'int'`.

The 6 survivors are each equivalent, with these arguments:

- `is_valid` with an extra condition that the 32-bit wrapped sum `x + w` is at least `x`: for a rectangle that passes the other conditions `w >= 0` and `x + w <= INT32_MAX`, so the sum does not wrap and is at least `x`. The extra condition is implied.
- `is_empty` with `w <= 0` instead of `w == 0`: the two differ only for a negative `w`, and that rectangle is invalid, so `!is_valid(r)` already returned true.
- `bounds_union` rejecting when the 32-bit wrapped `right - left` is negative instead of `> INT32_MAX`: this branch is reached only with both inputs non-empty, so `right > left` and `right - left` is in `[1, 2^32 - 1]`. For these values the wrapped `int32` is negative exactly when the value is above `INT32_MAX`.
- `bounds_union` assigning `out->x`, `out->y`, `out->w`, `out->h` one by one instead of `*out = result`: the fields come from the local `result`, which is complete before any write, so aliasing cannot change the values read.
- `contains_point` with the right edge computed in 32-bit arithmetic: the comparison is reached only for a non-empty rectangle, which is valid, so `x + w <= INT32_MAX` and the 32-bit sum does not wrap.
- `area` testing only `!is_valid(r)` instead of `is_empty(r)`: the two differ only for a valid rectangle with `w == 0` or `h == 0`, whose product is 0 anyway.

## Not run

- Windows (MSVC) is covered by CI only; it was not run locally.
- No test of threads. No test of an `out` that partly overlaps `a` or `b` (only full aliasing is promised and tested).
