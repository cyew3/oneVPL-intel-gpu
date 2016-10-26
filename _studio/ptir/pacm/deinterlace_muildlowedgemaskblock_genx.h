//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef _BUILD_LOW_EDGE_MASK_BLOCK_GENX_H_
#define _BUILD_LOW_EDGE_MASK_BLOCK_GENX_H_

template <uint R, uint C>
inline matrix<uchar, R, C> _GENX_ cm_FilterClip3x3Matrix(matrix_ref<uchar, R + 2, C> left, matrix_ref<uchar, R + 2, C> med, matrix_ref<uchar, R + 2, C> right) {
    matrix<short, R + 2, C> temp = left + med + right;
    matrix<short, R, C> filtered = temp.template select<R, 1, C, 1>(1) * 2 - temp.template select<R, 1, C, 1>(0) - temp.template select<R, 1, C, 1>(2);
    matrix<uchar, R, C> out;
    out.merge(10, 128, filtered<-24);
    out.merge(255, filtered>23);
    return out;
}

template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV, uint BLOCKWIDTH, uint BLOCKHEIGHT>
void inline _GENX_ ReadTMPBLock(SurfaceIndex surfFrame, matrix<uchar, BLOCKHEIGHT, BLOCKWIDTH> &tmp, int xoff, int y, uint IMAGEWIDTH) {
    matrix<uchar, BLOCKHEIGHT + 2, BLOCKWIDTH*XSTEP> left, med, right;
    int topy = (y - 2) >> 1;
#ifdef CMRT_EMU
    topy <<= 1;
#endif
    if (BotBase) {
        read_plane(TOP_FIELD(surfFrame), PID, xoff*XSTEP - XSTEP, topy, left);
        read_plane(TOP_FIELD(surfFrame), PID, xoff*XSTEP, topy, med);
        read_plane(TOP_FIELD(surfFrame), PID, xoff*XSTEP + XSTEP, topy, right);
    }
    else {
        read_plane(BOTTOM_FIELD(surfFrame), PID, xoff*XSTEP - XSTEP, topy, left);
        read_plane(BOTTOM_FIELD(surfFrame), PID, xoff*XSTEP, topy, med);
        read_plane(BOTTOM_FIELD(surfFrame), PID, xoff*XSTEP + XSTEP, topy, right);
    }

    tmp = cm_FilterClip3x3Matrix<BLOCKHEIGHT, BLOCKWIDTH*XSTEP>(left.select_all(), med.select_all(), right.select_all()).template select<BLOCKHEIGHT, 1, BLOCKWIDTH, XSTEP>(UV);
    if (xoff == 0)
        tmp.template select<BLOCKHEIGHT, 1, 4 / XSTEP, 1>(0, 0) = 128;
    if (xoff == IMAGEWIDTH / XSTEP - BLOCKWIDTH)
        tmp.template select<BLOCKHEIGHT, 1, 4 / XSTEP, 1>(0, BLOCKWIDTH - 4 / XSTEP) = 128;
}


template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int START>
inline void _GENX_ WriteMask(SurfaceIndex surfBadMC, int xoff, int y, int curN, int spacer, int prevN) {
    uint ONES;
    if (XSTEP == 1)
        ONES = 0xFFFFFFFF;
    else if (XSTEP == 2 && START == 0)
        ONES = 0x55555555;
    else
        ONES = 0xAAAAAAAA;

    uint borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
    uint borderLen = borderStart * 2 + spacer;
    uint begin = xoff - borderStart;
    uint end = begin + borderLen - 1;

    const int BufferSlots = 32 / XSTEP;
    const int beginbufpos = begin >> (6 - XSTEP);
    const int endbufpos = end >> (6 - XSTEP);
    const int beginzeros = begin % BufferSlots;
    const int endzeros = BufferSlots - end % BufferSlots;

    vector<uchar, 32> beginbuffer = 0;
    beginbuffer.merge(1, cm_shl<uint>(ONES, beginzeros * XSTEP));
    vector<uchar, 32> endbuffer = 0;
    endbuffer.merge(1, cm_shr<uint>(ONES, endzeros * XSTEP));
    if (beginbufpos == endbufpos) {
        matrix<uchar, 1, 32> buffer;
        read_plane(surfBadMC, PID, beginbufpos * 32, y, buffer);
        buffer.merge(255, beginbuffer & endbuffer);
        write_plane(surfBadMC, PID, beginbufpos * 32, y, buffer);
    }
    else {
        matrix<uchar, 1, 32> buffer;
        read_plane(surfBadMC, PID, beginbufpos * 32, y, buffer);
        buffer.merge(255, beginbuffer);
        write_plane(surfBadMC, PID, beginbufpos * 32, y, buffer);
        read_plane(surfBadMC, PID, endbufpos * 32, y, buffer);
        buffer.merge(255, endbuffer);
        write_plane(surfBadMC, PID, endbufpos * 32, y, buffer);
        buffer = 255;
        for (int i = beginbufpos * 32 + 32; i<endbufpos * 32; i += 32)
            write_plane(surfBadMC, PID, i, y, buffer);
    }
}

