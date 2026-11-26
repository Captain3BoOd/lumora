// Tests for core/audio/analysis. Evidence label: unit tests only. Every comparison is exact.
// The block between BEGIN and END GENERATED REFERENCE comes from tools/audio_analysis_ref.py, an independent
// Python implementation (tables from math.cos, a recursive FFT). Regenerate it with:
//   python3 tools/audio_analysis_ref.py
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "analysis.h"

using namespace lumora::audio;

namespace {

constexpr std::uint64_t kM64 = std::numeric_limits<std::uint64_t>::max();

// BEGIN GENERATED REFERENCE (tools/audio_analysis_ref.py)
const std::int16_t kRefCos[1024] = {
    32767, 32767, 32766, 32762, 32758, 32753, 32746, 32738, 32729, 32718, 32706, 32693,
    32679, 32664, 32647, 32629, 32610, 32590, 32568, 32546, 32522, 32496, 32470, 32442,
    32413, 32383, 32352, 32319, 32286, 32251, 32214, 32177, 32138, 32099, 32058, 32015,
    31972, 31927, 31881, 31834, 31786, 31737, 31686, 31634, 31581, 31527, 31471, 31415,
    31357, 31298, 31238, 31177, 31114, 31050, 30986, 30920, 30853, 30784, 30715, 30644,
    30572, 30499, 30425, 30350, 30274, 30196, 30118, 30038, 29957, 29875, 29792, 29707,
    29622, 29535, 29448, 29359, 29269, 29178, 29086, 28993, 28899, 28803, 28707, 28610,
    28511, 28411, 28311, 28209, 28106, 28002, 27897, 27791, 27684, 27576, 27467, 27357,
    27246, 27133, 27020, 26906, 26791, 26674, 26557, 26439, 26320, 26199, 26078, 25956,
    25833, 25708, 25583, 25457, 25330, 25202, 25073, 24943, 24812, 24680, 24548, 24414,
    24279, 24144, 24008, 23870, 23732, 23593, 23453, 23312, 23170, 23028, 22884, 22740,
    22595, 22449, 22302, 22154, 22006, 21856, 21706, 21555, 21403, 21251, 21097, 20943,
    20788, 20632, 20475, 20318, 20160, 20001, 19841, 19681, 19520, 19358, 19195, 19032,
    18868, 18703, 18538, 18372, 18205, 18037, 17869, 17700, 17531, 17361, 17190, 17018,
    16846, 16673, 16500, 16326, 16151, 15976, 15800, 15624, 15447, 15269, 15091, 14912,
    14733, 14553, 14373, 14192, 14010, 13828, 13646, 13463, 13279, 13095, 12910, 12725,
    12540, 12354, 12167, 11980, 11793, 11605, 11417, 11228, 11039, 10850, 10660, 10469,
    10279, 10088, 9896, 9704, 9512, 9319, 9127, 8933, 8740, 8546, 8351, 8157,
    7962, 7767, 7571, 7376, 7180, 6983, 6787, 6590, 6393, 6195, 5998, 5800,
    5602, 5404, 5205, 5007, 4808, 4609, 4410, 4211, 4011, 3812, 3612, 3412,
    3212, 3012, 2811, 2611, 2411, 2210, 2009, 1809, 1608, 1407, 1206, 1005,
    804, 603, 402, 201, 0, -201, -402, -603, -804, -1005, -1206, -1407,
    -1608, -1809, -2009, -2210, -2411, -2611, -2811, -3012, -3212, -3412, -3612, -3812,
    -4011, -4211, -4410, -4609, -4808, -5007, -5205, -5404, -5602, -5800, -5998, -6195,
    -6393, -6590, -6787, -6983, -7180, -7376, -7571, -7767, -7962, -8157, -8351, -8546,
    -8740, -8933, -9127, -9319, -9512, -9704, -9896, -10088, -10279, -10469, -10660, -10850,
    -11039, -11228, -11417, -11605, -11793, -11980, -12167, -12354, -12540, -12725, -12910, -13095,
    -13279, -13463, -13646, -13828, -14010, -14192, -14373, -14553, -14733, -14912, -15091, -15269,
    -15447, -15624, -15800, -15976, -16151, -16326, -16500, -16673, -16846, -17018, -17190, -17361,
    -17531, -17700, -17869, -18037, -18205, -18372, -18538, -18703, -18868, -19032, -19195, -19358,
    -19520, -19681, -19841, -20001, -20160, -20318, -20475, -20632, -20788, -20943, -21097, -21251,
    -21403, -21555, -21706, -21856, -22006, -22154, -22302, -22449, -22595, -22740, -22884, -23028,
    -23170, -23312, -23453, -23593, -23732, -23870, -24008, -24144, -24279, -24414, -24548, -24680,
    -24812, -24943, -25073, -25202, -25330, -25457, -25583, -25708, -25833, -25956, -26078, -26199,
    -26320, -26439, -26557, -26674, -26791, -26906, -27020, -27133, -27246, -27357, -27467, -27576,
    -27684, -27791, -27897, -28002, -28106, -28209, -28311, -28411, -28511, -28610, -28707, -28803,
    -28899, -28993, -29086, -29178, -29269, -29359, -29448, -29535, -29622, -29707, -29792, -29875,
    -29957, -30038, -30118, -30196, -30274, -30350, -30425, -30499, -30572, -30644, -30715, -30784,
    -30853, -30920, -30986, -31050, -31114, -31177, -31238, -31298, -31357, -31415, -31471, -31527,
    -31581, -31634, -31686, -31737, -31786, -31834, -31881, -31927, -31972, -32015, -32058, -32099,
    -32138, -32177, -32214, -32251, -32286, -32319, -32352, -32383, -32413, -32442, -32470, -32496,
    -32522, -32546, -32568, -32590, -32610, -32629, -32647, -32664, -32679, -32693, -32706, -32718,
    -32729, -32738, -32746, -32753, -32758, -32762, -32766, -32767, -32767, -32767, -32766, -32762,
    -32758, -32753, -32746, -32738, -32729, -32718, -32706, -32693, -32679, -32664, -32647, -32629,
    -32610, -32590, -32568, -32546, -32522, -32496, -32470, -32442, -32413, -32383, -32352, -32319,
    -32286, -32251, -32214, -32177, -32138, -32099, -32058, -32015, -31972, -31927, -31881, -31834,
    -31786, -31737, -31686, -31634, -31581, -31527, -31471, -31415, -31357, -31298, -31238, -31177,
    -31114, -31050, -30986, -30920, -30853, -30784, -30715, -30644, -30572, -30499, -30425, -30350,
    -30274, -30196, -30118, -30038, -29957, -29875, -29792, -29707, -29622, -29535, -29448, -29359,
    -29269, -29178, -29086, -28993, -28899, -28803, -28707, -28610, -28511, -28411, -28311, -28209,
    -28106, -28002, -27897, -27791, -27684, -27576, -27467, -27357, -27246, -27133, -27020, -26906,
    -26791, -26674, -26557, -26439, -26320, -26199, -26078, -25956, -25833, -25708, -25583, -25457,
    -25330, -25202, -25073, -24943, -24812, -24680, -24548, -24414, -24279, -24144, -24008, -23870,
    -23732, -23593, -23453, -23312, -23170, -23028, -22884, -22740, -22595, -22449, -22302, -22154,
    -22006, -21856, -21706, -21555, -21403, -21251, -21097, -20943, -20788, -20632, -20475, -20318,
    -20160, -20001, -19841, -19681, -19520, -19358, -19195, -19032, -18868, -18703, -18538, -18372,
    -18205, -18037, -17869, -17700, -17531, -17361, -17190, -17018, -16846, -16673, -16500, -16326,
    -16151, -15976, -15800, -15624, -15447, -15269, -15091, -14912, -14733, -14553, -14373, -14192,
    -14010, -13828, -13646, -13463, -13279, -13095, -12910, -12725, -12540, -12354, -12167, -11980,
    -11793, -11605, -11417, -11228, -11039, -10850, -10660, -10469, -10279, -10088, -9896, -9704,
    -9512, -9319, -9127, -8933, -8740, -8546, -8351, -8157, -7962, -7767, -7571, -7376,
    -7180, -6983, -6787, -6590, -6393, -6195, -5998, -5800, -5602, -5404, -5205, -5007,
    -4808, -4609, -4410, -4211, -4011, -3812, -3612, -3412, -3212, -3012, -2811, -2611,
    -2411, -2210, -2009, -1809, -1608, -1407, -1206, -1005, -804, -603, -402, -201,
    0, 201, 402, 603, 804, 1005, 1206, 1407, 1608, 1809, 2009, 2210,
    2411, 2611, 2811, 3012, 3212, 3412, 3612, 3812, 4011, 4211, 4410, 4609,
    4808, 5007, 5205, 5404, 5602, 5800, 5998, 6195, 6393, 6590, 6787, 6983,
    7180, 7376, 7571, 7767, 7962, 8157, 8351, 8546, 8740, 8933, 9127, 9319,
    9512, 9704, 9896, 10088, 10279, 10469, 10660, 10850, 11039, 11228, 11417, 11605,
    11793, 11980, 12167, 12354, 12540, 12725, 12910, 13095, 13279, 13463, 13646, 13828,
    14010, 14192, 14373, 14553, 14733, 14912, 15091, 15269, 15447, 15624, 15800, 15976,
    16151, 16326, 16500, 16673, 16846, 17018, 17190, 17361, 17531, 17700, 17869, 18037,
    18205, 18372, 18538, 18703, 18868, 19032, 19195, 19358, 19520, 19681, 19841, 20001,
    20160, 20318, 20475, 20632, 20788, 20943, 21097, 21251, 21403, 21555, 21706, 21856,
    22006, 22154, 22302, 22449, 22595, 22740, 22884, 23028, 23170, 23312, 23453, 23593,
    23732, 23870, 24008, 24144, 24279, 24414, 24548, 24680, 24812, 24943, 25073, 25202,
    25330, 25457, 25583, 25708, 25833, 25956, 26078, 26199, 26320, 26439, 26557, 26674,
    26791, 26906, 27020, 27133, 27246, 27357, 27467, 27576, 27684, 27791, 27897, 28002,
    28106, 28209, 28311, 28411, 28511, 28610, 28707, 28803, 28899, 28993, 29086, 29178,
    29269, 29359, 29448, 29535, 29622, 29707, 29792, 29875, 29957, 30038, 30118, 30196,
    30274, 30350, 30425, 30499, 30572, 30644, 30715, 30784, 30853, 30920, 30986, 31050,
    31114, 31177, 31238, 31298, 31357, 31415, 31471, 31527, 31581, 31634, 31686, 31737,
    31786, 31834, 31881, 31927, 31972, 32015, 32058, 32099, 32138, 32177, 32214, 32251,
    32286, 32319, 32352, 32383, 32413, 32442, 32470, 32496, 32522, 32546, 32568, 32590,
    32610, 32629, 32647, 32664, 32679, 32693, 32706, 32718, 32729, 32738, 32746, 32753,
    32758, 32762, 32766, 32767,
};
const std::uint16_t kRefHann[1024] = {
    0, 0, 1, 3, 5, 8, 11, 15, 20, 25, 31, 37,
    44, 52, 60, 69, 79, 89, 100, 111, 123, 136, 149, 163,
    177, 192, 208, 224, 241, 259, 277, 296, 315, 335, 355, 376,
    398, 420, 443, 467, 491, 516, 541, 567, 593, 621, 648, 677,
    705, 735, 765, 796, 827, 859, 891, 924, 958, 992, 1027, 1062,
    1098, 1134, 1171, 1209, 1247, 1286, 1325, 1365, 1406, 1447, 1488, 1530,
    1573, 1616, 1660, 1704, 1749, 1795, 1841, 1887, 1935, 1982, 2030, 2079,
    2128, 2178, 2229, 2280, 2331, 2383, 2435, 2488, 2542, 2596, 2651, 2706,
    2761, 2817, 2874, 2931, 2989, 3047, 3105, 3165, 3224, 3284, 3345, 3406,
    3468, 3530, 3592, 3655, 3719, 3783, 3847, 3912, 3978, 4044, 4110, 4177,
    4244, 4312, 4380, 4449, 4518, 4587, 4657, 4728, 4799, 4870, 4942, 5014,
    5087, 5160, 5233, 5307, 5381, 5456, 5531, 5606, 5682, 5759, 5835, 5913,
    5990, 6068, 6146, 6225, 6304, 6383, 6463, 6543, 6624, 6705, 6786, 6868,
    6950, 7032, 7115, 7198, 7282, 7365, 7449, 7534, 7619, 7704, 7789, 7875,
    7961, 8047, 8134, 8221, 8308, 8396, 8484, 8572, 8661, 8749, 8839, 8928,
    9018, 9108, 9198, 9288, 9379, 9470, 9561, 9653, 9745, 9837, 9929, 10021,
    10114, 10207, 10300, 10394, 10487, 10581, 10676, 10770, 10864, 10959, 11054, 11149,
    11245, 11340, 11436, 11532, 11628, 11724, 11821, 11917, 12014, 12111, 12208, 12306,
    12403, 12501, 12598, 12696, 12794, 12892, 12991, 13089, 13188, 13286, 13385, 13484,
    13583, 13682, 13781, 13881, 13980, 14079, 14179, 14279, 14378, 14478, 14578, 14678,
    14778, 14878, 14978, 15078, 15179, 15279, 15379, 15480, 15580, 15680, 15781, 15881,
    15982, 16082, 16183, 16283, 16384, 16485, 16585, 16686, 16786, 16887, 16987, 17088,
    17188, 17288, 17389, 17489, 17589, 17690, 17790, 17890, 17990, 18090, 18190, 18290,
    18390, 18489, 18589, 18689, 18788, 18887, 18987, 19086, 19185, 19284, 19383, 19482,
    19580, 19679, 19777, 19876, 19974, 20072, 20170, 20267, 20365, 20462, 20560, 20657,
    20754, 20851, 20947, 21044, 21140, 21236, 21332, 21428, 21523, 21619, 21714, 21809,
    21904, 21998, 22092, 22187, 22281, 22374, 22468, 22561, 22654, 22747, 22839, 22931,
    23023, 23115, 23207, 23298, 23389, 23480, 23570, 23660, 23750, 23840, 23929, 24019,
    24107, 24196, 24284, 24372, 24460, 24547, 24634, 24721, 24807, 24893, 24979, 25064,
    25149, 25234, 25319, 25403, 25486, 25570, 25653, 25736, 25818, 25900, 25982, 26063,
    26144, 26225, 26305, 26385, 26464, 26543, 26622, 26700, 26778, 26855, 26933, 27009,
    27086, 27162, 27237, 27312, 27387, 27461, 27535, 27608, 27681, 27754, 27826, 27898,
    27969, 28040, 28111, 28181, 28250, 28319, 28388, 28456, 28524, 28591, 28658, 28724,
    28790, 28856, 28921, 28985, 29049, 29113, 29176, 29238, 29300, 29362, 29423, 29484,
    29544, 29603, 29663, 29721, 29779, 29837, 29894, 29951, 30007, 30062, 30117, 30172,
    30226, 30280, 30333, 30385, 30437, 30488, 30539, 30590, 30640, 30689, 30738, 30786,
    30833, 30881, 30927, 30973, 31019, 31064, 31108, 31152, 31195, 31238, 31280, 31321,
    31362, 31403, 31443, 31482, 31521, 31559, 31597, 31634, 31670, 31706, 31741, 31776,
    31810, 31844, 31877, 31909, 31941, 31972, 32003, 32033, 32063, 32091, 32120, 32147,
    32175, 32201, 32227, 32252, 32277, 32301, 32325, 32348, 32370, 32392, 32413, 32433,
    32453, 32472, 32491, 32509, 32527, 32544, 32560, 32576, 32591, 32605, 32619, 32632,
    32645, 32657, 32668, 32679, 32689, 32699, 32708, 32716, 32724, 32731, 32737, 32743,
    32748, 32753, 32757, 32760, 32763, 32765, 32767, 32767, 32767, 32767, 32767, 32765,
    32763, 32760, 32757, 32753, 32748, 32743, 32737, 32731, 32724, 32716, 32708, 32699,
    32689, 32679, 32668, 32657, 32645, 32632, 32619, 32605, 32591, 32576, 32560, 32544,
    32527, 32509, 32491, 32472, 32453, 32433, 32413, 32392, 32370, 32348, 32325, 32301,
    32277, 32252, 32227, 32201, 32175, 32147, 32120, 32091, 32063, 32033, 32003, 31972,
    31941, 31909, 31877, 31844, 31810, 31776, 31741, 31706, 31670, 31634, 31597, 31559,
    31521, 31482, 31443, 31403, 31362, 31321, 31280, 31238, 31195, 31152, 31108, 31064,
    31019, 30973, 30927, 30881, 30833, 30786, 30738, 30689, 30640, 30590, 30539, 30488,
    30437, 30385, 30333, 30280, 30226, 30172, 30117, 30062, 30007, 29951, 29894, 29837,
    29779, 29721, 29663, 29603, 29544, 29484, 29423, 29362, 29300, 29238, 29176, 29113,
    29049, 28985, 28921, 28856, 28790, 28724, 28658, 28591, 28524, 28456, 28388, 28319,
    28250, 28181, 28111, 28040, 27969, 27898, 27826, 27754, 27681, 27608, 27535, 27461,
    27387, 27312, 27237, 27162, 27086, 27009, 26933, 26855, 26778, 26700, 26622, 26543,
    26464, 26385, 26305, 26225, 26144, 26063, 25982, 25900, 25818, 25736, 25653, 25570,
    25486, 25403, 25319, 25234, 25149, 25064, 24979, 24893, 24807, 24721, 24634, 24547,
    24460, 24372, 24284, 24196, 24107, 24019, 23929, 23840, 23750, 23660, 23570, 23480,
    23389, 23298, 23207, 23115, 23023, 22931, 22839, 22747, 22654, 22561, 22468, 22374,
    22281, 22187, 22092, 21998, 21904, 21809, 21714, 21619, 21523, 21428, 21332, 21236,
    21140, 21044, 20947, 20851, 20754, 20657, 20560, 20462, 20365, 20267, 20170, 20072,
    19974, 19876, 19777, 19679, 19580, 19482, 19383, 19284, 19185, 19086, 18987, 18887,
    18788, 18689, 18589, 18489, 18390, 18290, 18190, 18090, 17990, 17890, 17790, 17690,
    17589, 17489, 17389, 17288, 17188, 17088, 16987, 16887, 16786, 16686, 16585, 16485,
    16384, 16283, 16183, 16082, 15982, 15881, 15781, 15680, 15580, 15480, 15379, 15279,
    15179, 15078, 14978, 14878, 14778, 14678, 14578, 14478, 14378, 14279, 14179, 14079,
    13980, 13881, 13781, 13682, 13583, 13484, 13385, 13286, 13188, 13089, 12991, 12892,
    12794, 12696, 12598, 12501, 12403, 12306, 12208, 12111, 12014, 11917, 11821, 11724,
    11628, 11532, 11436, 11340, 11245, 11149, 11054, 10959, 10864, 10770, 10676, 10581,
    10487, 10394, 10300, 10207, 10114, 10021, 9929, 9837, 9745, 9653, 9561, 9470,
    9379, 9288, 9198, 9108, 9018, 8928, 8839, 8749, 8661, 8572, 8484, 8396,
    8308, 8221, 8134, 8047, 7961, 7875, 7789, 7704, 7619, 7534, 7449, 7365,
    7282, 7198, 7115, 7032, 6950, 6868, 6786, 6705, 6624, 6543, 6463, 6383,
    6304, 6225, 6146, 6068, 5990, 5913, 5835, 5759, 5682, 5606, 5531, 5456,
    5381, 5307, 5233, 5160, 5087, 5014, 4942, 4870, 4799, 4728, 4657, 4587,
    4518, 4449, 4380, 4312, 4244, 4177, 4110, 4044, 3978, 3912, 3847, 3783,
    3719, 3655, 3592, 3530, 3468, 3406, 3345, 3284, 3224, 3165, 3105, 3047,
    2989, 2931, 2874, 2817, 2761, 2706, 2651, 2596, 2542, 2488, 2435, 2383,
    2331, 2280, 2229, 2178, 2128, 2079, 2030, 1982, 1935, 1887, 1841, 1795,
    1749, 1704, 1660, 1616, 1573, 1530, 1488, 1447, 1406, 1365, 1325, 1286,
    1247, 1209, 1171, 1134, 1098, 1062, 1027, 992, 958, 924, 891, 859,
    827, 796, 765, 735, 705, 677, 648, 621, 593, 567, 541, 516,
    491, 467, 443, 420, 398, 376, 355, 335, 315, 296, 277, 259,
    241, 224, 208, 192, 177, 163, 149, 136, 123, 111, 100, 89,
    79, 69, 60, 52, 44, 37, 31, 25, 20, 15, 11, 8,
    5, 3, 1, 0,
};
const std::uint64_t kRefFixtureDigest = 0xB8430644909D2656ull;
const std::size_t kRefFixtureCount = 135;
const std::uint64_t kRefIsqrtDigest = 0x94BAAEF6D87DB9DEull;
const std::uint64_t kRefEnvelopeDigest = 0x6A66EAA6F7E0261Aull;
const std::size_t kRefEnvelopeCount = 8100;
const std::uint64_t kRefBandDigest = 0x26FC14BDCA3F9F38ull;
const std::size_t kRefBandCount = 6000;
struct RefSpectrum64 {
  const char* name;
  std::uint32_t peak;
  std::uint64_t sum_squares;
  std::uint32_t power[32];
};
const RefSpectrum64 kRefSpectra64[] = {
    {"silence", 0u, 0ull, {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"dc_pos", 32767u, 68715282496ull, {268369924u, 67125249u, 1u, 1u, 0u, 0u, 1u, 1u, 0u, 0u, 2u, 0u, 0u, 1u, 0u, 2u, 0u, 0u, 0u, 2u, 0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u}},
    {"dc_neg", 32768u, 68719476736ull, {268402689u, 67076100u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 2u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u}},
    {"alt", 32768u, 68717379616ull, {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 67092481u}},
    {"impulse_0", 32767u, 1073676289ull, {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
    {"impulse_1", 32767u, 1073676289ull, {1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u}},
    {"impulse_32", 32767u, 1073676289ull, {261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u, 261121u, 262144u}},
    {"impulse_63", 32767u, 1073676289ull, {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 4u, 1u, 2u, 1u}},
    {"impulse_neg_1", 32768u, 1073741824ull, {4u, 4u, 4u, 4u, 4u, 4u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u}},
    {"tone_1", 32767u, 34358354995ull, {67125249u, 67076101u, 16785410u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 2u, 0u, 0u, 1u, 1u, 0u, 0u, 0u, 1u}},
    {"tone_neg_1", 32767u, 34359688982ull, {67076100u, 67125249u, 16769025u, 1u, 0u, 1u, 1u, 1u, 1u, 0u, 2u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 2u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 1u}},
    {"tone_2", 32767u, 34358203270ull, {4u, 16785409u, 67092481u, 16785409u, 1u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u, 2u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 1u, 0u}},
    {"tone_neg_2", 32767u, 34359534036ull, {1u, 16769025u, 67125249u, 16760836u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u}},
    {"tone_5", 32767u, 34358354995ull, {4u, 1u, 1u, 2u, 16777217u, 67092481u, 16785409u, 0u, 0u, 0u, 1u, 1u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 2u, 0u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u}},
    {"tone_neg_5", 32767u, 34359688982ull, {4u, 2u, 1u, 0u, 16769025u, 67125249u, 16769025u, 2u, 1u, 1u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, 0u, 0u}},
    {"tone_16", 32767u, 34356592720ull, {1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 16777216u, 67092481u, 16777216u, 0u, 1u, 1u, 0u, 2u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 2u, 1u}},
    {"tone_neg_16", 32767u, 34357641248ull, {1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 2u, 1u, 1u, 1u, 0u, 16769026u, 67108864u, 16777216u, 0u, 1u, 0u, 0u, 1u, 1u, 0u, 1u, 1u, 2u, 1u, 1u, 2u, 2u}},
    {"tone_31", 32767u, 34358354995ull, {4u, 1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 2u, 1u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 0u, 1u, 1u, 1u, 0u, 0u, 16777216u, 67092481u}},
    {"tone_neg_31", 32767u, 34359688982ull, {4u, 5u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 16769025u, 67125249u}},
    {"square_1", 32768u, 68717379616ull, {263169u, 108879205u, 48357394u, 12177425u, 7814673u, 4439057u, 3280338u, 2308882u, 1839680u, 1435033u, 1200193u, 992146u, 859153u, 737221u, 656298u, 581369u, 526340u, 477346u, 440410u, 405521u, 379793u, 356393u, 337769u, 320170u, 307088u, 296000u, 286480u, 278785u, 272548u, 267920u, 264644u, 262769u}},
    {"square_3", 32768u, 68717379616ull, {264196u, 355370u, 25836418u, 108858362u, 28682730u, 320170u, 267930u, 405521u, 2621440u, 12177425u, 3563633u, 295633u, 282025u, 476417u, 887269u, 4439057u, 1441540u, 278528u, 309668u, 581369u, 448768u, 2308882u, 829153u, 267920u, 356393u, 737221u, 300560u, 1432868u, 568978u, 262769u, 433540u, 991460u}},
    {"square_31", 32768u, 68717379616ull, {264196u, 261797u, 265570u, 267050u, 272548u, 277762u, 286169u, 294977u, 307088u, 320170u, 338245u, 355370u, 380818u, 405521u, 441253u, 476417u, 526340u, 580346u, 656298u, 738602u, 858128u, 992146u, 1199168u, 1434010u, 1839680u, 2309905u, 3279313u, 4440080u, 7813648u, 12178448u, 48370240u, 108859385u}},
    {"ramp", 32768u, 23632971706ull, {66564u, 15801890u, 781457u, 48842u, 7922u, 1937u, 626u, 257u, 122u, 64u, 25u, 16u, 17u, 5u, 5u, 4u, 4u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 0u}},
    {"noise_1", 32110u, 21686002362ull, {3575881u, 1163529u, 210960u, 257933u, 1079797u, 3819730u, 689572u, 753416u, 306049u, 950569u, 2667969u, 2392400u, 700173u, 1047572u, 2086373u, 704405u, 442621u, 2397785u, 4829225u, 3558385u, 480961u, 1705225u, 5856050u, 4261289u, 877669u, 937537u, 2787161u, 5376241u, 4829642u, 1498964u, 4449325u, 1043104u}},
    {"noise_2", 32535u, 23759720676ull, {1493284u, 2344717u, 3657800u, 6757085u, 2480413u, 7577104u, 8560413u, 1832008u, 1897825u, 2602321u, 2378756u, 707013u, 891922u, 1590121u, 493497u, 841748u, 1725904u, 2859946u, 1964153u, 1272365u, 2744352u, 833832u, 2410920u, 394810u, 723665u, 5091505u, 9444392u, 2125309u, 618500u, 489634u, 390289u, 1199365u}},
    {"noise_3", 32174u, 24697570249ull, {2972176u, 106525u, 7329761u, 11492377u, 5659957u, 1157120u, 170865u, 521210u, 316962u, 2421508u, 8865460u, 2464285u, 240722u, 1862290u, 6460345u, 3692330u, 333506u, 182484u, 385380u, 118625u, 221626u, 1890605u, 812466u, 33786u, 2451445u, 3178297u, 1137890u, 6436537u, 4589429u, 468953u, 2793661u, 84346u}},
    {"quiet_noise", 125u, 362922ull, {9u, 49u, 194u, 185u, 53u, 16u, 41u, 10u, 37u, 74u, 13u, 10u, 2u, 29u, 1u, 37u, 10u, 16u, 13u, 20u, 10u, 45u, 68u, 25u, 9u, 50u, 13u, 13u, 80u, 65u, 10u, 25u}},
};
// END GENERATED REFERENCE

// xorshift64*, the same generator as tools/audio_analysis_ref.py.
struct Rng {
  std::uint64_t s;
  explicit Rng(std::uint64_t seed) : s(seed) {}
  std::uint64_t next() {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 0x2545F4914F6CDD1Dull;
  }
};

std::int16_t i16(std::uint64_t v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xFFFF)); }

struct Fnv {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001b3ull;
  }
  void num(std::uint64_t v, int n) {
    for (int i = 0; i < n; ++i) byte(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  }
};

struct Fixture {
  std::string name;
  std::vector<std::int16_t> s;
};

// Same fixtures, in the same order, as the Python reference.
std::vector<Fixture> fixtures(std::size_t n) {
  std::vector<Fixture> f;
  auto add = [&](const std::string& name, std::vector<std::int16_t> s) { f.push_back({name, std::move(s)}); };
  add("silence", std::vector<std::int16_t>(n, 0));
  add("dc_pos", std::vector<std::int16_t>(n, 32767));
  add("dc_neg", std::vector<std::int16_t>(n, -32768));
  {
    std::vector<std::int16_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = i % 2 == 0 ? -32768 : 32767;
    add("alt", v);
  }
  for (std::size_t pos : {std::size_t{0}, std::size_t{1}, n / 2, n - 1}) {
    std::vector<std::int16_t> v(n, 0);
    v[pos] = 32767;
    add("impulse_" + std::to_string(pos), v);
  }
  {
    std::vector<std::int16_t> v(n, 0);
    v[1] = -32768;
    add("impulse_neg_1", v);
  }
  for (std::size_t b : {std::size_t{1}, std::size_t{2}, std::size_t{5}, n / 4, n / 2 - 1}) {
    std::vector<std::int16_t> v(n), w(n);
    for (std::size_t i = 0; i < n; ++i) {
      const std::int32_t c = kRefCos[(b * i * (1024 / n)) % 1024];
      v[i] = static_cast<std::int16_t>((32767 * c) >> 15);
      w[i] = static_cast<std::int16_t>(-((32768 * c) >> 15));
    }
    add("tone_" + std::to_string(b), v);
    add("tone_neg_" + std::to_string(b), w);
  }
  for (std::size_t b : {std::size_t{1}, std::size_t{3}, n / 2 - 1}) {
    std::vector<std::int16_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = (i * b * 2 / n) % 2 == 0 ? 32767 : -32768;
    add("square_" + std::to_string(b), v);
  }
  {
    std::vector<std::int16_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::int16_t>(-32768 + static_cast<std::int64_t>(i * 65535 / (n - 1)));
    add("ramp", v);
  }
  for (std::uint64_t seed : {1ull, 2ull, 3ull}) {
    Rng r(seed * 0x9E3779B97F4A7C15ull);
    std::vector<std::int16_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = i16(r.next() >> 48);
    add("noise_" + std::to_string(seed), v);
  }
  {
    Rng r(77);
    std::vector<std::int16_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::int16_t>(i16(r.next() >> 48) >> 8);
    add("quiet_noise", v);
  }
  return f;
}

std::vector<std::uint32_t> spectrum(const std::vector<std::int16_t>& s) {
  std::vector<std::uint32_t> out(s.size() / 2, 0xA5A5A5A5u);
  REQUIRE(fft_power(s.data(), s.size(), out.data()));
  return out;
}

// ---- Oracle: a recursive decimation-in-time FFT with tables from std::cos and std::sin (test code only). ----
struct Cx {
  std::int32_t re, im;
};

std::int64_t round_away(double x) { return x >= 0 ? static_cast<std::int64_t>(std::floor(x + 0.5)) : -static_cast<std::int64_t>(std::floor(-x + 0.5)); }
std::int64_t clamp15(std::int64_t v) { return v > 32767 ? 32767 : (v < -32767 ? -32767 : v); }

std::vector<Cx> oracle_fft(const std::vector<Cx>& x) {
  const std::size_t m = x.size();
  if (m == 1) return x;
  std::vector<Cx> even, odd;
  for (std::size_t i = 0; i < m; i += 2) {
    even.push_back(x[i]);
    odd.push_back(x[i + 1]);
  }
  const std::vector<Cx> e = oracle_fft(even), o = oracle_fft(odd);
  std::vector<Cx> out(m);
  const double pi = 3.14159265358979323846;
  for (std::size_t j = 0; j < m / 2; ++j) {
    const double angle = 2 * pi * static_cast<double>(j) / static_cast<double>(m);
    const std::int64_t wr = clamp15(round_away(32768.0 * std::cos(angle)));
    const std::int64_t wi = clamp15(round_away(-32768.0 * std::sin(angle)));
    const std::int64_t br = o[j].re, bi = o[j].im;
    const std::int32_t tre = static_cast<std::int32_t>((br * wr - bi * wi + 16384) >> 15);
    const std::int32_t tim = static_cast<std::int32_t>((br * wi + bi * wr + 16384) >> 15);
    out[j] = {(e[j].re + tre) >> 1, (e[j].im + tim) >> 1};
    out[j + m / 2] = {(e[j].re - tre) >> 1, (e[j].im - tim) >> 1};
  }
  return out;
}

std::vector<std::uint32_t> oracle_power(const std::vector<std::int16_t>& s) {
  const std::size_t n = s.size();
  const double pi = 3.14159265358979323846;
  std::vector<Cx> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::int64_t w = std::min<std::int64_t>(32767, round_away(32768.0 * (0.5 - 0.5 * std::cos(2 * pi * static_cast<double>(i) / static_cast<double>(n)))));
    x[i] = {static_cast<std::int32_t>((static_cast<std::int64_t>(s[i]) * w) >> 15), 0};
  }
  const std::vector<Cx> y = oracle_fft(x);
  std::vector<std::uint32_t> p(n / 2);
  for (std::size_t k = 0; k < n / 2; ++k) {
    const std::uint64_t v = static_cast<std::uint64_t>(static_cast<std::int64_t>(y[k].re) * y[k].re + static_cast<std::int64_t>(y[k].im) * y[k].im);
    REQUIRE(v <= 0xFFFFFFFFull);
    p[k] = static_cast<std::uint32_t>(v);
  }
  return p;
}

}  // namespace

