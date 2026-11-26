// Tests for core/dither. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/dither_ref.py, an independent Python
// implementation (recursive Bayer doubling, and the identity 65535 = 255 * 257). Regenerate it with:
// python3 tools/dither_ref.py
// The tests also carry a naive oracle (the same recursive doubling and the same identity, written again in C++) that
// shares no code with core/dither/dither.cpp.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dither.h"

using namespace lumora::dither;

namespace {

// BEGIN GENERATED REFERENCE (tools/dither_ref.py)
const std::uint32_t kRefXs[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 100u, 255u, 256u, 2147483647u, 2147483648u, 4294967286u, 4294967287u, 4294967288u, 4294967294u, 4294967295u};
const std::uint32_t kRefYs[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 100u, 255u, 256u, 2147483647u, 2147483648u, 4294967286u, 4294967287u, 4294967288u, 4294967294u, 4294967295u};
const std::uint32_t kRefVs[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u, 41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u, 69u, 70u, 71u, 72u, 73u, 74u, 75u, 76u, 77u, 78u, 79u, 80u, 81u, 82u, 83u, 84u, 85u, 86u, 87u, 88u, 89u, 90u, 91u, 92u, 93u, 94u, 95u, 96u, 97u, 98u, 99u, 100u, 101u, 102u, 103u, 104u, 105u, 106u, 107u, 108u, 109u, 110u, 111u, 112u, 113u, 114u, 115u, 116u, 117u, 118u, 119u, 120u, 121u, 122u, 123u, 124u, 125u, 126u, 127u, 128u, 129u, 130u, 131u, 132u, 133u, 134u, 135u, 136u, 137u, 138u, 139u, 140u, 141u, 142u, 143u, 144u, 145u, 146u, 147u, 148u, 149u, 150u, 151u, 152u, 153u, 154u, 155u, 156u, 157u, 158u, 159u, 160u, 161u, 162u, 163u, 164u, 165u, 166u, 167u, 168u, 169u, 170u, 171u, 172u, 173u, 174u, 175u, 176u, 177u, 178u, 179u, 180u, 181u, 182u, 183u, 184u, 185u, 186u, 187u, 188u, 189u, 190u, 191u, 192u, 193u, 194u, 195u, 196u, 197u, 198u, 199u, 200u, 201u, 202u, 203u, 204u, 205u, 206u, 207u, 208u, 209u, 210u, 211u, 212u, 213u, 214u, 215u, 216u, 217u, 218u, 219u, 220u, 221u, 222u, 223u, 224u, 225u, 226u, 227u, 228u, 229u, 230u, 231u, 232u, 233u, 234u, 235u, 236u, 237u, 238u, 239u, 240u, 241u, 242u, 243u, 244u, 245u, 246u, 247u, 248u, 249u, 250u, 251u, 252u, 253u, 254u, 255u, 256u, 257u, 258u, 259u, 260u, 261u, 262u, 263u, 264u, 265u, 266u, 267u, 268u, 269u, 270u, 271u, 272u, 273u, 274u, 275u, 276u, 277u, 278u, 279u, 280u, 281u, 282u, 283u, 284u, 285u, 286u, 287u, 288u, 289u, 290u, 291u, 292u, 293u, 294u, 295u, 296u, 297u, 298u, 299u, 300u, 301u, 302u, 303u, 304u, 305u, 306u, 307u, 308u, 309u, 310u, 311u, 312u, 313u, 314u, 315u, 316u, 317u, 318u, 319u, 320u, 321u, 322u, 323u, 324u, 325u, 326u, 327u, 328u, 329u, 330u, 331u, 332u, 333u, 334u, 335u, 336u, 337u, 338u, 339u, 340u, 341u, 342u, 343u, 344u, 345u, 346u, 347u, 348u, 349u, 350u, 351u, 352u, 353u, 354u, 355u, 356u, 357u, 358u, 359u, 360u, 361u, 362u, 363u, 364u, 365u, 366u, 367u, 368u, 369u, 370u, 371u, 372u, 373u, 374u, 375u, 376u, 377u, 378u, 379u, 380u, 381u, 382u, 383u, 384u, 385u, 386u, 387u, 388u, 389u, 390u, 391u, 392u, 393u, 394u, 395u, 396u, 397u, 398u, 399u, 400u, 401u, 402u, 403u, 404u, 405u, 406u, 407u, 408u, 409u, 410u, 411u, 412u, 413u, 414u, 415u, 416u, 417u, 418u, 419u, 420u, 421u, 422u, 423u, 424u, 425u, 426u, 427u, 428u, 429u, 430u, 431u, 432u, 433u, 434u, 435u, 436u, 437u, 438u, 439u, 440u, 441u, 442u, 443u, 444u, 445u, 446u, 447u, 448u, 449u, 450u, 451u, 452u, 453u, 454u, 455u, 456u, 457u, 458u, 459u, 460u, 461u, 462u, 463u, 464u, 465u, 466u, 467u, 468u, 469u, 470u, 471u, 472u, 473u, 474u, 475u, 476u, 477u, 478u, 479u, 480u, 481u, 482u, 483u, 484u, 485u, 486u, 487u, 488u, 489u, 490u, 491u, 492u, 493u, 494u, 495u, 496u, 497u, 498u, 499u, 500u, 501u, 502u, 503u, 504u, 505u, 506u, 507u, 508u, 509u, 510u, 511u, 512u, 513u, 514u, 515u, 516u, 517u, 518u, 519u, 520u, 521u, 522u, 523u, 524u, 525u, 526u, 527u, 528u, 529u, 530u, 531u, 532u, 533u, 534u, 535u, 536u, 537u, 538u, 539u, 540u, 541u, 542u, 543u, 544u, 545u, 546u, 547u, 548u, 549u, 550u, 551u, 552u, 553u, 554u, 555u, 556u, 557u, 558u, 559u, 560u, 561u, 562u, 563u, 564u, 565u, 566u, 567u, 568u, 569u, 570u, 571u, 572u, 573u, 574u, 575u, 576u, 577u, 578u, 579u, 580u, 581u, 582u, 583u, 584u, 585u, 586u, 587u, 588u, 589u, 590u, 591u, 592u, 593u, 594u, 595u, 596u, 597u, 598u, 599u, 65000u, 65001u, 65002u, 65003u, 65004u, 65005u, 65006u, 65007u, 65008u, 65009u, 65010u, 65011u, 65012u, 65013u, 65014u, 65015u, 65016u, 65017u, 65018u, 65019u, 65020u, 65021u, 65022u, 65023u, 65024u, 65025u, 65026u, 65027u, 65028u, 65029u, 65030u, 65031u, 65032u, 65033u, 65034u, 65035u, 65036u, 65037u, 65038u, 65039u, 65040u, 65041u, 65042u, 65043u, 65044u, 65045u, 65046u, 65047u, 65048u, 65049u, 65050u, 65051u, 65052u, 65053u, 65054u, 65055u, 65056u, 65057u, 65058u, 65059u, 65060u, 65061u, 65062u, 65063u, 65064u, 65065u, 65066u, 65067u, 65068u, 65069u, 65070u, 65071u, 65072u, 65073u, 65074u, 65075u, 65076u, 65077u, 65078u, 65079u, 65080u, 65081u, 65082u, 65083u, 65084u, 65085u, 65086u, 65087u, 65088u, 65089u, 65090u, 65091u, 65092u, 65093u, 65094u, 65095u, 65096u, 65097u, 65098u, 65099u, 65100u, 65101u, 65102u, 65103u, 65104u, 65105u, 65106u, 65107u, 65108u, 65109u, 65110u, 65111u, 65112u, 65113u, 65114u, 65115u, 65116u, 65117u, 65118u, 65119u, 65120u, 65121u, 65122u, 65123u, 65124u, 65125u, 65126u, 65127u, 65128u, 65129u, 65130u, 65131u, 65132u, 65133u, 65134u, 65135u, 65136u, 65137u, 65138u, 65139u, 65140u, 65141u, 65142u, 65143u, 65144u, 65145u, 65146u, 65147u, 65148u, 65149u, 65150u, 65151u, 65152u, 65153u, 65154u, 65155u, 65156u, 65157u, 65158u, 65159u, 65160u, 65161u, 65162u, 65163u, 65164u, 65165u, 65166u, 65167u, 65168u, 65169u, 65170u, 65171u, 65172u, 65173u, 65174u, 65175u, 65176u, 65177u, 65178u, 65179u, 65180u, 65181u, 65182u, 65183u, 65184u, 65185u, 65186u, 65187u, 65188u, 65189u, 65190u, 65191u, 65192u, 65193u, 65194u, 65195u, 65196u, 65197u, 65198u, 65199u, 65200u, 65201u, 65202u, 65203u, 65204u, 65205u, 65206u, 65207u, 65208u, 65209u, 65210u, 65211u, 65212u, 65213u, 65214u, 65215u, 65216u, 65217u, 65218u, 65219u, 65220u, 65221u, 65222u, 65223u, 65224u, 65225u, 65226u, 65227u, 65228u, 65229u, 65230u, 65231u, 65232u, 65233u, 65234u, 65235u, 65236u, 65237u, 65238u, 65239u, 65240u, 65241u, 65242u, 65243u, 65244u, 65245u, 65246u, 65247u, 65248u, 65249u, 65250u, 65251u, 65252u, 65253u, 65254u, 65255u, 65256u, 65257u, 65258u, 65259u, 65260u, 65261u, 65262u, 65263u, 65264u, 65265u, 65266u, 65267u, 65268u, 65269u, 65270u, 65271u, 65272u, 65273u, 65274u, 65275u, 65276u, 65277u, 65278u, 65279u, 65280u, 65281u, 65282u, 65283u, 65284u, 65285u, 65286u, 65287u, 65288u, 65289u, 65290u, 65291u, 65292u, 65293u, 65294u, 65295u, 65296u, 65297u, 65298u, 65299u, 65300u, 65301u, 65302u, 65303u, 65304u, 65305u, 65306u, 65307u, 65308u, 65309u, 65310u, 65311u, 65312u, 65313u, 65314u, 65315u, 65316u, 65317u, 65318u, 65319u, 65320u, 65321u, 65322u, 65323u, 65324u, 65325u, 65326u, 65327u, 65328u, 65329u, 65330u, 65331u, 65332u, 65333u, 65334u, 65335u, 65336u, 65337u, 65338u, 65339u, 65340u, 65341u, 65342u, 65343u, 65344u, 65345u, 65346u, 65347u, 65348u, 65349u, 65350u, 65351u, 65352u, 65353u, 65354u, 65355u, 65356u, 65357u, 65358u, 65359u, 65360u, 65361u, 65362u, 65363u, 65364u, 65365u, 65366u, 65367u, 65368u, 65369u, 65370u, 65371u, 65372u, 65373u, 65374u, 65375u, 65376u, 65377u, 65378u, 65379u, 65380u, 65381u, 65382u, 65383u, 65384u, 65385u, 65386u, 65387u, 65388u, 65389u, 65390u, 65391u, 65392u, 65393u, 65394u, 65395u, 65396u, 65397u, 65398u, 65399u, 65400u, 65401u, 65402u, 65403u, 65404u, 65405u, 65406u, 65407u, 65408u, 65409u, 65410u, 65411u, 65412u, 65413u, 65414u, 65415u, 65416u, 65417u, 65418u, 65419u, 65420u, 65421u, 65422u, 65423u, 65424u, 65425u, 65426u, 65427u, 65428u, 65429u, 65430u, 65431u, 65432u, 65433u, 65434u, 65435u, 65436u, 65437u, 65438u, 65439u, 65440u, 65441u, 65442u, 65443u, 65444u, 65445u, 65446u, 65447u, 65448u, 65449u, 65450u, 65451u, 65452u, 65453u, 65454u, 65455u, 65456u, 65457u, 65458u, 65459u, 65460u, 65461u, 65462u, 65463u, 65464u, 65465u, 65466u, 65467u, 65468u, 65469u, 65470u, 65471u, 65472u, 65473u, 65474u, 65475u, 65476u, 65477u, 65478u, 65479u, 65480u, 65481u, 65482u, 65483u, 65484u, 65485u, 65486u, 65487u, 65488u, 65489u, 65490u, 65491u, 65492u, 65493u, 65494u, 65495u, 65496u, 65497u, 65498u, 65499u, 65500u, 65501u, 65502u, 65503u, 65504u, 65505u, 65506u, 65507u, 65508u, 65509u, 65510u, 65511u, 65512u, 65513u, 65514u, 65515u, 65516u, 65517u, 65518u, 65519u, 65520u, 65521u, 65522u, 65523u, 65524u, 65525u, 65526u, 65527u, 65528u, 65529u, 65530u, 65531u, 65532u, 65533u, 65534u, 65535u, 1000u, 4096u, 12345u, 21845u, 32767u, 32768u, 32769u, 43690u, 50000u, 257u, 258u, 514u, 65534u, 65535u};
const std::uint32_t kRefRowCounts[] = {1u, 2u, 7u, 8u, 9u, 63u, 64u, 65u, 255u, 1000u};
const std::uint32_t kRefRowX0[] = {0u, 1u, 7u, 8u, 100u, 4294966295u, 4294967287u, 4294967288u, 4294967292u, 4294967295u};
const std::uint32_t kRefRowY[] = {0u, 1u, 5u, 8u, 4294967295u};
const int kRefRowPatterns = 4;
const std::uint64_t kRefDigest_threshold = 0xEED278832079A4FFull;
const std::size_t kRefCases_threshold = 900;
const std::uint64_t kRefDigest_golden = 0x88BFCB32688B31A5ull;
const std::size_t kRefCases_golden = 4194304;
const std::uint64_t kRefDigest_extreme = 0x224A3FBCEDF80609ull;
const std::size_t kRefCases_extreme = 1035000;
const std::uint64_t kRefDigest_row = 0xB86A7004A49A983Bull;
const std::size_t kRefCases_row = 2000;
// END GENERATED REFERENCE

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
};

