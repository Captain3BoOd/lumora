// Tests for core/decfmt. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/decfmt_ref.py, an independent Python implementation on Python integers.
// Regenerate it with: python3 tools/decfmt_ref.py
// The tests also carry a naive oracle that shares no code with core/decfmt/decfmt.cpp: format by repeated subtraction of powers of ten (no
// division, no remainder), parse by a hand-written form check and a decimal-string comparison against the limit (length first, then
// lexicographic) before any value is computed.
// Parse inputs are heap buffers of exactly the stated length (AddressSanitizer sees any read past the end). Format output buffers are canary
// filled: on success only dst[0 .. length) may change (nothing at or above the length, inside or past the capacity), on BufferTooSmall the whole
// buffer must be unchanged. *out_len after a format failure is unspecified and never asserted. A parse failure leaves *out untouched.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "decfmt.h"

using namespace lumora::decfmt;

namespace {

// BEGIN GENERATED REFERENCE (tools/decfmt_ref.py)
const std::uint8_t kRefAlpha[] = {0x30, 0x31, 0x35, 0x39, 0x2D, 0x2B, 0x20, 0x78, 0x00, 0x80, 0xFF, 0x2E};
const std::uint32_t kRefSmallMax = 4, kRefRandSeeds = 400;
const char* const kRefBound[] = {"0", "1", "9223372036854775807", "9223372036854775808", "9223372036854775809", "18446744073709551615", "18446744073709551616", "18446744073709551617", "9999999999999999999", "10000000000000000000", "99999999999999999999", "100000000000000000000", "184467440737095516150", "922337203685477580", "1844674407370955161"};
const char* const kRefPrefix[] = {"", "-", "+", "--", "x"};
const std::uint32_t kRefZeros[] = {0, 1, 5, 30};
const std::uint64_t kRefU[] = {0ull, 1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 7ull, 8ull, 9ull, 10ull, 11ull, 15ull, 16ull, 17ull, 20ull, 26ull, 30ull, 31ull, 32ull, 33ull, 39ull, 45ull, 55ull, 58ull, 63ull, 64ull, 65ull, 76ull, 99ull, 100ull, 101ull, 107ull, 123ull, 127ull, 128ull, 129ull, 225ull, 243ull, 255ull, 256ull, 257ull, 269ull, 494ull, 511ull, 512ull, 513ull, 668ull, 755ull, 999ull, 1000ull, 1001ull, 1023ull, 1024ull, 1025ull, 1560ull, 1866ull, 2047ull, 2048ull, 2049ull, 2372ull, 3702ull, 4095ull, 4096ull, 4097ull, 6789ull, 8036ull, 8191ull, 8192ull, 8193ull, 9999ull, 10000ull, 10001ull, 10675ull, 16383ull, 16384ull, 16385ull, 32767ull, 32768ull, 32769ull, 65535ull, 65536ull, 65537ull, 90814ull, 99999ull, 100000ull, 100001ull, 131071ull, 131072ull, 131073ull, 254681ull, 262143ull, 262144ull, 262145ull, 345784ull, 524287ull, 524288ull, 524289ull, 677979ull, 680108ull, 705889ull, 795177ull, 876334ull, 957082ull, 999999ull, 1000000ull, 1000001ull, 1048575ull, 1048576ull, 1048577ull, 1909292ull, 2097151ull, 2097152ull, 2097153ull, 2315703ull, 4073387ull, 4194303ull, 4194304ull, 4194305ull, 6527704ull, 8388607ull, 8388608ull, 8388609ull, 8949285ull, 9999999ull, 10000000ull, 10000001ull, 11631440ull, 12118192ull, 16777215ull, 16777216ull, 16777217ull, 18384880ull, 19778220ull, 28886111ull, 33554431ull, 33554432ull, 33554433ull, 41065426ull, 67108863ull, 67108864ull, 67108865ull, 99999999ull, 100000000ull, 100000001ull, 101022213ull, 106551217ull, 134217727ull, 134217728ull, 134217729ull, 136706469ull, 179682383ull, 268435455ull, 268435456ull, 268435457ull, 444846856ull, 536870911ull, 536870912ull, 536870913ull, 629257819ull, 704212523ull, 853561159ull, 999999999ull, 1000000000ull, 1000000001ull, 1008497597ull, 1020010672ull, 1028092053ull, 1073741823ull, 1073741824ull, 1073741825ull, 2029194859ull, 2147483647ull, 2147483648ull, 2147483649ull, 2949325402ull, 3232270532ull, 4294967295ull, 4294967296ull, 4294967297ull, 4624586371ull, 4702251075ull, 5716379746ull, 8589934591ull, 8589934592ull, 8589934593ull, 9999999999ull, 10000000000ull, 10000000001ull, 12008359672ull, 12471110457ull, 13121172750ull, 13906238743ull, 15537283459ull, 17179869183ull, 17179869184ull, 17179869185ull, 22467406891ull, 27172232885ull, 34359738367ull, 34359738368ull, 34359738369ull, 43351543280ull, 68719476735ull, 68719476736ull, 68719476737ull, 99999999999ull, 100000000000ull, 100000000001ull, 137438953471ull, 137438953472ull, 137438953473ull, 274877906943ull, 274877906944ull, 274877906945ull, 335220765046ull, 549755813887ull, 549755813888ull, 549755813889ull, 790042794234ull, 999999999999ull, 1000000000000ull, 1000000000001ull, 1099511627775ull, 1099511627776ull, 1099511627777ull, 1132112657734ull, 2083298667225ull, 2199023255551ull, 2199023255552ull, 2199023255553ull, 3889189620979ull, 4156634118049ull, 4356390762332ull, 4398046511103ull, 4398046511104ull, 4398046511105ull, 4556621682129ull, 5428149944681ull, 6615866008670ull, 8796093022207ull, 8796093022208ull, 8796093022209ull, 9835229374941ull, 9999999999999ull, 10000000000000ull, 10000000000001ull, 17592186044415ull, 17592186044416ull, 17592186044417ull, 21416866234149ull, 35184372088831ull, 35184372088832ull, 35184372088833ull, 44849449409636ull, 64478606831192ull, 70368744177663ull, 70368744177664ull, 70368744177665ull, 89354237001790ull, 99999999999999ull, 100000000000000ull, 100000000000001ull, 101718719623806ull, 132518669770444ull, 140737488355327ull, 140737488355328ull, 140737488355329ull, 143534759973630ull, 281474976710655ull, 281474976710656ull, 281474976710657ull, 281761447116983ull, 562949953421311ull, 562949953421312ull, 562949953421313ull, 660705598046009ull, 999999999999999ull, 1000000000000000ull, 1000000000000001ull, 1125899906842623ull, 1125899906842624ull, 1125899906842625ull, 1668968361920945ull, 2251799813685247ull, 2251799813685248ull, 2251799813685249ull, 3355528857481331ull, 3867717684212565ull, 4128230007397863ull, 4503599627370495ull, 4503599627370496ull, 4503599627370497ull, 5978079434702266ull, 8596313097792575ull, 9007199254740991ull, 9007199254740992ull, 9007199254740993ull, 9999999999999999ull, 10000000000000000ull, 10000000000000001ull, 18014398509481983ull, 18014398509481984ull, 18014398509481985ull, 29419150626568242ull, 31590404164799168ull, 36016478366373943ull, 36028797018963967ull, 36028797018963968ull, 36028797018963969ull, 50965549353054358ull, 63167765671805523ull, 72057594037927935ull, 72057594037927936ull, 72057594037927937ull, 99999999999999999ull, 100000000000000000ull, 100000000000000001ull, 144115188075855871ull, 144115188075855872ull, 144115188075855873ull, 214275434338485938ull, 258050487832289047ull, 278023732877246075ull, 288230376151711743ull, 288230376151711744ull, 288230376151711745ull, 488309057521033750ull, 576460752303423487ull, 576460752303423488ull, 576460752303423489ull, 845533432841752020ull, 992744263031444050ull, 999999999999999999ull, 1000000000000000000ull, 1000000000000000001ull, 1115292974145323820ull, 1152921504606846975ull, 1152921504606846976ull, 1152921504606846977ull, 1575591485216640384ull, 2280697032119885105ull, 2305843009213693951ull, 2305843009213693952ull, 2305843009213693953ull, 2926118320135880396ull, 4457071746250023739ull, 4611686018427387903ull, 4611686018427387904ull, 4611686018427387905ull, 7460853024784007123ull, 9049276705396298333ull, 9223372036854775807ull, 9223372036854775808ull, 9223372036854775809ull, 9999999999999999999ull, 10000000000000000000ull, 10000000000000000001ull, 11984698055240501893ull, 18446744073709551614ull, 18446744073709551615ull};
const std::int64_t kRefI[] = {(-9223372036854775807ll - 1), -9223372036854775807ll, -9049276705396298333ll, -7460853024784007123ll, -4611686018427387905ll, -4611686018427387904ll, -4611686018427387903ll, -4457071746250023739ll, -2926118320135880396ll, -2305843009213693953ll, -2305843009213693952ll, -2305843009213693951ll, -2280697032119885105ll, -1575591485216640384ll, -1152921504606846977ll, -1152921504606846976ll, -1152921504606846975ll, -1115292974145323820ll, -1000000000000000001ll, -1000000000000000000ll, -999999999999999999ll, -992744263031444050ll, -845533432841752020ll, -576460752303423489ll, -576460752303423488ll, -576460752303423487ll, -488309057521033750ll, -288230376151711745ll, -288230376151711744ll, -288230376151711743ll, -278023732877246075ll, -258050487832289047ll, -214275434338485938ll, -144115188075855873ll, -144115188075855872ll, -144115188075855871ll, -100000000000000001ll, -100000000000000000ll, -99999999999999999ll, -72057594037927937ll, -72057594037927936ll, -72057594037927935ll, -63167765671805523ll, -50965549353054358ll, -36028797018963969ll, -36028797018963968ll, -36028797018963967ll, -36016478366373943ll, -31590404164799168ll, -29419150626568242ll, -18014398509481985ll, -18014398509481984ll, -18014398509481983ll, -10000000000000001ll, -10000000000000000ll, -9999999999999999ll, -9007199254740993ll, -9007199254740992ll, -9007199254740991ll, -8596313097792575ll, -5978079434702266ll, -4503599627370497ll, -4503599627370496ll, -4503599627370495ll, -4128230007397863ll, -3867717684212565ll, -3355528857481331ll, -2251799813685249ll, -2251799813685248ll, -2251799813685247ll, -1668968361920945ll, -1125899906842625ll, -1125899906842624ll, -1125899906842623ll, -1000000000000001ll, -1000000000000000ll, -999999999999999ll, -660705598046009ll, -562949953421313ll, -562949953421312ll, -562949953421311ll, -281761447116983ll, -281474976710657ll, -281474976710656ll, -281474976710655ll, -143534759973630ll, -140737488355329ll, -140737488355328ll, -140737488355327ll, -132518669770444ll, -101718719623806ll, -100000000000001ll, -100000000000000ll, -99999999999999ll, -89354237001790ll, -70368744177665ll, -70368744177664ll, -70368744177663ll, -64478606831192ll, -44849449409636ll, -35184372088833ll, -35184372088832ll, -35184372088831ll, -21416866234149ll, -17592186044417ll, -17592186044416ll, -17592186044415ll, -10000000000001ll, -10000000000000ll, -9999999999999ll, -9835229374941ll, -8796093022209ll, -8796093022208ll, -8796093022207ll, -6615866008670ll, -5428149944681ll, -4556621682129ll, -4398046511105ll, -4398046511104ll, -4398046511103ll, -4356390762332ll, -4156634118049ll, -3889189620979ll, -2199023255553ll, -2199023255552ll, -2199023255551ll, -2083298667225ll, -1132112657734ll, -1099511627777ll, -1099511627776ll, -1099511627775ll, -1000000000001ll, -1000000000000ll, -999999999999ll, -790042794234ll, -549755813889ll, -549755813888ll, -549755813887ll, -335220765046ll, -274877906945ll, -274877906944ll, -274877906943ll, -137438953473ll, -137438953472ll, -137438953471ll, -100000000001ll, -100000000000ll, -99999999999ll, -68719476737ll, -68719476736ll, -68719476735ll, -43351543280ll, -34359738369ll, -34359738368ll, -34359738367ll, -27172232885ll, -22467406891ll, -17179869185ll, -17179869184ll, -17179869183ll, -15537283459ll, -13906238743ll, -13121172750ll, -12471110457ll, -12008359672ll, -10000000001ll, -10000000000ll, -9999999999ll, -8589934593ll, -8589934592ll, -8589934591ll, -5716379746ll, -4702251075ll, -4624586371ll, -4294967297ll, -4294967296ll, -4294967295ll, -3232270532ll, -2949325402ll, -2147483649ll, -2147483648ll, -2147483647ll, -2029194859ll, -1073741825ll, -1073741824ll, -1073741823ll, -1028092053ll, -1020010672ll, -1008497597ll, -1000000001ll, -1000000000ll, -999999999ll, -853561159ll, -704212523ll, -629257819ll, -536870913ll, -536870912ll, -536870911ll, -444846856ll, -268435457ll, -268435456ll, -268435455ll, -179682383ll, -136706469ll, -134217729ll, -134217728ll, -134217727ll, -106551217ll, -101022213ll, -100000001ll, -100000000ll, -99999999ll, -67108865ll, -67108864ll, -67108863ll, -41065426ll, -33554433ll, -33554432ll, -33554431ll, -28886111ll, -19778220ll, -18384880ll, -16777217ll, -16777216ll, -16777215ll, -12118192ll, -11631440ll, -10000001ll, -10000000ll, -9999999ll, -8949285ll, -8388609ll, -8388608ll, -8388607ll, -6527704ll, -4194305ll, -4194304ll, -4194303ll, -4073387ll, -2315703ll, -2097153ll, -2097152ll, -2097151ll, -1909292ll, -1048577ll, -1048576ll, -1048575ll, -1000001ll, -1000000ll, -999999ll, -957082ll, -876334ll, -795177ll, -705889ll, -680108ll, -677979ll, -524289ll, -524288ll, -524287ll, -345784ll, -262145ll, -262144ll, -262143ll, -254681ll, -131073ll, -131072ll, -131071ll, -100001ll, -100000ll, -99999ll, -90814ll, -65537ll, -65536ll, -65535ll, -32769ll, -32768ll, -32767ll, -16385ll, -16384ll, -16383ll, -10675ll, -10001ll, -10000ll, -9999ll, -8193ll, -8192ll, -8191ll, -8036ll, -6789ll, -4097ll, -4096ll, -4095ll, -3702ll, -2372ll, -2049ll, -2048ll, -2047ll, -1866ll, -1560ll, -1025ll, -1024ll, -1023ll, -1001ll, -1000ll, -999ll, -755ll, -668ll, -513ll, -512ll, -511ll, -494ll, -269ll, -257ll, -256ll, -255ll, -243ll, -225ll, -129ll, -128ll, -127ll, -123ll, -107ll, -101ll, -100ll, -99ll, -76ll, -65ll, -64ll, -63ll, -58ll, -55ll, -45ll, -39ll, -33ll, -32ll, -31ll, -30ll, -26ll, -20ll, -17ll, -16ll, -15ll, -11ll, -10ll, -9ll, -8ll, -7ll, -6ll, -5ll, -4ll, -3ll, -2ll, -1ll, 0ll, 1ll, 2ll, 3ll, 4ll, 5ll, 6ll, 7ll, 8ll, 9ll, 10ll, 11ll, 15ll, 16ll, 17ll, 20ll, 26ll, 30ll, 31ll, 32ll, 33ll, 39ll, 45ll, 55ll, 58ll, 63ll, 64ll, 65ll, 76ll, 99ll, 100ll, 101ll, 107ll, 123ll, 127ll, 128ll, 129ll, 225ll, 243ll, 255ll, 256ll, 257ll, 269ll, 494ll, 511ll, 512ll, 513ll, 668ll, 755ll, 999ll, 1000ll, 1001ll, 1023ll, 1024ll, 1025ll, 1560ll, 1866ll, 2047ll, 2048ll, 2049ll, 2372ll, 3702ll, 4095ll, 4096ll, 4097ll, 6789ll, 8036ll, 8191ll, 8192ll, 8193ll, 9999ll, 10000ll, 10001ll, 10675ll, 16383ll, 16384ll, 16385ll, 32767ll, 32768ll, 32769ll, 65535ll, 65536ll, 65537ll, 90814ll, 99999ll, 100000ll, 100001ll, 131071ll, 131072ll, 131073ll, 254681ll, 262143ll, 262144ll, 262145ll, 345784ll, 524287ll, 524288ll, 524289ll, 677979ll, 680108ll, 705889ll, 795177ll, 876334ll, 957082ll, 999999ll, 1000000ll, 1000001ll, 1048575ll, 1048576ll, 1048577ll, 1909292ll, 2097151ll, 2097152ll, 2097153ll, 2315703ll, 4073387ll, 4194303ll, 4194304ll, 4194305ll, 6527704ll, 8388607ll, 8388608ll, 8388609ll, 8949285ll, 9999999ll, 10000000ll, 10000001ll, 11631440ll, 12118192ll, 16777215ll, 16777216ll, 16777217ll, 18384880ll, 19778220ll, 28886111ll, 33554431ll, 33554432ll, 33554433ll, 41065426ll, 67108863ll, 67108864ll, 67108865ll, 99999999ll, 100000000ll, 100000001ll, 101022213ll, 106551217ll, 134217727ll, 134217728ll, 134217729ll, 136706469ll, 179682383ll, 268435455ll, 268435456ll, 268435457ll, 444846856ll, 536870911ll, 536870912ll, 536870913ll, 629257819ll, 704212523ll, 853561159ll, 999999999ll, 1000000000ll, 1000000001ll, 1008497597ll, 1020010672ll, 1028092053ll, 1073741823ll, 1073741824ll, 1073741825ll, 2029194859ll, 2147483647ll, 2147483648ll, 2147483649ll, 2949325402ll, 3232270532ll, 4294967295ll, 4294967296ll, 4294967297ll, 4624586371ll, 4702251075ll, 5716379746ll, 8589934591ll, 8589934592ll, 8589934593ll, 9999999999ll, 10000000000ll, 10000000001ll, 12008359672ll, 12471110457ll, 13121172750ll, 13906238743ll, 15537283459ll, 17179869183ll, 17179869184ll, 17179869185ll, 22467406891ll, 27172232885ll, 34359738367ll, 34359738368ll, 34359738369ll, 43351543280ll, 68719476735ll, 68719476736ll, 68719476737ll, 99999999999ll, 100000000000ll, 100000000001ll, 137438953471ll, 137438953472ll, 137438953473ll, 274877906943ll, 274877906944ll, 274877906945ll, 335220765046ll, 549755813887ll, 549755813888ll, 549755813889ll, 790042794234ll, 999999999999ll, 1000000000000ll, 1000000000001ll, 1099511627775ll, 1099511627776ll, 1099511627777ll, 1132112657734ll, 2083298667225ll, 2199023255551ll, 2199023255552ll, 2199023255553ll, 3889189620979ll, 4156634118049ll, 4356390762332ll, 4398046511103ll, 4398046511104ll, 4398046511105ll, 4556621682129ll, 5428149944681ll, 6615866008670ll, 8796093022207ll, 8796093022208ll, 8796093022209ll, 9835229374941ll, 9999999999999ll, 10000000000000ll, 10000000000001ll, 17592186044415ll, 17592186044416ll, 17592186044417ll, 21416866234149ll, 35184372088831ll, 35184372088832ll, 35184372088833ll, 44849449409636ll, 64478606831192ll, 70368744177663ll, 70368744177664ll, 70368744177665ll, 89354237001790ll, 99999999999999ll, 100000000000000ll, 100000000000001ll, 101718719623806ll, 132518669770444ll, 140737488355327ll, 140737488355328ll, 140737488355329ll, 143534759973630ll, 281474976710655ll, 281474976710656ll, 281474976710657ll, 281761447116983ll, 562949953421311ll, 562949953421312ll, 562949953421313ll, 660705598046009ll, 999999999999999ll, 1000000000000000ll, 1000000000000001ll, 1125899906842623ll, 1125899906842624ll, 1125899906842625ll, 1668968361920945ll, 2251799813685247ll, 2251799813685248ll, 2251799813685249ll, 3355528857481331ll, 3867717684212565ll, 4128230007397863ll, 4503599627370495ll, 4503599627370496ll, 4503599627370497ll, 5978079434702266ll, 8596313097792575ll, 9007199254740991ll, 9007199254740992ll, 9007199254740993ll, 9999999999999999ll, 10000000000000000ll, 10000000000000001ll, 18014398509481983ll, 18014398509481984ll, 18014398509481985ll, 29419150626568242ll, 31590404164799168ll, 36016478366373943ll, 36028797018963967ll, 36028797018963968ll, 36028797018963969ll, 50965549353054358ll, 63167765671805523ll, 72057594037927935ll, 72057594037927936ll, 72057594037927937ll, 99999999999999999ll, 100000000000000000ll, 100000000000000001ll, 144115188075855871ll, 144115188075855872ll, 144115188075855873ll, 214275434338485938ll, 258050487832289047ll, 278023732877246075ll, 288230376151711743ll, 288230376151711744ll, 288230376151711745ll, 488309057521033750ll, 576460752303423487ll, 576460752303423488ll, 576460752303423489ll, 845533432841752020ll, 992744263031444050ll, 999999999999999999ll, 1000000000000000000ll, 1000000000000000001ll, 1115292974145323820ll, 1152921504606846975ll, 1152921504606846976ll, 1152921504606846977ll, 1575591485216640384ll, 2280697032119885105ll, 2305843009213693951ll, 2305843009213693952ll, 2305843009213693953ll, 2926118320135880396ll, 4457071746250023739ll, 4611686018427387903ll, 4611686018427387904ll, 4611686018427387905ll, 7460853024784007123ll, 9049276705396298333ll, 9223372036854775807ll};
const std::uint64_t kRefDigest_fmt = 0xC89C935B8C595DFBull;
const std::size_t kRefCases_fmt = 1067, kRefOps_fmt = 13393;
const std::uint64_t kRefDigest_small = 0x32ACC7443309C176ull;
const std::size_t kRefCases_small = 22621, kRefOps_small = 45242;
const std::uint64_t kRefDigest_bound = 0x71E5B5C49C631F35ull;
const std::size_t kRefCases_bound = 1200, kRefOps_bound = 2400;
const std::uint64_t kRefDigest_rand = 0x6938066C88CA48E9ull;
const std::size_t kRefCases_rand = 400, kRefOps_rand = 800;
// END GENERATED REFERENCE

constexpr std::uint32_t kSentinel = 0xDEADBEEFu;
constexpr std::uint64_t kSentinel64 = 0xDEADBEEFCAFEF00Dull;
constexpr std::int64_t kSentinelI = 0x1EADBEEFCAFEF00DLL;
constexpr std::uint32_t kPad = 3;

std::uint32_t code(DecResult r) { return static_cast<std::uint32_t>(r); }

struct Fold {
  std::uint64_t h = 0xCBF29CE484222325ull;
  std::size_t cases = 0;
  std::size_t ops = 0;
  void add(std::uint64_t v) { h = (h ^ v) * 0x100000001B3ull; }
};

// ---- oracle ----
std::string o_digits(std::uint64_t v) {
  static const std::uint64_t kPow[] = {10000000000000000000ull, 1000000000000000000ull, 100000000000000000ull, 10000000000000000ull,
                                       1000000000000000ull,     100000000000000ull,     10000000000000ull,     1000000000000ull,
                                       100000000000ull,         10000000000ull,         1000000000ull,         100000000ull,
                                       10000000ull,             1000000ull,             100000ull,             10000ull,
                                       1000ull,                 100ull,                 10ull,                 1ull};
  std::string s;
  for (std::uint64_t p : kPow) {
    char d = '0';
    while (v >= p) {
      v -= p;
      ++d;
    }
    if (!s.empty() || d != '0') s.push_back(d);
  }
  if (s.empty()) s = "0";
  return s;
}

std::string o_text_u(std::uint64_t v) { return o_digits(v); }

std::string o_text_i(std::int64_t v) {
  if (v >= 0) return o_digits(static_cast<std::uint64_t>(v));
  // |v| = (-(v + 1)) + 1, without overflow
  const std::uint64_t m = static_cast<std::uint64_t>(-(v + 1));
  return "-" + o_digits(m + 1u);
}

struct Parsed {
  DecResult r;
  std::uint64_t mag;
  bool neg;
};

// Form and range by string comparison. `sign` allows one leading '-'.
Parsed o_parse(const std::string& s, bool sign) {
  Parsed p{DecResult::Ok, 0, false};
  if (s.size() > kMaxParseLen) {
    p.r = DecResult::TooLarge;
    return p;
  }
  std::size_t i = 0;
  if (sign && !s.empty() && s[0] == '-') {
    p.neg = true;
    i = 1;
  }
  if (i == s.size()) {
    p.r = DecResult::Invalid;
    return p;
  }
  for (std::size_t j = i; j < s.size(); ++j) {
    if (s[j] < '0' || s[j] > '9') {
      p.r = DecResult::Invalid;
      return p;
    }
  }
  std::size_t z = i;
  while (z + 1 < s.size() && s[z] == '0') ++z;
  const std::string d = s.substr(z);
  const std::string lim = !sign ? "18446744073709551615" : (p.neg ? "9223372036854775808" : "9223372036854775807");
  if (d.size() > lim.size() || (d.size() == lim.size() && d > lim)) {
    p.r = DecResult::Overflow;
    return p;
  }
  for (char c : d) p.mag = p.mag * 10u + static_cast<std::uint64_t>(c - '0');
  return p;
}

// ---- buffers ----
struct In {
  std::unique_ptr<char[]> p;
  std::uint32_t n;
  explicit In(const std::string& s) : p(new char[s.empty() ? 1 : s.size()]), n(static_cast<std::uint32_t>(s.size())) {
    for (std::size_t i = 0; i < s.size(); ++i) p[i] = s[i];
  }
};

struct Out {
  std::unique_ptr<char[]> p;
  std::uint32_t cap;
  std::vector<char> snap;
  Out(std::uint32_t c, std::uint32_t pad) : p(new char[static_cast<std::size_t>(c) + pad + 1u]), cap(c) {
    for (std::uint32_t i = 0; i < c + pad + 1u; ++i) p[i] = static_cast<char>(0x40 + static_cast<int>(i % 7u));
    snap.assign(p.get(), p.get() + c + pad + 1u);
    total = c + pad + 1u;
  }
  std::uint32_t total = 0;
  bool unchanged() const {
    for (std::uint32_t i = 0; i < total; ++i) {
      if (p[i] != snap[i]) return false;
    }
    return true;
  }
  // true when every byte at index >= from is unchanged
  bool unchanged_from(std::uint32_t from) const {
    for (std::uint32_t i = from; i < total; ++i) {
      if (p[i] != snap[i]) return false;
    }
    return true;
  }
};

void do_fmt(Fold* f, std::size_t* bad, bool is_signed, std::uint64_t uv, std::int64_t iv, std::uint32_t cap) {
  const std::string want = is_signed ? o_text_i(iv) : o_text_u(uv);
  Out dst(cap, kPad);
  std::uint32_t len = kSentinel;
  const DecResult r = is_signed ? format_i64(iv, dst.p.get(), cap, &len) : format_u64(uv, dst.p.get(), cap, &len);
  if (cap < want.size()) {
    if (r != DecResult::BufferTooSmall || !dst.unchanged()) ++*bad;
  } else {
    if (r != DecResult::Ok || len != want.size()) {
      ++*bad;
    } else {
      for (std::size_t i = 0; i < want.size(); ++i) {
        if (dst.p[i] != want[i]) ++*bad;
      }
      if (!dst.unchanged_from(static_cast<std::uint32_t>(want.size()))) ++*bad;
    }
  }
  if (f != nullptr) {
    ++f->ops;
    if (r == DecResult::Ok) {
      f->add((static_cast<std::uint64_t>(code(r)) << 32) | len);
      for (std::size_t i = 0; i < want.size(); ++i) f->add(static_cast<std::uint8_t>(dst.p[i]));
    } else {
      f->add(static_cast<std::uint64_t>(code(r)) << 32);
    }
  }
}

void do_parse(Fold* f, std::size_t* bad, const std::string& s) {
  const In in(s);
  const Parsed pu = o_parse(s, false);
  const Parsed pi = o_parse(s, true);
  std::uint64_t ou = kSentinel64;
  std::int64_t oi = kSentinelI;
  const DecResult ru = parse_u64(in.p.get(), in.n, &ou);
  const DecResult ri = parse_i64(in.p.get(), in.n, &oi);
  if (ru != pu.r) ++*bad;
  if (ri != pi.r) ++*bad;
  if (ru == DecResult::Ok) {
    if (pu.r != DecResult::Ok || ou != pu.mag) ++*bad;
  } else if (ou != kSentinel64) {
    ++*bad;
  }
  std::int64_t want_i = 0;
  if (pi.r == DecResult::Ok) want_i = pi.neg ? static_cast<std::int64_t>(0u - pi.mag) : static_cast<std::int64_t>(pi.mag);
  if (ri == DecResult::Ok) {
    if (pi.r != DecResult::Ok || oi != want_i) ++*bad;
  } else if (oi != kSentinelI) {
    ++*bad;
  }
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (in.p[i] != s[i]) ++*bad;
  }
  if (f != nullptr) {
    f->ops += 2;
    f->add(static_cast<std::uint64_t>(code(ru)) << 32);
    if (ru == DecResult::Ok) f->add(ou);
    f->add(static_cast<std::uint64_t>(code(ri)) << 32);
    if (ri == DecResult::Ok) f->add(static_cast<std::uint64_t>(oi));
    ++f->cases;
  }
}