TEST_CASE("reference tables match std::cos tables", "[audio_analysis]") {
  const double pi = 3.14159265358979323846;
  for (int j = 0; j < 1024; ++j) {
    const double a = 2 * pi * j / 1024;
    INFO("index " << j);
    REQUIRE(kRefCos[j] == clamp15(round_away(32768.0 * std::cos(a))));
    REQUIRE(kRefHann[j] == std::min<std::int64_t>(32767, round_away(32768.0 * (0.5 - 0.5 * std::cos(a)))));
  }
  REQUIRE(kRefCos[0] == 32767);
  REQUIRE(kRefCos[256] == 0);
  REQUIRE(kRefCos[512] == -32767);
  REQUIRE(kRefCos[768] == 0);
  REQUIRE(kRefHann[0] == 0);
  REQUIRE(kRefHann[512] == 32767);
  REQUIRE(kRefHann[256] == 16384);
}

TEST_CASE("measure_level", "[audio_analysis]") {
  Level l;
  l.peak = 7;
  l.sum_squares = 9;
  SECTION("empty block, samples may be null") {
    REQUIRE(measure_level(nullptr, 0, &l));
    REQUIRE(l.peak == 0);
    REQUIRE(l.sum_squares == 0);
    const std::int16_t one = 5;
    REQUIRE(measure_level(&one, 0, &l));
    REQUIRE(l.peak == 0);
  }
  SECTION("null pointers write nothing") {
    const std::int16_t one = 5;
    REQUIRE_FALSE(measure_level(nullptr, 1, &l));
    REQUIRE_FALSE(measure_level(&one, 1, nullptr));
    REQUIRE_FALSE(measure_level(nullptr, 0, nullptr));
    REQUIRE(l.peak == 7);
    REQUIRE(l.sum_squares == 9);
  }
  SECTION("values") {
    const std::int16_t a[] = {3, -4, 0, 2};
    REQUIRE(measure_level(a, 4, &l));
    REQUIRE(l.peak == 4);
    REQUIRE(l.sum_squares == 29);
    const std::int16_t m[] = {-32768};
    REQUIRE(measure_level(m, 1, &l));
    REQUIRE(l.peak == 32768);
    REQUIRE(l.sum_squares == 1073741824ull);
    const std::int16_t p[] = {32767, -32767};
    REQUIRE(measure_level(p, 2, &l));
    REQUIRE(l.peak == 32767);
    REQUIRE(l.sum_squares == 2ull * 32767 * 32767);
    const std::int16_t z[] = {0, 0, 0};
    REQUIRE(measure_level(z, 3, &l));
    REQUIRE(l.peak == 0);
    REQUIRE(l.sum_squares == 0);
    const std::int16_t first[] = {-1, 1};
    REQUIRE(measure_level(first, 2, &l));
    REQUIRE(l.peak == 1);
  }
  SECTION("a large block of -32768 is exact") {
    const std::vector<std::int16_t> v(1u << 20, -32768);
    REQUIRE(measure_level(v.data(), v.size(), &l));
    REQUIRE(l.peak == 32768);
    REQUIRE(l.sum_squares == (1ull << 50));
  }
  SECTION("peak is the largest magnitude wherever it sits") {
    for (std::size_t pos = 0; pos < 5; ++pos) {
      std::int16_t v[5] = {1, -2, 3, -4, 5};
      v[pos] = -32768;
      REQUIRE(measure_level(v, 5, &l));
      REQUIRE(l.peak == 32768);
    }
  }
}