// Bayer matrix by recursive doubling: M(2n) = [[4M, 4M + 2], [4M + 3, 4M + 1]].
struct Bayer {
  int m[8][8];
  Bayer() {
    int cur[8][8] = {};
    int n = 1;
    cur[0][0] = 0;
    while (n < 8) {
      int nxt[8][8] = {};
      for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
          const int v = 4 * cur[y][x];
          nxt[y][x] = v;
          nxt[y][x + n] = v + 2;
          nxt[y + n][x] = v + 3;
          nxt[y + n][x + n] = v + 1;
        }
      }
      n *= 2;
      for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) cur[y][x] = nxt[y][x];
    }
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x) m[y][x] = cur[y][x];
  }
};

const Bayer& bayer() {
  static const Bayer b;
  return b;
}

int oracle_threshold(std::uint32_t x, std::uint32_t y) { return bayer().m[y & 7u][x & 7u]; }

// 65535 = 255 * 257, so floor(v * 255 / 65535) = v / 257 and the remainder is 255 * (v % 257).
int oracle_quantize(std::uint32_t v, std::uint32_t x, std::uint32_t y) {
  const std::uint64_t q = v / 257u;
  const std::uint64_t rem = 255u * (v % 257u);
  return static_cast<int>(rem * 128u >= (2u * static_cast<std::uint64_t>(oracle_threshold(x, y)) + 1u) * 65535u ? q + 1 : q);
}