template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV>
inline uint _GENX_ ReadRightArm(SurfaceIndex surfIn, int xoff, int y, uchar cur, int LeftSpacerAllowed, int ImageWidth) {
    matrix<uchar, 1, 16> tmp;
    xoff += (2 - LeftSpacerAllowed);
    uint count = 0;
    while (1) {
        ReadTMPBLock<BotBase, PID, XSTEP, UV, 16>(surfIn, tmp, xoff, y, ImageWidth);
        uint mask = cm_pack_mask(tmp != cur) | 0xffff0000;
        uint runLength = cm_fbl(mask);
        count += runLength;
        if (runLength != 16)
            break;
        xoff += runLength;
    }
    return count;
}

template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV>
inline uint _GENX_ ReadLeftArm(SurfaceIndex surfIn, int xoff, int y, uchar cur, int LeftSpacerAllowed, int ImageWidth) {
    matrix<uchar, 1, 16> tmp;
    xoff -= 15;
    uint count = 0;
    while (1) {
        ReadTMPBLock<BotBase, PID, XSTEP, UV, 16>(surfIn, tmp, xoff, y, ImageWidth);
        uint mask = cm_pack_mask(tmp != cur) << 16 | 0xffff;
        uint runLength = cm_fbh(mask);
        count += runLength;
        if (runLength != 16)
            break;
        xoff -= runLength;
    }
    return count;
}

template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV>
_GENX_ void SearchAndWriteMask(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int xoff, int y, uchar leftVal, uchar rightVal, uint ImageWidth, int LeftSpacerAllowed) {
    uint right = ReadRightArm<BotBase, PID, XSTEP, UV>(surfIn, xoff + 1, y, rightVal, LeftSpacerAllowed, ImageWidth);
    uint left;
    left = ReadLeftArm<BotBase, PID, XSTEP, UV>(surfIn, xoff, y, leftVal, LeftSpacerAllowed, ImageWidth);

    while (LeftSpacerAllowed > 0) {
        short spacer0 = ReadLeftArm<BotBase, PID, XSTEP, UV>(surfIn, xoff - left, y, 128, LeftSpacerAllowed, ImageWidth);
        if (spacer0 == 0)
            break;
        else if (spacer0 <= LeftSpacerAllowed) {
            left += ReadLeftArm<BotBase, PID, XSTEP, UV>(surfIn, xoff - left - spacer0, y, leftVal, LeftSpacerAllowed, ImageWidth);
            LeftSpacerAllowed -= spacer0;
        }
        else
            break;
    }
    WriteMask<BotBase, PID, XSTEP, UV>(surfBadMC, xoff, y, right, 2 - LeftSpacerAllowed, left);
}

template <uint BLOCK_WIDTH, uint BLOCK_HEIGHT, int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV>
inline void _GENX_ cmk_FilterMask_Main_Block_Plane(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width) {
    uint y = BLOCK_HEIGHT * get_thread_origin_y();
    uint x = BLOCK_WIDTH * get_thread_origin_x();
    if (x > Width) return;
    if (y > Height) return;
    matrix<uchar, BLOCK_HEIGHT, BLOCK_WIDTH + 4> tmp_extended;
    matrix_ref<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> tmp = tmp_extended.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(0, 0);
    matrix_ref<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> tmpR1 = tmp_extended.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(0, 1);
    matrix_ref<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> tmpR2 = tmp_extended.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(0, 2);
    matrix_ref<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> tmpR3 = tmp_extended.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(0, 3);
    ReadTMPBLock<BotBase, PID, XSTEP, UV>(surfIn, tmp_extended, x, y, Width);
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> check0, check1, check2;
    vector<ushort, BLOCK_WIDTH> mask;
    check0 = tmp + tmpR1;
    check1 = tmp + tmpR2;
    check2 = tmp + tmpR3;
#pragma unroll
    for (int r = 0; r < BLOCK_HEIGHT; r++) {
        mask = check0.row(r) == 265;
        if (mask.any()) {
            for (int c = 0; c < BLOCK_HEIGHT; c++) {
                if (mask[c]) {
                    SearchAndWriteMask<BotBase, PID, XSTEP, UV>(surfIn, surfBadMC, x + c, y + r * 2, tmp[r][c], 265 - tmp[r][c], Width, 2);
                }
            }
        }
        mask = (check1.row(r) == 265) & (tmpR1.row(r) == 128);
        if (mask.any()) {
            for (int c = 0; c < BLOCK_HEIGHT; c++) {
                if (mask[c]) {
                    SearchAndWriteMask<BotBase, PID, XSTEP, UV>(surfIn, surfBadMC, x + c, y + r * 2, tmp[r][c], 265 - tmp[r][c], Width, 1);
                }
            }
        }
        mask = (check2.row(r) == 265) & (tmpR1.row(r) == 128) & tmpR2.row(r) == 128;
        if (mask.any()) {
            for (int c = 0; c < BLOCK_HEIGHT; c++) {
                if (mask[c]) {
                    SearchAndWriteMask<BotBase, PID, XSTEP, UV>(surfIn, surfBadMC, x + c, y + r * 2, tmp[r][c], 265 - tmp[r][c], Width, 0);
                }
            }
        }
    }
}

template <uint BLOCK_HEIGHT, uint BLOCK_WIDTH, int BotBase>
void _GENX_MAIN_ cmk_FilterMask_Main_Block(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width) {
    cmk_FilterMask_Main_Block_Plane<BLOCK_WIDTH, BLOCK_HEIGHT, BotBase, GENX_SURFACE_Y_PLANE, 1, 0>(surfIn, surfBadMC, Height, Width);
}

#endif _BUILD_LOW_EDGE_MASK_BLOCK_GENX_H_