TEST_CASE("isqrt64", "[audio_analysis]") {
  SECTION("small values") {
    for (std::uint64_t v = 0; v < 70000; ++v) {
      std::uint64_t r = 0;
      while ((r + 1) * (r + 1) <= v) ++r;
      REQUIRE(isqrt64(v) == r);
    }
  }
  SECTION("squares and their neighbours") {
    std::vector<std::uint64_t> ks = {0, 1, 2, 65535, 65536, 2147483647ull, 2147483648ull, 4294967294ull, 4294967295ull};
    Rng r(5);
    for (int i = 0; i < 3000; ++i) ks.push_back(r.next() >> 32);
    for (std::uint64_t k : ks) {
      REQUIRE(isqrt64(k * k) == k);
      if (k > 0) REQUIRE(isqrt64(k * k - 1) == k - 1);
      if (k > 0 && k < 4294967295ull) REQUIRE(isqrt64(k * k + 1) == k);
      if (k < 4294967295ull) REQUIRE(isqrt64((k + 1) * (k + 1) - 1) == k);
    }
  }
  SECTION("the maximum") {
    REQUIRE(isqrt64(kM64) == 4294967295u);
    REQUIRE(isqrt64(kM64 - 1) == 4294967295u);
    REQUIRE(isqrt64(4294967295ull * 4294967295ull) == 4294967295u);
    REQUIRE(isqrt64(4294967295ull * 4294967295ull - 1) == 4294967294u);
    REQUIRE(isqrt64(1ull << 63) == 3037000499u);
    REQUIRE(isqrt64((1ull << 62) - 1) == 2147483647u);
    REQUIRE(isqrt64(1ull << 62) == 2147483648u);
  }
  SECTION("random values against r*r <= v < (r+1)^2") {
    Rng r(6);
    for (int i = 0; i < 20000; ++i) {
      const std::uint64_t a = r.next();
      const std::uint64_t b = r.next();
      const std::uint64_t v = a >> (b % 64);
      const std::uint64_t root = isqrt64(v);
      REQUIRE(root * root <= v);
      REQUIRE((root == 4294967295ull || v < (root + 1) * (root + 1)));
    }
  }
  SECTION("digest matches the Python math.isqrt reference") {
    Fnv h;
    for (std::uint64_t v = 0; v < 70000; ++v) h.num(isqrt64(v), 4);
    std::vector<std::uint64_t> ks;
    for (std::uint64_t k = 0; k < 70000; k += 7) ks.push_back(k);
    for (std::uint64_t k : {65535ull, 65536ull, 2147483647ull, 2147483648ull, 4294967294ull, 4294967295ull}) ks.push_back(k);
    Rng r(5);
    for (int i = 0; i < 3000; ++i) ks.push_back(r.next() >> 32);
    for (std::uint64_t k : ks) {
      if (k > 0) h.num(isqrt64(k * k - 1), 4);
      h.num(isqrt64(k * k), 4);
      h.num(isqrt64(k * k + 1), 4);
    }
    h.num(isqrt64(kM64), 4);
    Rng q(6);
    for (int i = 0; i < 20000; ++i) {
      const std::uint64_t a = q.next();
      const std::uint64_t b = q.next();
      h.num(isqrt64(a >> (b % 64)), 4);
    }
    REQUIRE(h.h == kRefIsqrtDigest);
  }
}

