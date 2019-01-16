// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

typedef int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef uint8_t BlockSize;
typedef uint8_t TxSize;

const int32_t MAX_MIB_MASK = 7;
const int32_t MAX_MIB_SIZE = 8;
const int32_t MAX_MIB_SIZE_LOG2 = 3;
const int32_t MI_SIZE_LOG2 = 3;

#define IPP_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define IPP_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )

enum BlockSize_ {
    BLOCK_4X4, BLOCK_4X8, BLOCK_8X4, BLOCK_8X8, BLOCK_8X16, BLOCK_16X8, BLOCK_16X16, BLOCK_16X32, BLOCK_32X16, BLOCK_32X32,
    BLOCK_32X64, BLOCK_64X32, BLOCK_64X64, BLOCK_64X128, BLOCK_128X64, BLOCK_128X128, BLOCK_4X16, BLOCK_16X4, BLOCK_8X32,
    BLOCK_32X8, BLOCK_16X64, BLOCK_64X16, BLOCK_32X128, BLOCK_128X32, BLOCK_SIZES_ALL, BLOCK_SIZES = BLOCK_4X16,
    BLOCK_LARGEST = BLOCK_SIZES - 1, BLOCK_INVALID = 255
};
enum {
    TX_4X4, TX_8X8, TX_16X16, TX_32X32, TX_64X64, TX_4X8, TX_8X4, TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32,
    TX_4X16, TX_16X4, TX_8X32, TX_32X8, TX_16X64, TX_64X16, TX_SIZES_ALL, TX_SIZES = TX_4X8, TX_SIZES_LARGEST = TX_64X64,
    TX_INVALID = 255
};

const uint8_t block_size_wide[BLOCK_SIZES_ALL] = { 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 32, 64, 64, 64, 128, 128, 4, 16, 8, 32, 16, 64, 32, 128 };
const uint8_t block_size_high[BLOCK_SIZES_ALL] = { 4, 8, 4, 8, 16, 8, 16, 32, 16, 32, 64, 32, 64, 128, 64, 128, 16, 4, 32, 8, 64, 16, 128, 32 };
const uint8_t tx_size_wide_unit[TX_SIZES_ALL] = { 1, 2, 4, 8, 16, 1, 2, 2, 4, 4, 8, 8, 16, 1, 4, 2, 8, 4, 16 };
const uint8_t tx_size_high_unit[TX_SIZES_ALL] = { 1, 2, 4, 8, 16, 2, 1, 4, 2, 8, 4, 16, 8, 4, 1, 8, 2, 16, 4 };
const uint8_t block_size_wide_4x4_log2[BLOCK_SIZES_ALL] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 0, 2, 1, 3, 2, 4, 3, 5 };
const uint8_t block_size_high_4x4_log2[BLOCK_SIZES_ALL] = { 0, 1, 0, 1, 2, 1, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 2, 0, 3, 1, 4, 2, 5, 3 };