std::vector<std::uint16_t> pattern(int kind, std::size_t n) {
  std::vector<std::uint16_t> out;
  std::uint64_t x = 12345u + static_cast<std::uint64_t>(kind);
  for (std::size_t i = 0; i < n; ++i) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    const std::uint64_t r = x >> 33;
    if (kind == 0) out.push_back(static_cast<std::uint16_t>((i * 257u) & 0xFFFFu));
    else if (kind == 1) out.push_back(static_cast<std::uint16_t>(r & 0xFFFFu));
    else if (kind == 2) out.push_back(65535);
    else out.push_back(static_cast<std::uint16_t>((i * 65u) & 0xFFFFu));
  }
  return out;
}

}  // namespace

TEST_CASE("constants", "[dither]") { CHECK(kMaxRow == 16777216u); }

TEST_CASE("threshold header check values and tile property", "[dither][threshold]") {
  const int row0[8] = {0, 32, 8, 40, 2, 34, 10, 42};
  const int row1[8] = {48, 16, 56, 24, 50, 18, 58, 26};
  for (std::uint32_t x = 0; x < 8; ++x) {
    CHECK(threshold(x, 0) == row0[x]);
    CHECK(threshold(x, 1) == row1[x]);
  }
  int seen[64] = {};
  for (std::uint32_t y = 0; y < 8; ++y) {
    for (std::uint32_t x = 0; x < 8; ++x) {
      const int t = threshold(x, y);
      REQUIRE(t >= 0);
      REQUIRE(t < 64);
      ++seen[t];
      CHECK(t == oracle_threshold(x, y));
    }
  }
  for (int i = 0; i < 64; ++i) CHECK(seen[i] == 1);
}