TEST_CASE("envelope_step", "[audio_analysis]") {
  SECTION("equal returns env whatever the coefficients") {
    REQUIRE(envelope_step(100, 100, 0, 0) == 100);
    REQUIRE(envelope_step(100, 100, 65536, 65536) == 100);
    REQUIRE(envelope_step(0, 0, 1, 1) == 0);
    REQUIRE(envelope_step(0xFFFFFFFFu, 0xFFFFFFFFu, 5, 5) == 0xFFFFFFFFu);
  }
  SECTION("attack is used above, release below") {
    REQUIRE(envelope_step(0, 1000, 32768, 0) == 500);
    REQUIRE(envelope_step(1000, 0, 0, 32768) == 500);
    REQUIRE(envelope_step(0, 1000, 0, 65536) == 0);   // attack 0 holds, release unused
    REQUIRE(envelope_step(1000, 0, 65536, 0) == 1000);  // release 0 holds, attack unused
  }
  SECTION("65536 reaches x exactly in both directions") {
    REQUIRE(envelope_step(10, 1000, 65536, 0) == 1000);
    REQUIRE(envelope_step(1000, 10, 0, 65536) == 10);
    REQUIRE(envelope_step(0, 0xFFFFFFFFu, 65536, 0) == 0xFFFFFFFFu);
    REQUIRE(envelope_step(0xFFFFFFFFu, 0, 0, 65536) == 0);
  }
  SECTION("coefficients above 65536 are clamped") {
    REQUIRE(envelope_step(10, 1000, 65537, 0) == 1000);
    REQUIRE(envelope_step(10, 1000, 0xFFFFFFFFu, 0) == 1000);
    REQUIRE(envelope_step(1000, 10, 0, 65537) == 10);
    REQUIRE(envelope_step(1000, 10, 0, 0xFFFFFFFFu) == 10);
  }
  SECTION("the shift floors: a falling step rounds down, a rising step rounds down") {
    REQUIRE(envelope_step(10, 5, 0, 1) == 9);    // delta -5, coef 1: step -1
    REQUIRE(envelope_step(5, 10, 1, 0) == 5);    // delta +5, coef 1: step 0
    REQUIRE(envelope_step(100, 99, 0, 1) == 99);
    REQUIRE(envelope_step(99, 100, 1, 0) == 99);
    REQUIRE(envelope_step(0, 65535, 1, 0) == 0);
    REQUIRE(envelope_step(0, 65536, 1, 0) == 1);
    REQUIRE(envelope_step(100, 0, 0, 32767) == 50);  // -100*32767 >> 16 = floor(-49.99) = -50
  }
  SECTION("full range deltas do not overflow") {
    REQUIRE(envelope_step(0, 0xFFFFFFFFu, 32768, 0) == 2147483647u);
    REQUIRE(envelope_step(0xFFFFFFFFu, 0, 0, 32768) == 2147483647u);  // floor(-(2^32-1)/2) = -2^31
  }
  SECTION("digest matches the Python reference") {
    Fnv h;
    std::size_t cnt = 0;
    const std::uint32_t vals[] = {0, 1, 2, 100, 65535, 65536, 65537, 2147483648u, 4294967294u, 4294967295u};
    const std::uint32_t coefs[] = {0, 1, 2, 100, 32768, 65535, 65536, 65537, 4294967295u};
    for (std::uint32_t env : vals)
      for (std::uint32_t x : vals)
        for (std::uint32_t a : coefs)
          for (std::uint32_t rl : coefs) {
            const std::uint32_t v = envelope_step(env, x, a, rl);
            REQUIRE(v >= std::min(env, x));
            REQUIRE(v <= std::max(env, x));
            h.num(v, 4);
            ++cnt;
          }
    REQUIRE(cnt == kRefEnvelopeCount);
    REQUIRE(h.h == kRefEnvelopeDigest);
  }
}