static const uint16_t orders_128x128[1] = { 0 };
static const uint16_t orders_128x64[2] = { 0, 1 };
static const uint16_t orders_64x128[2] = { 0, 1 };
static const uint16_t orders_64x64[4] = {0, 1, 2, 3};
static const uint16_t orders_64x32[8] = {
    0, 2, 1, 3, 4, 6, 5, 7,
};
static const uint16_t orders_32x64[8] = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const uint16_t orders_32x32[16] = {
    0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15,
};
static const uint16_t orders_32x16[32] = {
    0,  2,  8,  10, 1,  3,  9,  11, 4,  6,  12, 14, 5,  7,  13, 15,
    16, 18, 24, 26, 17, 19, 25, 27, 20, 22, 28, 30, 21, 23, 29, 31,
};
static const uint16_t orders_16x32[32] = {
    0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,
    16, 17, 18, 19, 24, 25, 26, 27, 20, 21, 22, 23, 28, 29, 30, 31,
};
static const uint16_t orders_16x16[64] = {
    0,  1,  4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
    8,  9,  12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
    32, 33, 36, 37, 48, 49, 52, 53, 34, 35, 38, 39, 50, 51, 54, 55,
    40, 41, 44, 45, 56, 57, 60, 61, 42, 43, 46, 47, 58, 59, 62, 63,
};
static const uint16_t orders_16x8[128] = {
  0,  2,  8,  10, 32,  34,  40,  42,  1,  3,  9,  11, 33,  35,  41,  43,
  4,  6,  12, 14, 36,  38,  44,  46,  5,  7,  13, 15, 37,  39,  45,  47,
  16, 18, 24, 26, 48,  50,  56,  58,  17, 19, 25, 27, 49,  51,  57,  59,
  20, 22, 28, 30, 52,  54,  60,  62,  21, 23, 29, 31, 53,  55,  61,  63,
  64, 66, 72, 74, 96,  98,  104, 106, 65, 67, 73, 75, 97,  99,  105, 107,
  68, 70, 76, 78, 100, 102, 108, 110, 69, 71, 77, 79, 101, 103, 109, 111,
  80, 82, 88, 90, 112, 114, 120, 122, 81, 83, 89, 91, 113, 115, 121, 123,
  84, 86, 92, 94, 116, 118, 124, 126, 85, 87, 93, 95, 117, 119, 125, 127,
};
static const uint16_t orders_8x16[128] = {
  0,  1,  2,  3,  8,  9,  10, 11, 32,  33,  34,  35,  40,  41,  42,  43,
  4,  5,  6,  7,  12, 13, 14, 15, 36,  37,  38,  39,  44,  45,  46,  47,
  16, 17, 18, 19, 24, 25, 26, 27, 48,  49,  50,  51,  56,  57,  58,  59,
  20, 21, 22, 23, 28, 29, 30, 31, 52,  53,  54,  55,  60,  61,  62,  63,
  64, 65, 66, 67, 72, 73, 74, 75, 96,  97,  98,  99,  104, 105, 106, 107,
  68, 69, 70, 71, 76, 77, 78, 79, 100, 101, 102, 103, 108, 109, 110, 111,
  80, 81, 82, 83, 88, 89, 90, 91, 112, 113, 114, 115, 120, 121, 122, 123,
  84, 85, 86, 87, 92, 93, 94, 95, 116, 117, 118, 119, 124, 125, 126, 127,
};
static const uint16_t orders_8x8[256] = {
  0,   1,   4,   5,   16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,
  85,  2,   3,   6,   7,   18,  19,  22,  23,  66,  67,  70,  71,  82,  83,
  86,  87,  8,   9,   12,  13,  24,  25,  28,  29,  72,  73,  76,  77,  88,
  89,  92,  93,  10,  11,  14,  15,  26,  27,  30,  31,  74,  75,  78,  79,
  90,  91,  94,  95,  32,  33,  36,  37,  48,  49,  52,  53,  96,  97,  100,
  101, 112, 113, 116, 117, 34,  35,  38,  39,  50,  51,  54,  55,  98,  99,
  102, 103, 114, 115, 118, 119, 40,  41,  44,  45,  56,  57,  60,  61,  104,
  105, 108, 109, 120, 121, 124, 125, 42,  43,  46,  47,  58,  59,  62,  63,
  106, 107, 110, 111, 122, 123, 126, 127, 128, 129, 132, 133, 144, 145, 148,
  149, 192, 193, 196, 197, 208, 209, 212, 213, 130, 131, 134, 135, 146, 147,
  150, 151, 194, 195, 198, 199, 210, 211, 214, 215, 136, 137, 140, 141, 152,
  153, 156, 157, 200, 201, 204, 205, 216, 217, 220, 221, 138, 139, 142, 143,
  154, 155, 158, 159, 202, 203, 206, 207, 218, 219, 222, 223, 160, 161, 164,
  165, 176, 177, 180, 181, 224, 225, 228, 229, 240, 241, 244, 245, 162, 163,
  166, 167, 178, 179, 182, 183, 226, 227, 230, 231, 242, 243, 246, 247, 168,
  169, 172, 173, 184, 185, 188, 189, 232, 233, 236, 237, 248, 249, 252, 253,
  170, 171, 174, 175, 186, 187, 190, 191, 234, 235, 238, 239, 250, 251, 254,
  255,
};

static const uint16_t *const orders[BLOCK_SIZES_ALL] = {
  //                              4X4
                                  nullptr,
  // 4X8,         8X4,            8X8
  nullptr,        nullptr,        orders_8x8,
  // 8X16,        16X8,           16X16
  orders_8x16,    orders_16x8,    orders_16x16,
  // 16X32,       32X16,          32X32
  orders_16x32,   orders_32x16,   orders_32x32,
  // 32X64,       64X32,          64X64
  orders_32x64,   orders_64x32,   orders_64x64,
  // 64x128,      128x64,         128x128
  orders_64x128,  orders_128x64,  orders_128x128,
  // 4x16,        16x4,           8x32
  nullptr,        nullptr,        nullptr,
  // 32x8,        16x64,          64x16
  nullptr,        nullptr,        nullptr,
  // 32x128,      128x32
  nullptr,        nullptr
};