TEST_CASE("threshold repeats every 8 pixels including across uint32 wrap", "[dither][threshold]") {
  std::size_t bad = 0;
  for (std::uint32_t y : {0u, 3u, 7u, 0xFFFFFFF9u, UINT32_MAX}) {
    for (std::uint32_t x : {0u, 1u, 5u, 7u, 100u, 0x80000000u, 0xFFFFFFF8u, 0xFFFFFFFDu, UINT32_MAX}) {
      if (threshold(x, y) != oracle_threshold(x & 7u, y & 7u)) ++bad;
      if (threshold(x + 8u, y) != threshold(x, y)) ++bad;
      if (threshold(x, y + 8u) != threshold(x, y)) ++bad;
      if (threshold(x + 0x100u, y + 0x1000u) != threshold(x, y)) ++bad;
    }
  }
  CHECK(bad == 0);
  CHECK(threshold(UINT32_MAX, 0) == threshold(7, 0));
  CHECK(threshold(0, UINT32_MAX) == threshold(0, 7));
  CHECK(threshold(0xFFFFFFF8u, 0) == threshold(0, 0));
}

TEST_CASE("quantize_u16 endpoints and examples", "[dither][quantize]") {
  for (std::uint32_t y = 0; y < 8; ++y) {
    for (std::uint32_t x = 0; x < 8; ++x) {
      CHECK(quantize_u16(0, x, y) == 0);
      CHECK(quantize_u16(65535, x, y) == 255);
    }
  }
  // v = 257 is exactly 1 (rem 0): never rounded up.
  for (std::uint32_t y = 0; y < 8; ++y)
    for (std::uint32_t x = 0; x < 8; ++x) {
      CHECK(quantize_u16(257, x, y) == 1);
      CHECK(quantize_u16(514, x, y) == 2);
    }
  // v = 128: q = 0, rem = 255 * 128 = 32640; rem * 128 = 4177920; threshold test against (2t + 1) * 65535.
  // That is true for 2t + 1 <= 63 (t <= 31) and false above.
  for (std::uint32_t y = 0; y < 8; ++y)
    for (std::uint32_t x = 0; x < 8; ++x) CHECK(quantize_u16(128, x, y) == (threshold(x, y) <= 31 ? 1 : 0));
  // v = 1: rem = 255, rem * 128 = 32640 < 65535: always 0.
  for (std::uint32_t y = 0; y < 8; ++y)
    for (std::uint32_t x = 0; x < 8; ++x) CHECK(quantize_u16(1, x, y) == 0);
  // v = 256: q = 0, rem = 255 * 256 = 65280; rem * 128 / 65535 = 127: only t = 0..63 with 2t + 1 <= 127: all.
  for (std::uint32_t y = 0; y < 8; ++y)
    for (std::uint32_t x = 0; x < 8; ++x) CHECK(quantize_u16(256, x, y) == 1);
  // v = 65534: q = 254, rem = 255 * 256 = 65280, rem * 128 / 65535 = 127 (floor): above 2t + 1 = 127 for every t.
  for (std::uint32_t y = 0; y < 8; ++y)
    for (std::uint32_t x = 0; x < 8; ++x) CHECK(quantize_u16(65534, x, y) == 255);
}