TEST_CASE("fft_power errors write nothing", "[audio_analysis]") {
  std::vector<std::int16_t> s(2048, 1000);
  std::vector<std::uint32_t> out(1024, 0x5A5A5A5Au);
  for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{32}, std::size_t{63}, std::size_t{65},
                        std::size_t{96}, std::size_t{100}, std::size_t{192}, std::size_t{1000}, std::size_t{1023},
                        std::size_t{1025}, std::size_t{1536}, std::size_t{2048}, std::size_t{4096}, kM64}) {
    INFO("n " << n);
    REQUIRE_FALSE(fft_power(s.data(), n, out.data()));
  }
  for (std::uint32_t v : out) REQUIRE(v == 0x5A5A5A5Au);
  REQUIRE_FALSE(fft_power(nullptr, 64, out.data()));
  REQUIRE_FALSE(fft_power(s.data(), 64, nullptr));
  REQUIRE_FALSE(fft_power(nullptr, 64, nullptr));
  REQUIRE_FALSE(fft_power(nullptr, 100, out.data()));
  for (std::uint32_t v : out) REQUIRE(v == 0x5A5A5A5Au);
  for (std::size_t n : {std::size_t{64}, std::size_t{128}, std::size_t{256}, std::size_t{512}, std::size_t{1024}}) {
    std::fill(out.begin(), out.end(), 0x5A5A5A5Au);
    REQUIRE(fft_power(s.data(), n, out.data()));
    for (std::size_t k = n / 2; k < out.size(); ++k) REQUIRE(out[k] == 0x5A5A5A5Au);  // only n/2 bins written
    for (std::size_t k = 0; k < n / 2; ++k) REQUIRE(out[k] != 0x5A5A5A5Au);
  }
}