struct Lcg {
  std::uint64_t x;
  explicit Lcg(std::uint64_t seed) : x(seed) {}
  std::uint32_t next() {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<std::uint32_t>(x >> 32);
  }
};

}  // namespace

TEST_CASE("header anchors", "[decfmt]") {
  CHECK(kMaxParseLen == (1u << 20));
  CHECK(code(DecResult::Ok) == 0u);
  CHECK(code(DecResult::NullArgument) == 1u);
  CHECK(code(DecResult::TooLarge) == 2u);
  CHECK(code(DecResult::BufferTooSmall) == 3u);
  CHECK(code(DecResult::Invalid) == 4u);
  CHECK(code(DecResult::Overflow) == 5u);
}

TEST_CASE("format: boundary and pseudo-random values x every capacity from 0 to length + 1", "[sweep][decfmt]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint64_t v : kRefU) {
    const std::uint32_t len = static_cast<std::uint32_t>(o_text_u(v).size());
    for (std::uint32_t cap = 0; cap <= len + 1u; ++cap) do_fmt(&f, &bad, false, v, 0, cap);
    ++f.cases;
  }
  for (std::int64_t v : kRefI) {
    const std::uint32_t len = static_cast<std::uint32_t>(o_text_i(v).size());
    for (std::uint32_t cap = 0; cap <= len + 1u; ++cap) do_fmt(&f, &bad, true, 0, v, cap);
    ++f.cases;
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_fmt);
  CHECK(f.ops == kRefOps_fmt);
  CHECK(f.h == kRefDigest_fmt);
}