TEST_CASE("quantize_u16 matches the oracle, bounds and monotonicity", "[dither][quantize][exhaustive]") {
  std::size_t bad = 0;
  for (std::uint32_t y = 0; y < 8; ++y) {
    for (std::uint32_t x = 0; x < 8; ++x) {
      int prev = 0;
      for (std::uint32_t v = 0; v <= 65535; ++v) {
        const int got = quantize_u16(static_cast<std::uint16_t>(v), x, y);
        if (got != oracle_quantize(v, x, y)) ++bad;
        if (got < prev || got > 255) ++bad;
        const int q = static_cast<int>(v / 257u);
        if (got != q && got != q + 1) ++bad;
        prev = got;
      }
    }
  }
  CHECK(bad == 0);
}

TEST_CASE("exact tile sum property for every v", "[dither][quantize][exhaustive]") {
  std::size_t bad = 0;
  for (std::uint32_t v = 0; v <= 65535; ++v) {
    const std::uint32_t scaled = v * 255u;
    const std::uint32_t q = scaled / 65535u;
    const std::uint32_t rem = scaled % 65535u;
    const std::uint32_t k = rem * 128u / 65535u;
    std::uint32_t sum = 0;
    for (std::uint32_t y = 0; y < 8; ++y)
      for (std::uint32_t x = 0; x < 8; ++x) sum += quantize_u16(static_cast<std::uint16_t>(v), x, y);
    if (sum != 64u * q + (k + 1u) / 2u) ++bad;
    // The same holds for a tile shifted by any aligned offset, wrapping at uint32.
    std::uint32_t sum2 = 0;
    for (std::uint32_t y = 0; y < 8; ++y)
      for (std::uint32_t x = 0; x < 8; ++x) sum2 += quantize_u16(static_cast<std::uint16_t>(v), 0xFFFFFFF8u + x, 16u + y);
    if (sum2 != sum) ++bad;
  }
  CHECK(bad == 0);
}