TEST_CASE("fft_power constants", "[audio_analysis]") {
  REQUIRE(kMinFftSize == 64);
  REQUIRE(kMaxFftSize == 1024);
}

TEST_CASE("fft_power matches the Python reference, exact", "[audio_analysis]") {
  SECTION("full spectra of every fixture at 64 samples") {
    const std::vector<Fixture> fx = fixtures(64);
    REQUIRE(fx.size() == std::size(kRefSpectra64));
    for (std::size_t i = 0; i < fx.size(); ++i) {
      INFO(fx[i].name);
      REQUIRE(fx[i].name == kRefSpectra64[i].name);
      const std::vector<std::uint32_t> p = spectrum(fx[i].s);
      for (std::size_t k = 0; k < 32; ++k) REQUIRE(p[k] == kRefSpectra64[i].power[k]);
      Level l;
      REQUIRE(measure_level(fx[i].s.data(), fx[i].s.size(), &l));
      REQUIRE(l.peak == kRefSpectra64[i].peak);
      REQUIRE(l.sum_squares == kRefSpectra64[i].sum_squares);
    }
  }
  SECTION("digest over every fixture at every size, spectrum and level") {
    Fnv h;
    std::size_t cnt = 0;
    for (std::size_t n : {std::size_t{64}, std::size_t{128}, std::size_t{256}, std::size_t{512}, std::size_t{1024}}) {
      for (const Fixture& f : fixtures(n)) {
        for (std::uint32_t v : spectrum(f.s)) h.num(v, 4);
        Level l;
        REQUIRE(measure_level(f.s.data(), f.s.size(), &l));
        h.num(l.peak, 4);
        h.num(l.sum_squares, 8);
        ++cnt;
      }
    }
    REQUIRE(cnt == kRefFixtureCount);
    REQUIRE(h.h == kRefFixtureDigest);
  }
}