int32_t HaveTopRight(BlockSize bsize, int32_t miRow, int32_t miCol, int32_t haveTop, int32_t haveRight, int32_t txsz, int32_t y, int32_t x, int32_t ss_x)
{
    if (!haveTop || !haveRight) return 0;

    const int bw_unit = block_size_wide[bsize] >> 2;
    const int plane_bw_unit = IPP_MAX(bw_unit >> ss_x, 1);
    const int top_right_count_unit = tx_size_wide_unit[txsz];

    if (y > 0) {  // Just need to check if enough pixels on the right.
        return x + top_right_count_unit < plane_bw_unit;
    } else {
        // All top-right pixels are in the block above, which is already available.
        if (x + top_right_count_unit < plane_bw_unit) return 1;

        const int bw_in_mi_log2 = block_size_wide_4x4_log2[bsize];
        const int bh_in_mi_log2 = block_size_high_4x4_log2[bsize];
        const int blk_row_in_sb = (miRow & MAX_MIB_MASK) << 1 >> bh_in_mi_log2;
        const int blk_col_in_sb = (miCol & MAX_MIB_MASK) << 1 >> bw_in_mi_log2;

        // Top row of superblock: so top-right pixels are in the top and/or
        // top-right superblocks, both of which are already available.
        if (blk_row_in_sb == 0) return 1;

        // Rightmost column of superblock (and not the top row): so top-right pixels
        // fall in the right superblock, which is not available yet.
        if (((blk_col_in_sb + 1) << bw_in_mi_log2) >= (MAX_MIB_SIZE << 1)) return 0;

        // General case (neither top row nor rightmost column): check if the
        // top-right block is coded before the current block.
        const uint16_t *const order = orders[bsize];
        const int this_blk_index =
            ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
            blk_col_in_sb + 0;
        const uint16_t this_blk_order = order[this_blk_index];
        const int tr_blk_index =
            ((blk_row_in_sb - 1) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
            blk_col_in_sb + 1;
        const uint16_t tr_blk_order = order[tr_blk_index];
        return tr_blk_order < this_blk_order;
    }
}

int HaveBottomLeft(BlockSize bsize, int32_t miRow, int32_t miCol, int32_t haveLeft, int32_t haveBottom, int32_t txsz, int32_t y, int32_t x, int32_t ss_y)
{
    if (!haveLeft || !haveBottom)
        return 0;

    const int32_t bh_unit = block_size_high[bsize] >> 2;
    const int32_t plane_bh_unit = IPP_MAX(bh_unit >> ss_y, 1);
    const int32_t bottom_left_count_unit = tx_size_high_unit[txsz];

    // Bottom-left pixels are in the bottom-left block, which is not available.
    if (x > 0)
        return 0;

    // All bottom-left pixels are in the left block, which is already available.
    if (y + bottom_left_count_unit < plane_bh_unit)
        return 1;

    const int32_t bw_in_mi_log2 = block_size_wide_4x4_log2[bsize];
    const int32_t bh_in_mi_log2 = block_size_high_4x4_log2[bsize];
    const int32_t blk_row_in_sb = (miRow & MAX_MIB_MASK) << 1 >> bh_in_mi_log2;
    const int32_t blk_col_in_sb = (miCol & MAX_MIB_MASK) << 1 >> bw_in_mi_log2;

    // Leftmost column of superblock: so bottom-left pixels maybe in the left
    // and/or bottom-left superblocks. But only the left superblock is
    // available, so check if all required pixels fall in that superblock.
    if (blk_col_in_sb == 0) {
        const int32_t blk_start_row_off = blk_row_in_sb << (bh_in_mi_log2 + MI_SIZE_LOG2-1 - 2) >> ss_y;
        const int32_t row_off_in_sb = blk_start_row_off + y;
        const int32_t sb_height_unit = MAX_MIB_SIZE << 1 << (MI_SIZE_LOG2-1 - 2) >> ss_y;
        return row_off_in_sb + bottom_left_count_unit < sb_height_unit;
    }

    // Bottom row of superblock (and not the leftmost column): so bottom-left
    // pixels fall in the bottom superblock, which is not available yet.
    if (((blk_row_in_sb + 1) << bh_in_mi_log2) >= (MAX_MIB_SIZE << 1)) return 0;

    // General case (neither leftmost column nor bottom row): check if the
    // bottom-left block is coded before the current block.
    const uint16_t *const order = orders[bsize];
    const int32_t this_blk_index =
        ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
        blk_col_in_sb + 0;
    const uint16_t this_blk_order = order[this_blk_index];
    const int32_t bl_blk_index =
        ((blk_row_in_sb + 1) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
        blk_col_in_sb - 1;
    const uint16_t bl_blk_order = order[bl_blk_index];
    return bl_blk_order < this_blk_order;
}