TEST_CASE("dither_row_u16 basics", "[dither][row]") {
  std::vector<std::uint16_t> src = pattern(1, 100);
  std::vector<std::uint8_t> dst(100, 0xEE);
  CHECK(dither_row_u16(src.data(), dst.data(), 100, 3, 5));
  std::size_t bad = 0;
  for (std::uint32_t i = 0; i < 100; ++i)
    if (dst[i] != oracle_quantize(src[i], 3u + i, 5u)) ++bad;
  CHECK(bad == 0);
  // x0 wraps modulo 2^32
  std::vector<std::uint8_t> dw(20, 0);
  CHECK(dither_row_u16(src.data(), dw.data(), 20, UINT32_MAX - 3u, UINT32_MAX));
  for (std::uint32_t i = 0; i < 20; ++i) CHECK(dw[i] == oracle_quantize(src[i], UINT32_MAX - 3u + i, UINT32_MAX));
  // count 1 writes exactly one byte
  std::vector<std::uint8_t> d1(4, 0x77);
  CHECK(dither_row_u16(src.data(), d1.data(), 1, 0, 0));
  CHECK(d1[0] == oracle_quantize(src[0], 0, 0));
  CHECK(d1[1] == 0x77);
  CHECK(d1[3] == 0x77);
}

TEST_CASE("dither_row_u16 count 0 touches nothing whatever the pointers", "[dither][row]") {
  std::uint8_t d[4] = {1, 2, 3, 4};
  std::uint16_t s[4] = {9, 9, 9, 9};
  CHECK(dither_row_u16(s, d, 0, 0, 0));
  CHECK(dither_row_u16(nullptr, nullptr, 0, 5, 5));
  CHECK(dither_row_u16(nullptr, d, 0, 5, 5));
  CHECK(dither_row_u16(s, nullptr, 0, 5, 5));
  CHECK(dither_row_u16(reinterpret_cast<const std::uint16_t*>(d), d, 0, 0, 0));  // overlapping, but count 0 comes first
  CHECK(d[0] == 1);
  CHECK(d[3] == 4);
}