TEST_CASE("fft_power by hand", "[audio_analysis]") {
  SECTION("silence is all zero") {
    for (std::size_t n : {std::size_t{64}, std::size_t{1024}}) {
      for (std::uint32_t v : spectrum(std::vector<std::int16_t>(n, 0))) REQUIRE(v == 0);
    }
  }
  SECTION("DC: the Hann window puts the energy in bins 0 and 1") {
    const std::vector<std::uint32_t> p = spectrum(std::vector<std::int16_t>(64, 32767));
    REQUIRE(p[0] > 0);
    REQUIRE(p[1] > 0);
    for (std::size_t k = 3; k < 32; ++k) REQUIRE(p[k] <= 4);  // rounding noise only
    REQUIRE(p[0] > 1000000);
  }
  SECTION("a tone puts its peak in its bin") {
    for (std::size_t n : {std::size_t{64}, std::size_t{256}, std::size_t{1024}}) {
      for (std::size_t b : {std::size_t{3}, n / 4}) {
        std::vector<std::int16_t> v(n);
        for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::int16_t>((32767 * kRefCos[(b * i * (1024 / n)) % 1024]) >> 15);
        const std::vector<std::uint32_t> p = spectrum(v);
        std::size_t best = 0;
        for (std::size_t k = 0; k < p.size(); ++k)
          if (p[k] > p[best]) best = k;
        REQUIRE(best == b);
      }
    }
  }
  SECTION("an impulse is flat to within 1 percent") {
    std::vector<std::int16_t> v(64, 0);
    v[32] = 32767;
    const std::vector<std::uint32_t> p = spectrum(v);
    for (std::size_t k = 1; k < 32; ++k) {
      REQUIRE(p[k] * 100 > p[0] * 99);
      REQUIRE(p[k] * 100 < p[0] * 101);
    }
    REQUIRE(p[0] > 0);
  }
  SECTION("the largest power seen over every fixture and size stays below 2^32") {
    std::uint64_t best = 0;
    for (std::size_t n : {std::size_t{64}, std::size_t{128}, std::size_t{256}, std::size_t{512}, std::size_t{1024}}) {
      for (const Fixture& f : fixtures(n)) {
        for (std::uint32_t v : oracle_power(f.s)) best = std::max<std::uint64_t>(best, v);
      }
    }
    REQUIRE(best > 0);
    REQUIRE(best < (1ull << 32));
  }
}