TEST_CASE("format then parse gives the value back", "[sweep][decfmt]") {
  std::size_t bad = 0;
  for (std::uint64_t v : kRefU) {
    char buf[24];
    std::uint32_t len = 0;
    std::uint64_t back = kSentinel64;
    if (format_u64(v, buf, sizeof buf, &len) != DecResult::Ok) ++bad;
    if (parse_u64(buf, len, &back) != DecResult::Ok || back != v) ++bad;
  }
  for (std::int64_t v : kRefI) {
    char buf[24];
    std::uint32_t len = 0;
    std::int64_t back = kSentinelI;
    if (format_i64(v, buf, sizeof buf, &len) != DecResult::Ok) ++bad;
    if (parse_i64(buf, len, &back) != DecResult::Ok || back != v) ++bad;
  }
  CHECK(bad == 0u);
}

TEST_CASE("parse: every string of length 0 to 4 over 12 symbols", "[sweep][decfmt]") {
  Fold f;
  std::size_t bad = 0;
  std::vector<std::string> cur(1, std::string());
  do_parse(&f, &bad, std::string());
  for (std::uint32_t len = 1; len <= kRefSmallMax; ++len) {
    std::vector<std::string> nxt;
    for (const std::string& p : cur) {
      for (std::uint8_t c : kRefAlpha) nxt.push_back(p + std::string(1, static_cast<char>(c)));
    }
    for (const std::string& s : nxt) do_parse(&f, &bad, s);
    cur.swap(nxt);
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_small);
  CHECK(f.ops == kRefOps_small);
  CHECK(f.h == kRefDigest_small);
}