TEST_CASE("dither_row_u16 rejects without writing", "[dither][row]") {
  std::vector<std::uint16_t> src(16, 40000);
  std::vector<std::uint8_t> dst(16, 0xAB);
  CHECK_FALSE(dither_row_u16(nullptr, dst.data(), 4, 0, 0));
  CHECK_FALSE(dither_row_u16(src.data(), nullptr, 4, 0, 0));
  CHECK_FALSE(dither_row_u16(nullptr, nullptr, 4, 0, 0));
  CHECK_FALSE(dither_row_u16(src.data(), dst.data(), kMaxRow + 1u, 0, 0));
  CHECK_FALSE(dither_row_u16(src.data(), dst.data(), UINT32_MAX, 0, 0));
  CHECK_FALSE(dither_row_u16(nullptr, nullptr, kMaxRow + 1u, 0, 0));
  CHECK_FALSE(dither_row_u16(src.data(), nullptr, UINT32_MAX, 0, 0));
  for (std::uint8_t b : dst) CHECK(b == 0xAB);
}

TEST_CASE("dither_row_u16 overlap uses half-open byte ranges", "[dither][row]") {
  const std::uint32_t n = 4;  // src 8 bytes, dst 4 bytes
  alignas(16) std::uint16_t words[64];
  std::uint8_t* bytes = reinterpret_cast<std::uint8_t*>(words);
  const std::uint16_t* src = words + 8;  // byte offset 16
  std::size_t bad = 0;
  for (int k = 0; k <= 40; ++k) {  // dst at byte offset k, range [k, k + 4); src range [16, 24)
    for (std::size_t i = 0; i < 64; ++i) words[i] = static_cast<std::uint16_t>(0x1111u * (i % 15u) + 7u);
    std::uint8_t before[128];
    for (std::size_t i = 0; i < 128; ++i) before[i] = bytes[i];
    const bool overlap = k < 24 && 16 < k + 4;  // [k, k + 4) against [16, 24)
    const bool ok = dither_row_u16(src, bytes + k, n, 0, 0);
    if (ok == overlap) ++bad;
    if (overlap) {
      for (std::size_t i = 0; i < 128; ++i)
        if (bytes[i] != before[i]) ++bad;  // nothing written on rejection
    } else {
      // success: four bytes at dst are the quantized values of the first four source words (read before the call)
      std::uint16_t sv[4];
      for (int i = 0; i < 4; ++i) sv[i] = static_cast<std::uint16_t>(before[16 + 2 * i] | (before[17 + 2 * i] << 8));
      for (std::uint32_t i = 0; i < n; ++i)
        if (bytes[k + static_cast<int>(i)] != oracle_quantize(sv[i], i, 0)) ++bad;
      for (std::size_t i = 0; i < 128; ++i) {
        const bool in_dst = i >= static_cast<std::size_t>(k) && i < static_cast<std::size_t>(k) + n;
        if (!in_dst && bytes[i] != before[i]) ++bad;  // nothing else changes
      }
    }
  }
  CHECK(bad == 0);
  // touching ranges: dst ends where src starts (k = 12) and dst starts where src ends (k = 24) are accepted;
  // one byte of overlap on either side (k = 13, k = 23) is rejected; identical addresses are rejected.
  for (std::size_t i = 0; i < 64; ++i) words[i] = 1000;
  CHECK(dither_row_u16(src, bytes + 12, n, 0, 0));
  CHECK(dither_row_u16(src, bytes + 24, n, 0, 0));
  CHECK_FALSE(dither_row_u16(src, bytes + 13, n, 0, 0));
  CHECK_FALSE(dither_row_u16(src, bytes + 23, n, 0, 0));
  CHECK_FALSE(dither_row_u16(src, bytes + 16, n, 0, 0));
  CHECK_FALSE(dither_row_u16(src, bytes + 16, 1, 0, 0));
  CHECK(dither_row_u16(src, bytes + 18, 1, 0, 0));  // dst starts where the 2-byte source range ends
  CHECK(dither_row_u16(src, bytes + 17, 1, 0, 0) == false);
  CHECK(dither_row_u16(src, bytes + 15, 1, 0, 0));
  CHECK(dither_row_u16(src, bytes + 24, 1, 0, 0));
}

