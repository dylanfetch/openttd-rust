// Monocypher version 4.0.2
//
// This file is dual-licensed.  Choose whichever licence you want from
// the two licences listed below.
//
// The first licence is a regular 2-clause BSD licence.  The second licence
// is the CC-0 from Creative Commons. It is intended to release Monocypher
// to the public domain.  The BSD licence serves as a fallback option.
//
// SPDX-License-Identifier: BSD-2-Clause OR CC0-1.0
//
// ------------------------------------------------------------------------
//
// Copyright (c) 2017-2020, Loup Vaillant
// All rights reserved.
//
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ------------------------------------------------------------------------
//
// Written in 2017-2020 by Loup Vaillant
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related neighboring rights to this software to the public domain
// worldwide.  This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software.  If not, see
// <https://creativecommons.org/publicdomain/zero/1.0/>

//! Remaining Monocypher 4.0.2 scalar/Edwards/Elligator family. All field calls
//! stay in the parent Rust core; only original nonthrowing wipe/verify leaves
//! cross back to C++. Final-storage raw accesses preserve original aliasing.
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::many_single_char_names,
    clippy::similar_names,
    clippy::too_many_arguments,
    clippy::needless_range_loop
)]
// Narrowing, notation and raw indexed carry order preserve the original scalar/field source.
use super::{
    Fe, Leaves, SQRTM1, fe_0, fe_1, fe_add, fe_ccopy, fe_copy, fe_cswap, fe_frombytes,
    fe_frombytes_mask, fe_invert, fe_mul, fe_mul_small, fe_neg, fe_sq, fe_sub, fe_tobytes, invsqrt,
    load, scalarmult, store, sub32, trim, wipe,
};
use std::mem::{MaybeUninit, size_of};
use std::ptr::{addr_of, null};
#[repr(C)]
struct Ge {
    x: Fe,
    y: Fe,
    z: Fe,
    t: Fe,
}
#[repr(C)]
struct Cached {
    yp: Fe,
    ym: Fe,
    z: Fe,
    t2: Fe,
}
#[repr(C)]
struct Precomp {
    yp: Fe,
    ym: Fe,
    t2: Fe,
}
// Address formation never reads an uninitialized aggregate or creates a borrow.
macro_rules! f {
    ($p:expr,$field:ident) => {
        unsafe { addr_of!((*$p).$field).cast::<i32>().cast_mut() }
    };
}
macro_rules! local {
    ($p:ident,$storage:ident,$ty:ty) => {
        let mut $storage = MaybeUninit::<$ty>::uninit();
        let $p = $storage.as_mut_ptr();
    };
}
macro_rules! field {
    ($p:ident,$storage:ident) => {
        local!($p, $storage, Fe);
        let $p = $p.cast::<i32>();
    };
}
macro_rules! bytes {
    ($p:ident,$storage:ident,$n:expr) => {
        local!($p, $storage, [u8; $n]);
        let $p = $p.cast::<u8>();
    };
}
macro_rules! words {
    ($p:ident,$storage:ident,$n:expr) => {
        local!($p, $storage, [u32; $n]);
        let $p = $p.cast::<u32>();
    };
}
const ONE: Fe = [1, 0, 0, 0, 0, 0, 0, 0, 0, 0];
const D: Fe = [
    -10_913_610,
    13_857_413,
    -15_372_611,
    6_949_391,
    114_729,
    -8_787_816,
    -6_275_908,
    -3_247_719,
    -18_696_448,
    -12_055_116,
];
const D2: Fe = [
    -21_827_239,
    -5_839_606,
    -30_745_221,
    13_898_782,
    229_458,
    15_978_800,
    -12_551_817,
    -6_495_438,
    29_715_968,
    9_444_199,
];
const LOP_X: Fe = [
    21_352_778,
    5_345_713,
    4_660_180,
    -8_347_857,
    24_143_090,
    14_568_123,
    30_185_756,
    -12_247_770,
    -33_528_939,
    8_345_319,
];
const LOP_Y: Fe = [
    -6_952_922,
    -1_265_500,
    6_862_341,
    -7_057_498,
    -4_037_696,
    -5_447_722,
    31_680_899,
    -15_325_402,
    -19_365_852,
    1_569_102,
];
const UFACTOR: Fe = [
    -1_917_299,
    15_887_451,
    -18_755_900,
    -7_000_830,
    -24_778_944,
    544_946,
    -16_816_446,
    4_011_309,
    -653_372,
    10_741_468,
];
const A2: Fe = [12_721_188, 3529, 0, 0, 0, 0, 0, 0, 0, 0];
const A: Fe = [486_662, 0, 0, 0, 0, 0, 0, 0, 0, 0];
static B_WINDOW: [Precomp; 8] = [
    Precomp {
        yp: [
            25_967_493,
            -14_356_035,
            29_566_456,
            3_660_896,
            -12_694_345,
            4_014_787,
            27_544_626,
            -11_754_271,
            -6_079_156,
            2_047_605,
        ],
        ym: [
            -12_545_711,
            934_262,
            -2_722_910,
            3_049_990,
            -727_428,
            9_406_986,
            12_720_692,
            5_043_384,
            19_500_929,
            -15_469_378,
        ],
        t2: [
            -8_738_181,
            4_489_570,
            9_688_441,
            -14_785_194,
            10_184_609,
            -12_363_380,
            29_287_919,
            11_864_899,
            -24_514_362,
            -4_438_546,
        ],
    },
    Precomp {
        yp: [
            15_636_291,
            -9_688_557,
            24_204_773,
            -7_912_398,
            616_977,
            -16_685_262,
            27_787_600,
            -14_772_189,
            28_944_400,
            -1_550_024,
        ],
        ym: [
            16_568_933,
            4_717_097,
            -11_556_148,
            -1_102_322,
            15_682_896,
            -11_807_043,
            16_354_577,
            -11_775_962,
            7_689_662,
            11_199_574,
        ],
        t2: [
            30_464_156,
            -5_976_125,
            -11_779_434,
            -15_670_865,
            23_220_365,
            15_915_852,
            7_512_774,
            10_017_326,
            -17_749_093,
            -9_920_357,
        ],
    },
    Precomp {
        yp: [
            10_861_363,
            11_473_154,
            27_284_546,
            1_981_175,
            -30_064_349,
            12_577_861,
            32_867_885,
            14_515_107,
            -15_438_304,
            10_819_380,
        ],
        ym: [
            4_708_026,
            6_336_745,
            20_377_586,
            9_066_809,
            -11_272_109,
            6_594_696,
            -25_653_668,
            12_483_688,
            -12_668_491,
            5_581_306,
        ],
        t2: [
            19_563_160,
            16_186_464,
            -29_386_857,
            4_097_519,
            10_237_984,
            -4_348_115,
            28_542_350,
            13_850_243,
            -23_678_021,
            -15_815_942,
        ],
    },
    Precomp {
        yp: [
            5_153_746,
            9_909_285,
            1_723_747,
            -2_777_874,
            30_523_605,
            5_516_873,
            19_480_852,
            5_230_134,
            -23_952_439,
            -15_175_766,
        ],
        ym: [
            -30_269_007,
            -3_463_509,
            7_665_486,
            10_083_793,
            28_475_525,
            1_649_722,
            20_654_025,
            16_520_125,
            30_598_449,
            7_715_701,
        ],
        t2: [
            28_881_845,
            14_381_568,
            9_657_904,
            3_680_757,
            -20_181_635,
            7_843_316,
            -31_400_660,
            1_370_708,
            29_794_553,
            -1_409_300,
        ],
    },
    Precomp {
        yp: [
            -22_518_993,
            -6_692_182,
            14_201_702,
            -8_745_502,
            -23_510_406,
            8_844_726,
            18_474_211,
            -1_361_450,
            -13_062_696,
            13_821_877,
        ],
        ym: [
            -6_455_177,
            -7_839_871,
            3_374_702,
            -4_740_862,
            -27_098_617,
            -10_571_707,
            31_655_028,
            -7_212_327,
            18_853_322,
            -14_220_951,
        ],
        t2: [
            4_566_830,
            -12_963_868,
            -28_974_889,
            -12_240_689,
            -7_602_672,
            -2_830_569,
            -8_514_358,
            -10_431_137,
            2_207_753,
            -3_209_784,
        ],
    },
    Precomp {
        yp: [
            -25_154_831,
            -4_185_821,
            29_681_144,
            7_868_801,
            -6_854_661,
            -9_423_865,
            -12_437_364,
            -663_000,
            -31_111_463,
            -16_132_436,
        ],
        ym: [
            25_576_264,
            -2_703_214,
            7_349_804,
            -11_814_844,
            16_472_782,
            9_300_885,
            3_844_789,
            15_725_684,
            171_356,
            6_466_918,
        ],
        t2: [
            23_103_977,
            13_316_479,
            9_739_013,
            -16_149_481,
            817_875,
            -15_038_942,
            8_965_339,
            -14_088_058,
            -30_714_912,
            16_193_877,
        ],
    },
    Precomp {
        yp: [
            -33_521_811,
            3_180_713,
            -2_394_130,
            14_003_687,
            -16_903_474,
            -16_270_840,
            17_238_398,
            4_729_455,
            -18_074_513,
            9_256_800,
        ],
        ym: [
            -25_182_317,
            -4_174_131,
            32_336_398,
            5_036_987,
            -21_236_817,
            11_360_617,
            22_616_405,
            9_761_698,
            -19_827_198,
            630_305,
        ],
        t2: [
            -13_720_693,
            2_639_453,
            -24_237_460,
            -7_406_481,
            9_494_427,
            -5_774_029,
            -6_554_551,
            -15_960_994,
            -2_449_256,
            -14_291_300,
        ],
    },
    Precomp {
        yp: [
            -3_151_181,
            -5_046_075,
            9_282_714,
            6_866_145,
            -31_907_062,
            -863_023,
            -18_940_575,
            15_033_784,
            25_105_118,
            -7_894_876,
        ],
        ym: [
            -24_326_370,
            15_950_226,
            -31_801_215,
            -14_592_823,
            -11_662_737,
            -5_090_925,
            1_573_892,
            -2_625_887,
            2_198_790,
            -15_804_619,
        ],
        t2: [
            -3_099_351,
            10_324_967,
            -2_241_613,
            7_453_183,
            -5_446_979,
            -2_735_503,
            -13_812_022,
            -16_236_442,
            -32_461_234,
            -12_290_683,
        ],
    },
];
static B_COMB_LOW: [Precomp; 8] = [
    Precomp {
        yp: [
            -6_816_601,
            -2_324_159,
            -22_559_413,
            124_364,
            18_015_490,
            8_373_481,
            19_993_724,
            1_979_872,
            -18_549_925,
            9_085_059,
        ],
        ym: [
            10_306_321,
            403_248,
            14_839_893,
            9_633_706,
            8_463_310,
            -8_354_981,
            -14_305_673,
            14_668_847,
            26_301_366,
            2_818_560,
        ],
        t2: [
            -22_701_500,
            -3_210_264,
            -13_831_292,
            -2_927_732,
            -16_326_337,
            -14_016_360,
            12_940_910,
            177_905,
            12_165_515,
            -2_397_893,
        ],
    },
    Precomp {
        yp: [
            -12_282_262,
            -7_022_066,
            9_920_413,
            -3_064_358,
            -32_147_467,
            2_927_790,
            22_392_436,
            -14_852_487,
            2_719_975,
            16_402_117,
        ],
        ym: [
            -7_236_961,
            -4_729_776,
            2_685_954,
            -6_525_055,
            -24_242_706,
            -15_940_211,
            -6_238_521,
            14_082_855,
            10_047_669,
            12_228_189,
        ],
        t2: [
            -30_495_588,
            -12_893_761,
            -11_161_261,
            3_539_405,
            -11_502_464,
            16_491_580,
            -27_286_798,
            -15_030_530,
            -7_272_871,
            -15_934_455,
        ],
    },
    Precomp {
        yp: [
            17_650_926,
            582_297,
            -860_412,
            -187_745,
            -12_072_900,
            -10_683_391,
            -20_352_381,
            15_557_840,
            -31_072_141,
            -5_019_061,
        ],
        ym: [
            -6_283_632,
            -2_259_834,
            -4_674_247,
            -4_598_977,
            -4_089_240,
            12_435_688,
            -31_278_303,
            1_060_251,
            6_256_175,
            10_480_726,
        ],
        t2: [
            -13_871_026,
            2_026_300,
            -21_928_428,
            -2_741_605,
            -2_406_664,
            -8_034_988,
            7_355_518,
            15_733_500,
            -23_379_862,
            7_489_131,
        ],
    },
    Precomp {
        yp: [
            6_883_359,
            695_140,
            23_196_907,
            9_644_202,
            -33_430_614,
            11_354_760,
            -20_134_606,
            6_388_313,
            -8_263_585,
            -8_491_918,
        ],
        ym: [
            -7_716_174,
            -13_605_463,
            -13_646_110,
            14_757_414,
            -19_430_591,
            -14_967_316,
            10_359_532,
            -11_059_670,
            -21_935_259,
            12_082_603,
        ],
        t2: [
            -11_253_345,
            -15_943_946,
            10_046_784,
            5_414_629,
            24_840_771,
            8_086_951,
            -6_694_742,
            9_868_723,
            15_842_692,
            -16_224_787,
        ],
    },
    Precomp {
        yp: [
            9_639_399,
            11_810_955,
            -24_007_778,
            -9_320_054,
            3_912_937,
            -9_856_959,
            996_125,
            -8_727_907,
            -8_919_186,
            -14_097_242,
        ],
        ym: [
            7_248_867, 14_468_564, 25_228_636, -8_795_035, 14_346_339, 8_224_790, 6_388_427,
            -7_181_107, 6_468_218, -8_720_783,
        ],
        t2: [
            15_513_115,
            15_439_095,
            7_342_322,
            -10_157_390,
            18_005_294,
            -7_265_713,
            2_186_239,
            4_884_640,
            10_826_567,
            7_135_781,
        ],
    },
    Precomp {
        yp: [
            -14_204_238,
            5_297_536,
            -5_862_318,
            -6_004_934,
            28_095_835,
            4_236_101,
            -14_203_318,
            1_958_636,
            -16_816_875,
            3_837_147,
        ],
        ym: [
            -5_511_166,
            -13_176_782,
            -29_588_215,
            12_339_465,
            15_325_758,
            -15_945_770,
            -8_813_185,
            11_075_932,
            -19_608_050,
            -3_776_283,
        ],
        t2: [
            11_728_032,
            9_603_156,
            -4_637_821,
            -5_304_487,
            -7_827_751,
            2_724_948,
            31_236_191,
            -16_760_175,
            -7_268_616,
            14_799_772,
        ],
    },
    Precomp {
        yp: [
            -28_842_672,
            4_840_636,
            -12_047_946,
            -9_101_456,
            -1_445_464,
            381_905,
            -30_977_094,
            -16_523_389,
            1_290_540,
            12_798_615,
        ],
        ym: [
            27_246_947,
            -10_320_914,
            14_792_098,
            -14_518_944,
            5_302_070,
            -8_746_152,
            -3_403_974,
            -4_149_637,
            -27_061_213,
            10_749_585,
        ],
        t2: [
            25_572_375,
            -6_270_368,
            -15_353_037,
            16_037_944,
            1_146_292,
            32_198,
            23_487_090,
            9_585_613,
            24_714_571,
            -1_418_265,
        ],
    },
    Precomp {
        yp: [
            19_844_825,
            282_124,
            -17_583_147,
            11_004_019,
            -32_004_269,
            -2_716_035,
            6_105_106,
            -1_711_007,
            -21_010_044,
            14_338_445,
        ],
        ym: [
            8_027_505,
            8_191_102,
            -18_504_907,
            -12_335_737,
            25_173_494,
            -5_923_905,
            15_446_145,
            7_483_684,
            -30_440_441,
            10_009_108,
        ],
        t2: [
            -14_134_701,
            -4_174_411,
            10_246_585,
            -14_677_495,
            33_553_567,
            -14_012_935,
            23_366_126,
            15_080_531,
            -7_969_992,
            7_663_473,
        ],
    },
];
static B_COMB_HIGH: [Precomp; 8] = [
    Precomp {
        yp: [
            33_055_887, -4_431_773, -521_787, 6_654_165, 951_411, -6_266_464, -5_158_124,
            6_995_613, -5_397_442, -6_985_227,
        ],
        ym: [
            4_014_062,
            6_967_095,
            -11_977_872,
            3_960_002,
            8_001_989,
            5_130_302,
            -2_154_812,
            -1_899_602,
            -31_954_493,
            -16_173_976,
        ],
        t2: [
            16_271_757,
            -9_212_948,
            23_792_794,
            731_486,
            -25_808_309,
            -3_546_396,
            6_964_344,
            -4_767_590,
            10_976_593,
            10_050_757,
        ],
    },
    Precomp {
        yp: [
            2_533_007,
            -4_288_439,
            -24_467_768,
            -12_387_405,
            -13_450_051,
            14_542_280,
            12_876_301,
            13_893_535,
            15_067_764,
            8_594_792,
        ],
        ym: [
            20_073_501,
            -11_623_621,
            3_165_391,
            -13_119_866,
            13_188_608,
            -11_540_496,
            -10_751_437,
            -13_482_671,
            29_588_810,
            2_197_295,
        ],
        t2: [
            -1_084_082,
            11_831_693,
            6_031_797,
            14_062_724,
            14_748_428,
            -8_159_962,
            -20_721_760,
            11_742_548,
            31_368_706,
            13_161_200,
        ],
    },
    Precomp {
        yp: [
            2_050_412,
            -6_457_589,
            15_321_215,
            5_273_360,
            25_484_180,
            124_590,
            -18_187_548,
            -7_097_255,
            -6_691_621,
            -14_604_792,
        ],
        ym: [
            9_938_196,
            2_162_889,
            -6_158_074,
            -1_711_248,
            4_278_932,
            -2_598_531,
            -22_865_792,
            -7_168_500,
            -24_323_168,
            11_746_309,
        ],
        t2: [
            -22_691_768,
            -14_268_164,
            5_965_485,
            9_383_325,
            20_443_693,
            5_854_192,
            28_250_679,
            -1_381_811,
            -10_837_134,
            13_717_818,
        ],
    },
    Precomp {
        yp: [
            -8_495_530,
            16_382_250,
            9_548_884,
            -4_971_523,
            -4_491_811,
            -3_902_147,
            6_182_256,
            -12_832_479,
            26_628_081,
            10_395_408,
        ],
        ym: [
            27_329_048,
            -15_853_735,
            7_715_764,
            8_717_446,
            -9_215_518,
            -14_633_480,
            28_982_250,
            -5_668_414,
            4_227_628,
            242_148,
        ],
        t2: [
            -13_279_943,
            -7_986_904,
            -7_100_016,
            8_764_468,
            -27_276_630,
            3_096_719,
            29_678_419,
            -9_141_299,
            3_906_709,
            11_265_498,
        ],
    },
    Precomp {
        yp: [
            11_918_285,
            15_686_328,
            -17_757_323,
            -11_217_300,
            -27_548_967,
            4_853_165,
            -27_168_827,
            6_807_359,
            6_871_949,
            -1_075_745,
        ],
        ym: [
            -29_002_610,
            13_984_323,
            -27_111_812,
            -2_713_442,
            28_107_359,
            -13_266_203,
            6_155_126,
            15_104_658,
            3_538_727,
            -7_513_788,
        ],
        t2: [
            14_103_158,
            11_233_913,
            -33_165_269,
            9_279_850,
            31_014_152,
            4_335_090,
            -1_827_936,
            4_590_951,
            13_960_841,
            12_787_712,
        ],
    },
    Precomp {
        yp: [
            1_469_134,
            -16_738_009,
            33_411_928,
            13_942_824,
            8_092_558,
            -8_778_224,
            -11_165_065,
            1_437_842,
            22_521_552,
            -2_792_954,
        ],
        ym: [
            31_352_705,
            -4_807_352,
            -25_327_300,
            3_962_447,
            12_541_566,
            -9_399_651,
            -27_425_693,
            7_964_818,
            -23_829_869,
            5_541_287,
        ],
        t2: [
            -25_732_021,
            -6_864_887,
            23_848_984,
            3_039_395,
            -9_147_354,
            6_022_816,
            -27_421_653,
            10_590_137,
            25_309_915,
            -1_584_678,
        ],
    },
    Precomp {
        yp: [
            -22_951_376,
            5_048_948,
            31_139_401,
            -190_316,
            -19_542_447,
            -626_310,
            -17_486_305,
            -16_511_925,
            -18_851_313,
            -12_985_140,
        ],
        ym: [
            -9_684_890,
            14_681_754,
            30_487_568,
            7_717_771,
            -10_829_709,
            9_630_497,
            30_290_549,
            -10_531_496,
            -27_798_994,
            -13_812_825,
        ],
        t2: [
            5_827_835,
            16_097_107,
            -24_501_327,
            12_094_619,
            7_413_972,
            11_447_087,
            28_057_551,
            -1_793_987,
            -14_056_981,
            4_359_312,
        ],
    },
    Precomp {
        yp: [
            26_323_183,
            2_342_588,
            -21_887_793,
            -1_623_758,
            -6_062_284,
            2_107_090,
            -28_724_907,
            9_036_464,
            -19_618_351,
            -13_055_189,
        ],
        ym: [
            -29_697_200,
            14_829_398,
            -4_596_333,
            14_220_089,
            -30_022_969,
            2_955_645,
            12_094_100,
            -13_693_652,
            -5_941_445,
            7_047_569,
        ],
        t2: [
            -3_201_977,
            14_413_268,
            -12_058_324,
            -16_417_589,
            -9_035_655,
            -7_224_648,
            9_258_160,
            1_399_236,
            30_397_584,
            -5_684_634,
        ],
    },
];
const L: [u32; 8] = [
    0x5cf5_d3ed,
    0x5812_631a,
    0xa2f7_9cd6,
    0x14de_f9de,
    0,
    0,
    0,
    0x1000_0000,
];
fn word(p: *const u32, i: usize) -> u32 {
    unsafe { p.add(i).read() }
}
fn set(p: *mut u32, i: usize, v: u32) {
    unsafe {
        p.add(i).write(v);
    }
}
fn zero(p: *mut u32, n: usize) {
    for i in 0..n {
        set(p, i, 0);
    }
}
fn load_words(p: *mut u32, s: *const u8, n: usize) {
    for i in 0..n {
        set(p, i, load(unsafe { s.add(4 * i) }, 4));
    }
}
fn store_words(s: *mut u8, p: *const u32, n: usize) {
    for i in 0..n {
        store(unsafe { s.add(4 * i) }, word(p, i));
    }
}
fn copy_bytes(out: *mut u8, input: *const u8, n: usize) {
    for i in 0..n {
        unsafe {
            out.add(i).write(input.add(i).read());
        }
    }
}
fn multiply(p: *mut u32, a: *const u32, b: *const u32) {
    for i in 0..8 {
        let mut carry = 0_u64;
        for j in 0..8 {
            carry = carry
                .wrapping_add(u64::from(word(p, i + j)))
                .wrapping_add(u64::from(word(a, i)) * u64::from(word(b, j)));
            set(p, i + j, carry as u32);
            carry >>= 32;
        }
        set(p, i + 8, carry as u32);
    }
}
fn is_above_l(x: *const u32) -> u32 {
    let mut carry = 1_u64;
    for i in 0..8 {
        carry = carry
            .wrapping_add(u64::from(word(x, i)))
            .wrapping_add(u64::from(!L[i]));
        carry >>= 32;
    }
    carry as u32
}
fn remove_l(r: *mut u32, x: *const u32) {
    let mut carry = u64::from(is_above_l(x));
    let mask = (!(carry as u32)).wrapping_add(1);
    for i in 0..8 {
        carry = carry
            .wrapping_add(u64::from(word(x, i)))
            .wrapping_add(u64::from(!L[i] & mask));
        set(r, i, carry as u32);
        carry >>= 32;
    }
}
fn mod_l(ops: &Leaves, reduced: *mut u8, x: *const u32) {
    const R: [u32; 9] = [
        0x0a2c_131b,
        0xed9c_e5a3,
        0x0863_29a7,
        0x2106_215d,
        0xffff_ffeb,
        0xffff_ffff,
        0xffff_ffff,
        0xffff_ffff,
        0xf,
    ];
    words!(xr, xr_storage, 25);
    zero(xr, 25);
    for i in 0..9 {
        let mut carry = 0_u64;
        for j in 0..16 {
            carry = carry
                .wrapping_add(u64::from(word(xr, i + j)))
                .wrapping_add(u64::from(R[i]) * u64::from(word(x, j)));
            set(xr, i + j, carry as u32);
            carry >>= 32;
        }
        set(xr, i + 16, carry as u32);
    }
    zero(xr, 8);
    for i in 0..8 {
        let mut carry = 0_u64;
        for j in 0..8 - i {
            carry = carry
                .wrapping_add(u64::from(word(xr, i + j)))
                .wrapping_add(u64::from(word(xr, i + 16)) * u64::from(L[j]));
            set(xr, i + j, carry as u32);
            carry >>= 32;
        }
    }
    let mut carry = 1_u64;
    for i in 0..8 {
        carry = carry
            .wrapping_add(u64::from(word(x, i)))
            .wrapping_add(u64::from(!word(xr, i)));
        set(xr, i, carry as u32);
        carry >>= 32;
    }
    remove_l(xr, xr);
    store_words(reduced, xr, 8);
    wipe(ops, xr, 100);
}
fn reduce(ops: &Leaves, reduced: *mut u8, expanded: *const u8) {
    words!(x, x_storage, 16);
    load_words(x, expanded, 16);
    mod_l(ops, reduced, x);
    wipe(ops, x, 64);
}
fn mul_add(ops: &Leaves, r: *mut u8, a: *const u8, b: *const u8, c: *const u8) {
    words!(aa, a_storage, 8);
    words!(bb, b_storage, 8);
    words!(p, p_storage, 16);
    load_words(aa, a, 8);
    load_words(bb, b, 8);
    load_words(p, c, 8);
    zero(unsafe { p.add(8) }, 8);
    multiply(p, aa, bb);
    mod_l(ops, r, p);
    wipe(ops, p, 64);
    wipe(ops, aa, 32);
    wipe(ops, bb, 32);
}
fn scalar_bit(s: *const u8, i: i32) -> i32 {
    if i < 0 {
        0
    } else {
        i32::from((unsafe { s.add((i >> 3) as usize).read() } >> (i & 7)) & 1)
    }
}
fn fe_isodd(ops: &Leaves, p: *const i32) -> i32 {
    bytes!(s, s_storage, 32);
    fe_tobytes(ops, s, p);
    let odd = i32::from(unsafe { s.read() } & 1);
    wipe(ops, s, 32);
    odd
}
fn ge_zero(p: *mut Ge) {
    fe_0(f!(p, x));
    fe_1(f!(p, y));
    fe_1(f!(p, z));
    fe_0(f!(p, t));
}
fn ge_tobytes(ops: &Leaves, s: *mut u8, p: *const Ge) {
    field!(recip, recip_storage);
    field!(x, x_storage);
    field!(y, y_storage);
    fe_invert(ops, recip, f!(p, z));
    fe_mul(x, f!(p, x), recip);
    fe_mul(y, f!(p, y), recip);
    fe_tobytes(ops, s, y);
    let sign = fe_isodd(ops, x) as u8;
    unsafe {
        s.add(31).write(s.add(31).read() ^ (sign << 7));
    }
    wipe(ops, recip, 40);
    wipe(ops, x, 40);
    wipe(ops, y, 40);
}
// Variable-time decoder has only public inputs. Preserve decode/failure order.
fn ge_frombytes_neg(ops: &Leaves, h: *mut Ge, s: *const u8) -> i32 {
    fe_frombytes(f!(h, y), s);
    fe_1(f!(h, z));
    fe_sq(f!(h, t), f!(h, y));
    fe_mul(f!(h, x), f!(h, t), D.as_ptr());
    fe_sub(f!(h, t), f!(h, t), f!(h, z));
    fe_add(f!(h, x), f!(h, x), f!(h, z));
    fe_mul(f!(h, x), f!(h, t), f!(h, x));
    let square = invsqrt(ops, f!(h, x), f!(h, x));
    if square == 0 {
        return -1;
    }
    fe_mul(f!(h, x), f!(h, t), f!(h, x));
    if fe_isodd(ops, f!(h, x)) == i32::from(unsafe { s.add(31).read() } >> 7) {
        fe_neg(f!(h, x), f!(h, x));
    }
    fe_mul(f!(h, t), f!(h, x), f!(h, y));
    0
}
fn ge_cache(c: *mut Cached, p: *const Ge) {
    fe_add(f!(c, yp), f!(p, y), f!(p, x));
    fe_sub(f!(c, ym), f!(p, y), f!(p, x));
    fe_copy(f!(c, z), f!(p, z));
    fe_mul(f!(c, t2), f!(p, t), D2.as_ptr());
}
fn ge_add(s: *mut Ge, p: *const Ge, q: *const Cached) {
    field!(a, a_storage);
    field!(b, b_storage);
    fe_add(a, f!(p, y), f!(p, x));
    fe_sub(b, f!(p, y), f!(p, x));
    fe_mul(a, a, f!(q, yp));
    fe_mul(b, b, f!(q, ym));
    fe_add(f!(s, y), a, b);
    fe_sub(f!(s, x), a, b);
    fe_add(f!(s, z), f!(p, z), f!(p, z));
    fe_mul(f!(s, z), f!(s, z), f!(q, z));
    fe_mul(f!(s, t), f!(p, t), f!(q, t2));
    fe_add(a, f!(s, z), f!(s, t));
    fe_sub(b, f!(s, z), f!(s, t));
    fe_mul(f!(s, t), f!(s, x), f!(s, y));
    fe_mul(f!(s, x), f!(s, x), b);
    fe_mul(f!(s, y), f!(s, y), a);
    fe_mul(f!(s, z), a, b);
}
fn ge_sub(s: *mut Ge, p: *const Ge, q: *const Cached) {
    local!(neg, neg_storage, Cached);
    fe_copy(f!(neg, ym), f!(q, yp));
    fe_copy(f!(neg, yp), f!(q, ym));
    fe_copy(f!(neg, z), f!(q, z));
    fe_neg(f!(neg, t2), f!(q, t2));
    ge_add(s, p, neg);
}
fn ge_madd(s: *mut Ge, p: *const Ge, q: *const Precomp, a: *mut i32, b: *mut i32) {
    fe_add(a, f!(p, y), f!(p, x));
    fe_sub(b, f!(p, y), f!(p, x));
    fe_mul(a, a, f!(q, yp));
    fe_mul(b, b, f!(q, ym));
    fe_add(f!(s, y), a, b);
    fe_sub(f!(s, x), a, b);
    fe_add(f!(s, z), f!(p, z), f!(p, z));
    fe_mul(f!(s, t), f!(p, t), f!(q, t2));
    fe_add(a, f!(s, z), f!(s, t));
    fe_sub(b, f!(s, z), f!(s, t));
    fe_mul(f!(s, t), f!(s, x), f!(s, y));
    fe_mul(f!(s, x), f!(s, x), b);
    fe_mul(f!(s, y), f!(s, y), a);
    fe_mul(f!(s, z), a, b);
}
fn ge_msub(s: *mut Ge, p: *const Ge, q: *const Precomp, a: *mut i32, b: *mut i32) {
    local!(neg, neg_storage, Precomp);
    fe_copy(f!(neg, ym), f!(q, yp));
    fe_copy(f!(neg, yp), f!(q, ym));
    fe_neg(f!(neg, t2), f!(q, t2));
    ge_madd(s, p, neg, a, b);
}
fn ge_double(s: *mut Ge, p: *const Ge, q: *mut Ge) {
    fe_sq(f!(q, x), f!(p, x));
    fe_sq(f!(q, y), f!(p, y));
    fe_sq(f!(q, z), f!(p, z));
    fe_mul_small(f!(q, z), f!(q, z), 2);
    fe_add(f!(q, t), f!(p, x), f!(p, y));
    fe_sq(f!(s, t), f!(q, t));
    fe_add(f!(q, t), f!(q, y), f!(q, x));
    fe_sub(f!(q, y), f!(q, y), f!(q, x));
    fe_sub(f!(q, x), f!(s, t), f!(q, t));
    fe_sub(f!(q, z), f!(q, z), f!(q, y));
    fe_mul(f!(s, x), f!(q, x), f!(q, z));
    fe_mul(f!(s, y), f!(q, t), f!(q, y));
    fe_mul(f!(s, z), f!(q, y), f!(q, z));
    fe_mul(f!(s, t), f!(q, x), f!(q, t));
}
struct Slide {
    index: i16,
    digit: i8,
    check: u8,
}
fn slide_init(s: *const u8) -> Slide {
    let mut i = 252;
    while i > 0 && scalar_bit(s, i) == 0 {
        i -= 1;
    }
    Slide {
        index: -1,
        digit: -1,
        check: (i + 1) as u8,
    }
}
fn slide_step(ctx: &mut Slide, width: i32, i: i32, s: *const u8) -> i32 {
    if i == i32::from(ctx.check) {
        if scalar_bit(s, i) == scalar_bit(s, i - 1) {
            ctx.check = ctx.check.wrapping_sub(1);
        } else {
            let w = width.min(i + 1);
            let mut v = -(scalar_bit(s, i) << (w - 1));
            for j in 0..w - 1 {
                v += scalar_bit(s, i - (w - 1) + j) << j;
            }
            v += scalar_bit(s, i - w);
            let lsb = v & (!v + 1);
            let shift = i32::from(lsb & 0xaa != 0)
                | (i32::from(lsb & 0xcc != 0) << 1)
                | (i32::from(lsb & 0xf0 != 0) << 2);
            ctx.index = (i - (w - 1) + shift) as i16;
            ctx.digit = (v >> shift) as i8;
            ctx.check = ctx.check.wrapping_sub(w as u8);
        }
    }
    if i == i32::from(ctx.index) {
        i32::from(ctx.digit)
    } else {
        0
    }
}
fn check_equation(ops: &Leaves, signature: *const u8, public_key: *const u8, h: *const u8) -> i32 {
    local!(minus_a, a_storage, Ge);
    local!(minus_r, r_storage, Ge);
    let s = unsafe { signature.add(32) };
    words!(s32, s_storage, 8);
    load_words(s32, s, 8);
    if ge_frombytes_neg(ops, minus_a, public_key) != 0
        || ge_frombytes_neg(ops, minus_r, signature) != 0
        || is_above_l(s32) != 0
    {
        return -1;
    }
    local!(lut_storage, lut_raw_storage, [Cached; 2]);
    let lut = lut_storage.cast::<Cached>();
    {
        local!(a2, a2_storage, Ge);
        local!(tmp, tmp_storage, Ge);
        ge_double(a2, minus_a, tmp);
        ge_cache(lut, minus_a);
        ge_add(tmp, a2, lut);
        ge_cache(unsafe { lut.add(1) }, tmp);
    }
    let mut h_slide = slide_init(h);
    let mut s_slide = slide_init(s);
    let mut i = i32::from(h_slide.check.max(s_slide.check));
    let sum = minus_a;
    ge_zero(sum);
    while i >= 0 {
        local!(tmp, tmp_storage, Ge);
        ge_double(sum, sum, tmp);
        let hd = slide_step(&mut h_slide, 3, i, h);
        let sd = slide_step(&mut s_slide, 5, i, s);
        if hd > 0 {
            ge_add(sum, sum, unsafe { lut.add((hd / 2) as usize) });
        }
        if hd < 0 {
            ge_sub(sum, sum, unsafe { lut.add((-hd / 2) as usize) });
        }
        field!(t1, t1_storage);
        field!(t2, t2_storage);
        if sd > 0 {
            ge_madd(
                sum,
                sum,
                unsafe { B_WINDOW.as_ptr().add((sd / 2) as usize) },
                t1,
                t2,
            );
        }
        if sd < 0 {
            ge_msub(
                sum,
                sum,
                unsafe { B_WINDOW.as_ptr().add((-sd / 2) as usize) },
                t1,
                t2,
            );
        }
        i -= 1;
    }
    local!(cached, cached_storage, Cached);
    bytes!(check, check_storage, 32);
    let mut zero_point = [0_u8; 32];
    zero_point[0] = 1;
    ge_cache(cached, minus_r);
    ge_add(sum, sum, cached);
    ge_double(sum, sum, minus_r);
    ge_double(sum, sum, minus_r);
    ge_double(sum, sum, minus_r);
    ge_tobytes(ops, check, sum);
    unsafe { (ops.verify32)(check, zero_point.as_ptr()) }
}
fn lookup_add(
    p: *mut Ge,
    tmp_c: *mut Precomp,
    tmp_a: *mut i32,
    tmp_b: *mut i32,
    comb: *const Precomp,
    scalar: *const u8,
    i: i32,
) {
    let teeth = (scalar_bit(scalar, i)
        + (scalar_bit(scalar, i + 32) << 1)
        + (scalar_bit(scalar, i + 64) << 2)
        + (scalar_bit(scalar, i + 96) << 3)) as u8;
    let high = teeth >> 3;
    let index = (i32::from(teeth) ^ (i32::from(high) - 1)) & 7;
    for j in 0..8 {
        let select = 1 & (sub32(j ^ index, 1) >> 8);
        let row = unsafe { comb.add(j as usize) };
        fe_ccopy(f!(tmp_c, yp), f!(row, yp), select);
        fe_ccopy(f!(tmp_c, ym), f!(row, ym), select);
        fe_ccopy(f!(tmp_c, t2), f!(row, t2), select);
    }
    fe_neg(tmp_a, f!(tmp_c, t2));
    fe_cswap(f!(tmp_c, t2), tmp_a, i32::from(high ^ 1));
    fe_cswap(f!(tmp_c, yp), f!(tmp_c, ym), i32::from(high ^ 1));
    ge_madd(p, p, tmp_c, tmp_a, tmp_b);
}
fn ge_scalarmult_base(ops: &Leaves, p: *mut Ge, scalar: *const u8) {
    const HALF_L: [u8; 32] = [
        247, 233, 122, 46, 141, 49, 9, 44, 107, 206, 123, 81, 239, 124, 111, 10, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 8,
    ];
    const HALF_ONES: [u8; 32] = [
        142, 74, 204, 70, 186, 24, 118, 107, 184, 231, 190, 57, 250, 173, 119, 99, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 7,
    ];
    bytes!(s, s_storage, 32);
    mul_add(ops, s, scalar, HALF_L.as_ptr(), HALF_ONES.as_ptr());
    field!(a, a_storage);
    field!(b, b_storage);
    local!(c, c_storage, Precomp);
    local!(d, d_storage, Ge);
    fe_1(f!(c, yp));
    fe_1(f!(c, ym));
    fe_0(f!(c, t2));
    ge_zero(p);
    lookup_add(p, c, a, b, B_COMB_LOW.as_ptr(), s, 31);
    lookup_add(p, c, a, b, B_COMB_HIGH.as_ptr(), s, 159);
    for i in (0..31).rev() {
        ge_double(p, p, d);
        lookup_add(p, c, a, b, B_COMB_LOW.as_ptr(), s, i);
        lookup_add(p, c, a, b, B_COMB_HIGH.as_ptr(), s, i + 128);
    }
    wipe(ops, a, 40);
    wipe(ops, d, size_of::<Ge>());
    wipe(ops, b, 40);
    wipe(ops, c, size_of::<Precomp>());
    wipe(ops, s, 32);
}
fn scalarbase(ops: &Leaves, out: *mut u8, scalar: *const u8) {
    local!(p, p_storage, Ge);
    ge_scalarmult_base(ops, p, scalar);
    ge_tobytes(ops, out, p);
    wipe(ops, p, size_of::<Ge>());
}
fn hash(ops: &Leaves, out: *mut u8, chunks: &[(*const u8, usize)]) {
    crate::blake2b::native_hash(ops.wipe, out, chunks);
}
fn hash_reduce(ops: &Leaves, out: *mut u8, chunks: &[(*const u8, usize)]) {
    bytes!(h, h_storage, 64);
    hash(ops, h, chunks);
    reduce(ops, out, h); /* original hash buffer is not explicitly wiped */
}
fn key_pair(ops: &Leaves, secret: *mut u8, public: *mut u8, seed: *mut u8) {
    bytes!(a, a_storage, 64);
    copy_bytes(a, seed, 32);
    wipe(ops, seed, 32);
    copy_bytes(secret, a, 32);
    hash(ops, a, &[(a, 32)]);
    trim(a, a);
    scalarbase(ops, unsafe { secret.add(32) }, a);
    copy_bytes(public, unsafe { secret.add(32) }, 32);
    wipe(ops, a, 64);
}
fn sign(ops: &Leaves, signature: *mut u8, secret: *const u8, message: *const u8, size: usize) {
    bytes!(a, a_storage, 64);
    bytes!(r, r_storage, 32);
    bytes!(h, h_storage, 32);
    bytes!(rr, rr_storage, 32);
    hash(ops, a, &[(secret, 32)]);
    trim(a, a);
    hash_reduce(
        ops,
        r,
        &[(unsafe { a.add(32) }, 32), (message, size), (null(), 0)],
    );
    scalarbase(ops, rr, r);
    hash_reduce(
        ops,
        h,
        &[(rr, 32), (unsafe { secret.add(32) }, 32), (message, size)],
    );
    copy_bytes(signature, rr, 32);
    mul_add(ops, unsafe { signature.add(32) }, h, a, r);
    wipe(ops, a, 64);
    wipe(ops, r, 32);
}
fn check(
    ops: &Leaves,
    signature: *const u8,
    public: *const u8,
    message: *const u8,
    size: usize,
) -> i32 {
    bytes!(h, h_storage, 32);
    hash_reduce(ops, h, &[(signature, 32), (public, 32), (message, size)]);
    check_equation(ops, signature, public, h)
}
fn conversion(ops: &Leaves, out: *mut u8, input: *const u8, to_montgomery: bool) {
    field!(t1, t1_storage);
    field!(t2, t2_storage);
    fe_frombytes(t2, input);
    if to_montgomery {
        fe_add(t1, ONE.as_ptr(), t2);
        fe_sub(t2, ONE.as_ptr(), t2);
    } else {
        fe_sub(t1, t2, ONE.as_ptr());
        fe_add(t2, t2, ONE.as_ptr());
    }
    fe_invert(ops, t2, t2);
    fe_mul(t1, t1, t2);
    fe_tobytes(ops, out, t1);
    wipe(ops, t1, 40);
    wipe(ops, t2, 40);
}
fn add_xl(s: *mut u8, x: u8) {
    let mod8 = u64::from(x & 7);
    let mut carry = 0_u64;
    for i in 0..8 {
        carry = carry
            .wrapping_add(u64::from(load(unsafe { s.add(4 * i) }, 4)))
            .wrapping_add(u64::from(L[i]) * mod8);
        store(unsafe { s.add(4 * i) }, carry as u32);
        carry >>= 32;
    }
}
fn dirty_small(ops: &Leaves, public: *mut u8, secret: *const u8) {
    const DIRTY: [u8; 32] = [
        0xd8, 0x86, 0x1a, 0xa2, 0x78, 0x7a, 0xd9, 0x26, 0x8b, 0x74, 0x74, 0xb6, 0x82, 0xe3, 0xbe,
        0xc3, 0xce, 0x36, 0x9a, 0x1e, 0x5e, 0x31, 0x47, 0xa2, 0x6d, 0x37, 0x7c, 0xfd, 0x20, 0xb5,
        0xdf, 0x75,
    ];
    bytes!(scalar, scalar_storage, 32);
    trim(scalar, secret);
    add_xl(scalar, unsafe { secret.read() });
    scalarmult(ops, public, scalar, DIRTY.as_ptr(), 256);
    wipe(ops, scalar, 32);
}
fn select_lop(ops: &Leaves, out: *mut i32, x: *const i32, k: *const i32, cofactor: u8) {
    field!(tmp, tmp_storage);
    fe_0(out);
    fe_ccopy(out, k, i32::from((cofactor >> 1) & 1));
    fe_ccopy(out, x, i32::from(cofactor & 1));
    fe_neg(tmp, out);
    fe_ccopy(out, tmp, i32::from((cofactor >> 2) & 1));
    wipe(ops, tmp, 40);
}
fn dirty_fast(ops: &Leaves, public: *mut u8, secret: *const u8) {
    bytes!(scalar, scalar_storage, 32);
    local!(pk, pk_storage, Ge);
    trim(scalar, secret);
    ge_scalarmult_base(ops, pk, scalar);
    field!(t1, t1_storage);
    field!(t2, t2_storage);
    let cofactor = unsafe { secret.read() };
    select_lop(ops, t1, LOP_X.as_ptr(), SQRTM1.as_ptr(), cofactor);
    select_lop(
        ops,
        t2,
        LOP_Y.as_ptr(),
        ONE.as_ptr(),
        cofactor.wrapping_add(2),
    );
    local!(low, low_storage, Precomp);
    fe_add(f!(low, yp), t2, t1);
    fe_sub(f!(low, ym), t2, t1);
    fe_mul(f!(low, t2), t2, t1);
    fe_mul(f!(low, t2), f!(low, t2), D2.as_ptr());
    ge_madd(pk, pk, low, t1, t2);
    fe_add(t1, f!(pk, z), f!(pk, y));
    fe_sub(t2, f!(pk, z), f!(pk, y));
    fe_invert(ops, t2, t2);
    fe_mul(t1, t1, t2);
    fe_tobytes(ops, public, t1);
    wipe(ops, t1, 40);
    wipe(ops, pk, size_of::<Ge>());
    wipe(ops, t2, 40);
    wipe(ops, low, size_of::<Precomp>());
    wipe(ops, scalar, 32);
}
fn elligator_map(ops: &Leaves, curve: *mut u8, hidden: *const u8) {
    field!(r, r_storage);
    field!(u, u_storage);
    field!(t1, t1_storage);
    field!(t2, t2_storage);
    field!(t3, t3_storage);
    fe_frombytes_mask(r, hidden, 2);
    fe_sq(r, r);
    fe_add(t1, r, r);
    fe_add(u, t1, ONE.as_ptr());
    fe_sq(t2, u);
    fe_mul(t3, A2.as_ptr(), t1);
    fe_sub(t3, t3, t2);
    fe_mul(t3, t3, A.as_ptr());
    fe_mul(t1, t2, u);
    fe_mul(t1, t3, t1);
    let square = invsqrt(ops, t1, t1);
    fe_mul(u, r, UFACTOR.as_ptr());
    fe_ccopy(u, ONE.as_ptr(), square);
    fe_sq(t1, t1);
    fe_mul(u, u, A.as_ptr());
    fe_mul(u, u, t3);
    fe_mul(u, u, t2);
    fe_mul(u, u, t1);
    fe_neg(u, u);
    fe_tobytes(ops, curve, u);
    wipe(ops, t1, 40);
    wipe(ops, r, 40);
    wipe(ops, t2, 40);
    wipe(ops, u, 40);
    wipe(ops, t3, 40);
}
fn elligator_rev(ops: &Leaves, hidden: *mut u8, public: *const u8, tweak: u8) -> i32 {
    field!(t1, t1_storage);
    field!(t2, t2_storage);
    field!(t3, t3_storage);
    fe_frombytes(t1, public);
    fe_add(t2, t1, A.as_ptr());
    fe_mul(t3, t1, t2);
    fe_mul_small(t3, t3, -2);
    let square = invsqrt(ops, t3, t3);
    if square != 0 {
        fe_ccopy(t1, t2, i32::from(tweak & 1));
        fe_mul(t3, t1, t3);
        fe_mul_small(t1, t3, 2);
        fe_neg(t2, t3);
        let odd = fe_isodd(ops, t1);
        fe_ccopy(t3, t2, odd);
        fe_tobytes(ops, hidden, t3);
        unsafe {
            hidden.add(31).write(hidden.add(31).read() | (tweak & 0xc0));
        }
    }
    wipe(ops, t1, 40);
    wipe(ops, t2, 40);
    wipe(ops, t3, 40);
    square - 1
}
fn elligator_key_pair(ops: &Leaves, hidden: *mut u8, secret: *mut u8, seed: *mut u8) {
    bytes!(pk, pk_storage, 32);
    bytes!(buf, buf_storage, 64);
    copy_bytes(unsafe { buf.add(32) }, seed, 32);
    let nonce = [0_u8; 8];
    loop {
        crate::crypto_primitives::djb_with_wipe(
            ops.wipe,
            buf,
            null(),
            64,
            unsafe { buf.add(32) },
            nonce.as_ptr(),
            0,
        );
        dirty_fast(ops, pk, buf);
        if elligator_rev(ops, unsafe { buf.add(32) }, pk, unsafe {
            buf.add(32).read()
        }) == 0
        {
            break;
        }
    }
    wipe(ops, seed, 32);
    copy_bytes(hidden, unsafe { buf.add(32) }, 32);
    copy_bytes(secret, buf, 32);
    wipe(ops, buf, 64);
    wipe(ops, pk, 32);
}
fn redc(ops: &Leaves, u: *mut u32, x: *const u32) {
    const K: [u32; 8] = [
        0x1254_7e1b,
        0xd2b5_1da3,
        0xfdba_84ff,
        0xb1a2_06f2,
        0xffa3_6bea,
        0x14e7_5438,
        0x6fe9_1836,
        0x9db6_c6f2,
    ];
    words!(s, s_storage, 8);
    zero(s, 8);
    for i in 0..8 {
        let mut carry = 0_u64;
        for j in 0..8 - i {
            carry = carry
                .wrapping_add(u64::from(word(s, i + j)))
                .wrapping_add(u64::from(word(x, i)) * u64::from(K[j]));
            set(s, i + j, carry as u32);
            carry >>= 32;
        }
    }
    words!(t, t_storage, 16);
    zero(t, 16);
    multiply(t, s, L.as_ptr());
    let mut carry = 0_u64;
    for i in 0..16 {
        carry = carry
            .wrapping_add(u64::from(word(t, i)))
            .wrapping_add(u64::from(word(x, i)));
        set(t, i, carry as u32);
        carry >>= 32;
    }
    remove_l(u, unsafe { t.add(8) });
    wipe(ops, s, 32);
    wipe(ops, t, 64);
}
fn inverse(ops: &Leaves, out: *mut u8, private: *const u8, point: *const u8) {
    const LM2: [u8; 32] = [
        0xeb, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde,
        0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10,
    ];
    const M_INV: [u32; 8] = [
        0x8d98_951d,
        0xd6ec_3174,
        0x737d_cf70,
        0xc6ef_5bf4,
        0xffff_fffe,
        0xffff_ffff,
        0xffff_ffff,
        0x0fff_ffff,
    ];
    words!(m_inv, inv_storage, 8);
    for i in 0..8 {
        set(m_inv, i, M_INV[i]);
    }
    bytes!(scalar, scalar_storage, 32);
    trim(scalar, private);
    words!(m_scl, scl_storage, 8);
    {
        words!(tmp, tmp_storage, 16);
        zero(tmp, 8);
        load_words(unsafe { tmp.add(8) }, scalar, 8);
        mod_l(ops, scalar, tmp);
        load_words(m_scl, scalar, 8);
        wipe(ops, tmp, 64);
    }
    words!(product, product_storage, 16);
    for i in (0..253).rev() {
        zero(product, 16);
        multiply(product, m_inv, m_inv);
        redc(ops, m_inv, product);
        if scalar_bit(LM2.as_ptr(), i) != 0 {
            zero(product, 16);
            multiply(product, m_inv, m_scl);
            redc(ops, m_inv, product);
        }
    }
    for i in 0..8 {
        set(product, i, word(m_inv, i));
    }
    zero(unsafe { product.add(8) }, 8);
    redc(ops, m_inv, product);
    store_words(scalar, m_inv, 8);
    add_xl(scalar, unsafe { scalar.read() }.wrapping_mul(3));
    scalarmult(ops, out, scalar, point, 256);
    wipe(ops, scalar, 32);
    wipe(ops, m_scl, 32);
    wipe(ops, product, 64);
    wipe(ops, m_inv, 32);
}
// SAFETY: original defined byte extents and valid nonthrowing leaf table. Raw
// sequential access preserves source read/write timing; no external slices,
// heap, retained pointer or C++ unwind. Public wrappers retain original aliases,
// sizes and acceptance, while Rust aborts on panic. The retained SHA-512 caller
// invokes these same curve operations; this module adds no SHA-512 algorithm.
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_reduce(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    reduce(unsafe { &*ops }, out, input);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_mul_add(
    ops: *const Leaves,
    out: *mut u8,
    a: *const u8,
    b: *const u8,
    c: *const u8,
) {
    mul_add(unsafe { &*ops }, out, a, b, c);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_scalarbase(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    scalarbase(unsafe { &*ops }, out, input);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_check_equation(
    ops: *const Leaves,
    sig: *const u8,
    pk: *const u8,
    h: *const u8,
) -> i32 {
    check_equation(unsafe { &*ops }, sig, pk, h)
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_key_pair(
    ops: *const Leaves,
    secret: *mut u8,
    public: *mut u8,
    seed: *mut u8,
) {
    key_pair(unsafe { &*ops }, secret, public, seed);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_sign(
    ops: *const Leaves,
    sig: *mut u8,
    secret: *const u8,
    msg: *const u8,
    size: usize,
) {
    sign(unsafe { &*ops }, sig, secret, msg, size);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_check(
    ops: *const Leaves,
    sig: *const u8,
    public: *const u8,
    msg: *const u8,
    size: usize,
) -> i32 {
    check(unsafe { &*ops }, sig, public, msg, size)
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_eddsa_to_x25519(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    conversion(unsafe { &*ops }, out, input, true);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_x25519_to_eddsa(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    conversion(unsafe { &*ops }, out, input, false);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_x25519_dirty_small(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    dirty_small(unsafe { &*ops }, out, input);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_x25519_dirty_fast(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    dirty_fast(unsafe { &*ops }, out, input);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_elligator_map(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
) {
    elligator_map(unsafe { &*ops }, out, input);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_elligator_rev(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
    tweak: u8,
) -> i32 {
    elligator_rev(unsafe { &*ops }, out, input, tweak)
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_elligator_key_pair(
    ops: *const Leaves,
    hidden: *mut u8,
    secret: *mut u8,
    seed: *mut u8,
) {
    elligator_key_pair(unsafe { &*ops }, hidden, secret, seed);
}
#[unsafe(no_mangle)]
pub(crate) unsafe extern "C" fn openttd_rust_x25519_inverse(
    ops: *const Leaves,
    out: *mut u8,
    input: *const u8,
    point: *const u8,
) {
    inverse(unsafe { &*ops }, out, input, point);
}