TEST_CASE("parse: strings around 2^63, 2^64 and 10^19 with prefixes, leading zeros and bad bytes", "[sweep][decfmt]") {
  Fold f;
  std::size_t bad = 0;
  for (const char* base : kRefBound) {
    const std::string s(base);
    for (const char* prefix : kRefPrefix) {
      for (std::uint32_t z : kRefZeros) {
        const std::string p(prefix);
        const std::string body = p + std::string(z, '0') + s;
        do_parse(&f, &bad, body);
        do_parse(&f, &bad, body + "x");
        const std::size_t mid = p.size() + z + s.size() / 2;
        do_parse(&f, &bad, body.substr(0, mid) + "x" + body.substr(mid));
        do_parse(&f, &bad, body + " ");
      }
    }
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_bound);
  CHECK(f.ops == kRefOps_bound);
  CHECK(f.h == kRefDigest_bound);
}

TEST_CASE("parse: pseudo-random strings of 1 to 30 symbols (not exhaustive)", "[sweep][decfmt]") {
  Fold f;
  std::size_t bad = 0;
  for (std::uint32_t seed = 1; seed <= kRefRandSeeds; ++seed) {
    Lcg g(seed);
    const bool neg = g.next() % 4u == 0u;
    const std::uint32_t n = 1u + g.next() % 30u;
    std::string s = neg ? "-" : "";
    for (std::uint32_t i = 0; i < n; ++i) {
      const std::uint32_t r = g.next();
      s.push_back(r % 16u != 0u ? static_cast<char>('0' + static_cast<int>(r % 10u)) : static_cast<char>(kRefAlpha[(r >> 4) % 12u]));
    }
    do_parse(&f, &bad, s);
  }
  CHECK(bad == 0u);
  CHECK(f.cases == kRefCases_rand);
  CHECK(f.ops == kRefOps_rand);
  CHECK(f.h == kRefDigest_rand);
}