TEST_CASE("fft_power against the recursive oracle", "[audio_analysis]") {
  SECTION("every fixture at every size") {
    for (std::size_t n : {std::size_t{64}, std::size_t{128}, std::size_t{256}, std::size_t{512}, std::size_t{1024}}) {
      for (const Fixture& f : fixtures(n)) {
        INFO("n " << n << " " << f.name);
        REQUIRE(spectrum(f.s) == oracle_power(f.s));
      }
    }
  }
  SECTION("seeded random blocks, full scale and reduced scale, every size") {
    Rng r(20260312);
    for (std::size_t n : {std::size_t{64}, std::size_t{128}, std::size_t{256}, std::size_t{512}, std::size_t{1024}}) {
      for (int iter = 0; iter < 60; ++iter) {
        std::vector<std::int16_t> v(n);
        const unsigned shift = static_cast<unsigned>(iter % 8) * 2;
        const std::uint64_t mode = r.next() % 4;
        for (std::size_t i = 0; i < n; ++i) {
          const std::int16_t x = i16(r.next() >> 48);
          if (mode == 0) v[i] = static_cast<std::int16_t>(x >> shift);
          else if (mode == 1) v[i] = (r.next() & 1) ? 32767 : -32768;
          else if (mode == 2) v[i] = (i % 7 == 0) ? static_cast<std::int16_t>(x) : 0;
          else v[i] = (x < 0) ? static_cast<std::int16_t>(-32768) : static_cast<std::int16_t>(32767);
        }
        INFO("n " << n << " iter " << iter);
        REQUIRE(spectrum(v) == oracle_power(v));
      }
    }
  }
}

TEST_CASE("band_energies", "[audio_analysis]") {
  const std::uint32_t power[] = {1, 2, 3, 4, 5, 6, 7, 8};
  std::uint64_t out[4];
  auto reset = [&] { for (auto& v : out) v = 0xAAAAAAAAull; };
  SECTION("sums") {
    reset();
    const std::uint32_t edges[] = {0, 2, 5, 8};
    REQUIRE(band_energies(power, 8, edges, 3, out));
    REQUIRE(out[0] == 3);
    REQUIRE(out[1] == 12);
    REQUIRE(out[2] == 21);
    REQUIRE(out[3] == 0xAAAAAAAAull);  // only band_count entries are written
  }
  SECTION("bins outside the bands are ignored and edges[0] may exceed 0") {
    reset();
    const std::uint32_t edges[] = {2, 4, 6};
    REQUIRE(band_energies(power, 8, edges, 2, out));
    REQUIRE(out[0] == 7);
    REQUIRE(out[1] == 11);
  }
  SECTION("empty bands give 0, also at the end") {
    reset();
    const std::uint32_t edges[] = {3, 3, 3, 8, 8};
    REQUIRE(band_energies(power, 8, edges, 4, out));
    REQUIRE(out[0] == 0);
    REQUIRE(out[1] == 0);
    REQUIRE(out[2] == 4 + 5 + 6 + 7 + 8);
    REQUIRE(out[3] == 0);
  }
  SECTION("sums are exact in 64 bits") {
    reset();
    const std::uint32_t big[] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    const std::uint32_t edges[] = {0, 3};
    REQUIRE(band_energies(big, 3, edges, 1, out));
    REQUIRE(out[0] == 3ull * 0xFFFFFFFFull);
  }
  SECTION("edges[band_count] equal to bins passes, one more fails") {
    reset();
    const std::uint32_t ok[] = {0, 8};
    REQUIRE(band_energies(power, 8, ok, 1, out));
    const std::uint32_t bad[] = {0, 9};
    reset();
    REQUIRE_FALSE(band_energies(power, 8, bad, 1, out));
    REQUIRE(out[0] == 0xAAAAAAAAull);
  }
  SECTION("decreasing edges fail anywhere, equal edges pass") {
    reset();
    const std::uint32_t bad_last[] = {0, 4, 3};
    REQUIRE_FALSE(band_energies(power, 8, bad_last, 2, out));
    const std::uint32_t bad_first[] = {5, 4, 6};
    REQUIRE_FALSE(band_energies(power, 8, bad_first, 2, out));
    const std::uint32_t bad_mid[] = {0, 3, 5, 4, 8};
    REQUIRE_FALSE(band_energies(power, 8, bad_mid, 4, out));
    for (int i = 0; i < 4; ++i) REQUIRE(out[i] == 0xAAAAAAAAull);
    const std::uint32_t same[] = {3, 3, 3};
    REQUIRE(band_energies(power, 8, same, 2, out));
  }
  SECTION("band_count 0") {
    reset();
    const std::uint32_t at_bins[] = {8};
    REQUIRE(band_energies(power, 8, at_bins, 0, out));
    REQUIRE(band_energies(nullptr, 8, at_bins, 0, nullptr));
    const std::uint32_t over[] = {9};
    REQUIRE_FALSE(band_energies(power, 8, over, 0, out));
    REQUIRE_FALSE(band_energies(power, 8, nullptr, 0, out));
    REQUIRE_FALSE(band_energies(nullptr, 8, nullptr, 0, nullptr));
    const std::uint32_t zero[] = {0};
    REQUIRE(band_energies(power, 0, zero, 0, out));
    REQUIRE_FALSE(band_energies(power, 0, over, 0, out));
    for (int i = 0; i < 4; ++i) REQUIRE(out[i] == 0xAAAAAAAAull);
  }
  SECTION("null pointers") {
    reset();
    const std::uint32_t edges[] = {0, 2, 4};
    REQUIRE_FALSE(band_energies(power, 8, nullptr, 2, out));
    REQUIRE_FALSE(band_energies(power, 8, edges, 2, nullptr));
    REQUIRE_FALSE(band_energies(nullptr, 8, edges, 2, out));
    for (int i = 0; i < 4; ++i) REQUIRE(out[i] == 0xAAAAAAAAull);
    // power may be null when no band is non-empty
    const std::uint32_t empty_edges[] = {3, 3, 3};
    REQUIRE(band_energies(nullptr, 8, empty_edges, 2, out));
    REQUIRE(out[0] == 0);
    REQUIRE(out[1] == 0);
    // but one non-empty band needs it
    const std::uint32_t one[] = {3, 3, 4};
    REQUIRE_FALSE(band_energies(nullptr, 8, one, 2, out));
    REQUIRE(out[0] == 0);
    REQUIRE(out[1] == 0);
  }
  SECTION("digest matches the Python reference") {
    Fnv h;
    Rng r(9);
    std::size_t cases = 0;
    for (int iter = 0; iter < 6000; ++iter) {
      const std::size_t bins = r.next() % 40;
      const std::size_t count = r.next() % 6;
      std::vector<std::uint32_t> pw;
      for (std::size_t i = 0; i < bins; ++i) {
        const std::uint64_t c = r.next() % 4;
        if (c != 0) pw.push_back(static_cast<std::uint32_t>((r.next() >> 33) & 0xFFFFFFFFull));
        else pw.push_back(0xFFFFFFFFu);
      }
      std::vector<std::uint32_t> edges;
      if (r.next() % 3 != 0) {
        for (std::size_t i = 0; i < count + 1; ++i) edges.push_back(static_cast<std::uint32_t>(r.next() % (bins + 1)));
        std::sort(edges.begin(), edges.end());
      } else {
        for (std::size_t i = 0; i < count + 1; ++i) edges.push_back(static_cast<std::uint32_t>(r.next() % (bins + 3)));
      }
      std::vector<std::uint64_t> en(count, 0x1234567ull);
      const bool ok = band_energies(pw.empty() ? nullptr : pw.data(), bins, edges.data(), count, en.empty() ? nullptr : en.data());
      h.byte(ok ? 1 : 0);
      for (std::uint32_t v : pw) h.num(v, 4);
      for (std::uint32_t v : edges) h.num(v, 4);
      if (ok) {
        for (std::uint64_t v : en) h.num(v, 8);
      } else {
        for (std::uint64_t v : en) REQUIRE(v == 0x1234567ull);
      }
      ++cases;
    }
    REQUIRE(cases == kRefBandCount);
    REQUIRE(h.h == kRefBandDigest);
  }
}