TEST_CASE("dither_row_u16 accepts count kMaxRow and matches the oracle", "[dither][row][large]") {
  std::vector<std::uint16_t> src(kMaxRow);
  std::vector<std::uint8_t> dst(kMaxRow, 0);
  for (std::uint32_t i = 0; i < kMaxRow; ++i) src[i] = static_cast<std::uint16_t>((i * 40503u) >> 4);
  CHECK(dither_row_u16(src.data(), dst.data(), kMaxRow, 5, 3));
  std::size_t bad = 0;
  for (std::uint32_t i = 0; i < kMaxRow; ++i)
    if (dst[i] != oracle_quantize(src[i], 5u + i, 3u)) ++bad;
  CHECK(bad == 0);
  CHECK_FALSE(dither_row_u16(src.data(), dst.data(), kMaxRow + 1u, 5, 3));
}

TEST_CASE("threshold digest", "[dither][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t y : kRefYs) {
    for (std::uint32_t x : kRefXs) {
      const std::uint8_t t = threshold(x, y);
      if (t != oracle_threshold(x, y)) ++bad;
      f.byte(t);
      ++n;
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_threshold);
  CHECK(f.h == kRefDigest_threshold);
}

TEST_CASE("golden digest over every v at all 64 tile positions", "[dither][sweep]") {
  Fnv f;
  std::size_t n = 0;
  for (std::uint32_t v = 0; v <= 65535; ++v) {
    for (std::uint32_t y = 0; y < 8; ++y) {
      for (std::uint32_t x = 0; x < 8; ++x) {
        f.byte(quantize_u16(static_cast<std::uint16_t>(v), x, y));
        ++n;
      }
    }
  }
  CHECK(n == kRefCases_golden);
  CHECK(f.h == kRefDigest_golden);
}

TEST_CASE("quantize digest at extreme coordinates", "[dither][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (std::uint32_t y : kRefYs) {
    for (std::uint32_t x : kRefXs) {
      for (std::uint32_t v : kRefVs) {
        const std::uint8_t q = quantize_u16(static_cast<std::uint16_t>(v), x, y);
        if (q != oracle_quantize(v, x, y)) ++bad;
        f.byte(q);
        ++n;
      }
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_extreme);
  CHECK(f.h == kRefDigest_extreme);
}

TEST_CASE("row digest over patterns, counts, x0 and y", "[dither][sweep]") {
  Fnv f;
  std::size_t n = 0, bad = 0;
  for (int kind = 0; kind < kRefRowPatterns; ++kind) {
    for (std::uint32_t count : kRefRowCounts) {
      const std::vector<std::uint16_t> src = pattern(kind, count);
      for (std::uint32_t x0 : kRefRowX0) {
        for (std::uint32_t y : kRefRowY) {
          std::vector<std::uint8_t> dst(count + 2u, 0xCC);
          if (!dither_row_u16(src.data(), dst.data() + 1, count, x0, y)) ++bad;
          if (dst[0] != 0xCC || dst[count + 1] != 0xCC) ++bad;  // guard bytes
          for (std::uint32_t i = 0; i < count; ++i) {
            if (dst[1 + i] != oracle_quantize(src[i], x0 + i, y)) ++bad;
            f.byte(dst[1 + i]);
          }
          ++n;
        }
      }
    }
  }
  CHECK(bad == 0);
  CHECK(n == kRefCases_row);
  CHECK(f.h == kRefDigest_row);
}