namespace {

DecResult pu(const std::string& s, std::uint64_t* out) {
  const In in(s);
  return parse_u64(in.p.get(), in.n, out);
}
DecResult pi(const std::string& s, std::int64_t* out) {
  const In in(s);
  return parse_i64(in.p.get(), in.n, out);
}

}  // namespace

TEST_CASE("format: hand-checked texts", "[decfmt]") {
  char buf[32];
  std::uint32_t len = kSentinel;
  CHECK(format_u64(0u, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(len == 1u);
  CHECK(buf[0] == '0');
  CHECK(format_u64(18446744073709551615ull, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "18446744073709551615");
  CHECK(len == 20u);
  CHECK(format_i64(-9223372036854775807LL - 1, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "-9223372036854775808");
  CHECK(len == 20u);
  CHECK(format_i64(9223372036854775807LL, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "9223372036854775807");
  CHECK(format_i64(-1, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "-1");
  CHECK(format_i64(0, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "0");
  CHECK(format_i64(-10, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "-10");
  CHECK(format_u64(1000u, buf, sizeof buf, &len) == DecResult::Ok);
  CHECK(std::string(buf, len) == "1000");
}

TEST_CASE("format: null rules, capacity 0 and a large capacity", "[args][decfmt]") {
  char buf[40];
  for (char& c : buf) c = '#';
  std::uint32_t len = kSentinel;
  // capacity 0 is always BufferTooSmall (the text is at least 1 byte); dst may be null there
  CHECK(format_u64(0u, nullptr, 0, &len) == DecResult::BufferTooSmall);
  CHECK(format_u64(123u, nullptr, 0, &len) == DecResult::BufferTooSmall);
  CHECK(format_i64(-5, nullptr, 0, &len) == DecResult::BufferTooSmall);
  CHECK(format_u64(0u, buf, 0, &len) == DecResult::BufferTooSmall);
  // a null dst with a positive capacity is a NullArgument; so is a null out_len, even when the capacity is too small
  CHECK(format_u64(7u, nullptr, 1, &len) == DecResult::NullArgument);
  CHECK(format_i64(7, nullptr, 1, &len) == DecResult::NullArgument);
  CHECK(format_u64(7u, nullptr, 0xFFFFFFFFu, &len) == DecResult::NullArgument);
  CHECK(format_u64(7u, buf, 8, nullptr) == DecResult::NullArgument);
  CHECK(format_i64(7, buf, 8, nullptr) == DecResult::NullArgument);
  CHECK(format_u64(12345u, buf, 0, nullptr) == DecResult::NullArgument);
  CHECK(format_u64(12345u, nullptr, 0, nullptr) == DecResult::NullArgument);
  CHECK(format_i64(-12345, nullptr, 0, nullptr) == DecResult::NullArgument);
  for (char c : buf) CHECK(c == '#');
  // a large capacity is not an error and the bytes after the text stay unchanged
  CHECK(format_u64(42u, buf, 0xFFFFFFFFu, &len) == DecResult::Ok);
  CHECK(len == 2u);
  CHECK(buf[0] == '4');
  CHECK(buf[1] == '2');
  for (std::size_t i = 2; i < sizeof buf; ++i) CHECK(buf[i] == '#');
  // nothing is written past the capacity: exact capacity, then one short
  char two[3] = {'#', '#', '#'};
  CHECK(format_u64(99u, two, 2, &len) == DecResult::Ok);
  CHECK(two[2] == '#');
  CHECK(format_u64(100u, two, 2, &len) == DecResult::BufferTooSmall);
  CHECK(two[0] == '9');
  CHECK(two[1] == '9');
  CHECK(two[2] == '#');
}

TEST_CASE("parse: hand-checked values", "[decfmt]") {
  std::uint64_t u = kSentinel64;
  std::int64_t i = kSentinelI;
  CHECK(pu("007", &u) == DecResult::Ok);
  CHECK(u == 7u);
  CHECK(pu(std::string(30, '0') + "5", &u) == DecResult::Ok);
  CHECK(u == 5u);
  CHECK(pu("0", &u) == DecResult::Ok);
  CHECK(u == 0u);
  CHECK(pu("18446744073709551615", &u) == DecResult::Ok);
  CHECK(u == 18446744073709551615ull);
  CHECK(pu("00018446744073709551615", &u) == DecResult::Ok);
  CHECK(u == 18446744073709551615ull);
  CHECK(pi("-0", &i) == DecResult::Ok);
  CHECK(i == 0);
  CHECK(pi("-9223372036854775808", &i) == DecResult::Ok);
  CHECK(i == -9223372036854775807LL - 1);
  CHECK(pi("9223372036854775807", &i) == DecResult::Ok);
  CHECK(i == 9223372036854775807LL);
  CHECK(pi("-000000000000000000000009223372036854775808", &i) == DecResult::Ok);
  CHECK(i == -9223372036854775807LL - 1);
  CHECK(pi("-12", &i) == DecResult::Ok);
  CHECK(i == -12);
  // out of range, only when the form is valid
  u = kSentinel64;
  i = kSentinelI;
  CHECK(pu("18446744073709551616", &u) == DecResult::Overflow);
  CHECK(pu("99999999999999999999999999999999", &u) == DecResult::Overflow);
  CHECK(pi("9223372036854775808", &i) == DecResult::Overflow);
  CHECK(pi("-9223372036854775809", &i) == DecResult::Overflow);
  CHECK(pi("18446744073709551615", &i) == DecResult::Overflow);
  // bad form
  CHECK(pu("", &u) == DecResult::Invalid);
  CHECK(pi("", &i) == DecResult::Invalid);
  CHECK(pu("-0", &u) == DecResult::Invalid);
  CHECK(pu("-1", &u) == DecResult::Invalid);
  CHECK(pu("+5", &u) == DecResult::Invalid);
  CHECK(pi("+5", &i) == DecResult::Invalid);
  CHECK(pi("-", &i) == DecResult::Invalid);
  CHECK(pi("--1", &i) == DecResult::Invalid);
  CHECK(pi("-+1", &i) == DecResult::Invalid);
  CHECK(pi("1-", &i) == DecResult::Invalid);
  CHECK(pu(" 1", &u) == DecResult::Invalid);
  CHECK(pu("1 ", &u) == DecResult::Invalid);
  CHECK(pu("1_0", &u) == DecResult::Invalid);
  CHECK(pu("1.0", &u) == DecResult::Invalid);
  CHECK(pu("0x10", &u) == DecResult::Invalid);
  CHECK(pu(std::string("1\0", 2), &u) == DecResult::Invalid);
  CHECK(pu(std::string("\0", 1), &u) == DecResult::Invalid);
  CHECK(pu(std::string("1\x80", 2), &u) == DecResult::Invalid);
  CHECK(pu(std::string("\xB1", 1), &u) == DecResult::Invalid);  // 0xB1 is not '1' (0x31) with the high bit set
  CHECK(pu("/", &u) == DecResult::Invalid);                    // 0x2F, just below '0'
  CHECK(pu(":", &u) == DecResult::Invalid);                    // 0x3A, just above '9'
  // Invalid wins over Overflow wherever the bad byte is
  CHECK(pu("18446744073709551616x", &u) == DecResult::Invalid);
  CHECK(pu("x18446744073709551616", &u) == DecResult::Invalid);
  CHECK(pu("184467440737095516x6", &u) == DecResult::Invalid);
  CHECK(pi("-9223372036854775809x", &i) == DecResult::Invalid);
  CHECK(pi("-x9223372036854775809", &i) == DecResult::Invalid);
  CHECK(pi("99999999999999999999+", &i) == DecResult::Invalid);
  CHECK(pi("+99999999999999999999", &i) == DecResult::Invalid);
  CHECK(u == kSentinel64);
  CHECK(i == kSentinelI);
}

TEST_CASE("parse: null rules and check order", "[args][decfmt]") {
  std::uint64_t u = kSentinel64;
  std::int64_t i = kSentinelI;
  const char one[1] = {'7'};
  const char bad[1] = {'x'};
  // src counts only while n > 0
  CHECK(parse_u64(nullptr, 1, &u) == DecResult::NullArgument);
  CHECK(parse_i64(nullptr, 1, &i) == DecResult::NullArgument);
  CHECK(parse_u64(nullptr, kMaxParseLen, &u) == DecResult::NullArgument);
  CHECK(parse_u64(nullptr, kMaxParseLen + 1u, &u) == DecResult::NullArgument);  // Null beats TooLarge
  CHECK(parse_i64(nullptr, kMaxParseLen + 1u, &i) == DecResult::NullArgument);
  CHECK(parse_u64(nullptr, 0, &u) == DecResult::Invalid);  // empty input, not a null argument
  CHECK(parse_i64(nullptr, 0, &i) == DecResult::Invalid);
  // out always counts
  CHECK(parse_u64(one, 1, nullptr) == DecResult::NullArgument);
  CHECK(parse_i64(one, 1, nullptr) == DecResult::NullArgument);
  CHECK(parse_u64(nullptr, 0, nullptr) == DecResult::NullArgument);
  CHECK(parse_i64(nullptr, 0, nullptr) == DecResult::NullArgument);
  CHECK(parse_u64(bad, kMaxParseLen + 1u, nullptr) == DecResult::NullArgument);
  CHECK(parse_i64(bad, kMaxParseLen + 1u, nullptr) == DecResult::NullArgument);
  // TooLarge beats Invalid and Overflow; the claimed length is never read (a 1-byte buffer)
  CHECK(parse_u64(bad, kMaxParseLen + 1u, &u) == DecResult::TooLarge);
  CHECK(parse_i64(bad, kMaxParseLen + 1u, &i) == DecResult::TooLarge);
  CHECK(parse_u64(one, kMaxParseLen + 1u, &u) == DecResult::TooLarge);
  CHECK(parse_i64(one, 0xFFFFFFFFu, &i) == DecResult::TooLarge);
  CHECK(u == kSentinel64);
  CHECK(i == kSentinelI);
}

TEST_CASE("parse: one real 1 MiB buffer at exactly kMaxParseLen", "[max][decfmt]") {
  std::vector<char> big(kMaxParseLen, '0');
  std::uint64_t u = kSentinel64;
  std::int64_t i = kSentinelI;
  const std::uint32_t n = kMaxParseLen;
  // all zeros
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Ok);
  CHECK(u == 0u);
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Ok);
  CHECK(i == 0);
  // zeros and a final digit
  big[n - 1u] = '7';
  u = kSentinel64;
  i = kSentinelI;
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Ok);
  CHECK(u == 7u);
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Ok);
  CHECK(i == 7);
  // a '-' and zeros with a final digit (n - 1 digits after the sign)
  big[0] = '-';
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Ok);
  CHECK(i == -7);
  u = kSentinel64;
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Invalid);
  CHECK(u == kSentinel64);
  // a lone sign is not possible here; a '-' followed by all zeros
  big[n - 1u] = '0';
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Ok);
  CHECK(i == 0);
  // a bad byte at the end
  big[0] = '0';
  big[n - 1u] = 'x';
  u = kSentinel64;
  i = kSentinelI;
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Invalid);
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Invalid);
  CHECK(u == kSentinel64);
  CHECK(i == kSentinelI);
  // a huge value: Overflow, and Invalid still wins when a bad byte is at the end
  big[0] = '1';
  big[n - 1u] = '0';
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Overflow);
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Overflow);
  big[n - 1u] = 'x';
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Invalid);
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Invalid);
  CHECK(u == kSentinel64);
  CHECK(i == kSentinelI);
  // the largest u64 behind about a million zeros
  const std::string max = "18446744073709551615";
  std::fill(big.begin(), big.end(), '0');
  for (std::size_t k = 0; k < max.size(); ++k) big[n - max.size() + k] = max[k];
  CHECK(parse_u64(big.data(), n, &u) == DecResult::Ok);
  CHECK(u == 18446744073709551615ull);
  CHECK(parse_i64(big.data(), n, &i) == DecResult::Overflow);
  CHECK(i == kSentinelI);
}
