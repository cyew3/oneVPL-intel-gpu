//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef _BUILD_LOW_EDGE_MASK_GENX_H_
#define _BUILD_LOW_EDGE_MASK_GENX_H_

//pack a vector of uchar to bit stream. when uchar == 1, bit is set; when uchar is other char, bit is cleared.
template <uint WIDTH>
void inline _GENX_ cm_pack_inplace(vector<uchar, WIDTH> &in) {
    static_assert(WIDTH % 16 == 0, "cm_pack_inplace only handles width that is multiple of 16");
#pragma unroll
    for (int i = 0; i < WIDTH; i += 16)
        in.template format<ushort>()[i / 16] = cm_pack_mask(in.template select<16, 1>(i) == 1);
}

//fill mask into badmc line. maybe there's some other way to improve speed.
template <int STEP, uint WIDTH>
void inline _GENX_ fillmask(vector_ref<uchar, WIDTH> badmc, uint begin, uint length, uchar content) {
    int end = begin + length;
    int i;
    for (i = begin; i < end - (STEP - 1); i += STEP) {
        badmc.template select<STEP, 1>(i) = content;
    }
    for (; i < end; i++)
        badmc[i] = content;
}

//generates badmc mask, fixed width.
template <uint WIDTH>
void inline _GENX_ TMP2BADMC(vector_ref<uchar, WIDTH> temp, vector_ref<uchar, WIDTH> badmc) {
    unsigned int x, y, i, j;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    {

        prevN = 0;
        curN = 0;
        spacer = 0;

        lastCol = 128;

        const uint end = WIDTH - 2;

        for (x = 2; x < end;)
        {
            vector<ushort, 16> mask;
            uint thisrun;
            uint count;

            //
            // process a run of pixels
            //
            uchar cur = temp[x];
            count = 0;
#if 0
            do {
                thisrun = cm_fbl(cm_pack_mask(temp.template select<16, 1>(x + count) != cur) | 0xffff0000);
                count += thisrun;
            } while (thisrun == 16 && x + count<end);
#else // optimization branches
#if 1
            {
                vector<uchar, 32> test;
                do {
                    test = temp.template select<32, 1>(x + count);
                    thisrun = cm_fbl(cm_pack_mask(test != cur) | 0x80000000);
                    count += thisrun;
                } while (thisrun == 31 && x + count<end);
            }
#else
            { // optimization causing CM compilter fail.
                vector<uchar, 32> test;
                do {
                    ushort pos = x + count;
                    ushort posDW = pos >> 2;
                    ushort skipbits = pos & 3;
                    vector_ref<uint, WIDTH / 4> tempref = temp.template format<uint>();
                    test.template format<uint>() = tempref.template select<8, 1>(posDW);
                    thisrun = cm_fbl((cm_pack_mask(test != cur) & (0xffffffff << skipbits)) | 0x80000000);
                    count += thisrun - skipbits;
                } while (thisrun == 31 && x + count<end);
            }

#endif
#endif

            if (cur == 128)
            {
                if (spacer <= 2 && curN > 0 && prevN > 0) //different colors is neighbours
                {
                    borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
                    borderLen = borderStart * 2 + spacer;
                    borderStart += spacer + curN;
                    fillmask<8>(badmc, x - borderStart, borderLen, 1);//filing mask

                    prevN = 0;
                    spacer = 0;
                }

                spacer += count;
                if (spacer >= 3)
                {
                    curN = 0;
                    prevN = 0;
                    lastCol = 128;
                }
            }
            //
            // process a run of non-empty pixels
            //
            else
            {
                if (cur == lastCol)//same Color
                {
                    curN += count;
                }
                else
                {
                    if (lastCol == 128)//new Line
                    {
                        prevN = 0;
                        spacer = 0;
                    }
                    else              //color changing
                    {
                        if (prevN > 0) //second(or more) color change per line
                        {
                            borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
                            borderLen = borderStart * 2 + spacer;
                            borderStart += spacer + curN;
                            fillmask<8>(badmc, x - borderStart, borderLen, 1); //__stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

                            spacer = 0;
                        }
                        prevN = curN;
                    }
                    curN = count;
                    lastCol = cur;
                }
            }
            x += count;
        }
    }
}

//generates badmc mask, variable width.
template <uint MAXWIDTH>
void inline _GENX_ TMP2BADMC_VarWidth(vector_ref<uchar, MAXWIDTH> temp, vector_ref<uchar, MAXWIDTH> badmc, uint width) {
    unsigned int x, y, i, j;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    {

        prevN = 0;
        curN = 0;
        spacer = 0;

        lastCol = 128;
        const uint end = width - 2;

        for (x = 2; x < end;)
        {
            vector<ushort, 16> mask;
            uint thisrun;
            uint count;

            //
            // process a run of pixels
            //
            uchar cur = temp[x];
            count = 0;

#if 0
            do {
                thisrun = cm_fbl(cm_pack_mask(temp.template select<16, 1>(x + count) != cur) | 0xffff0000);
                count += thisrun;
            } while (thisrun == 16 && x + count<end);
#else // optimization branches.
            {
                vector<uchar, 32> test;
                do {
                    test = temp.template select<32, 1>(x + count);
                    thisrun = cm_fbl(cm_pack_mask(test != cur) | 0x80000000);
                    count += thisrun;
                } while (thisrun == 31 && x + count<end);
            }
#endif

            if (cur == 128)
            {
                if (spacer <= 2 && curN > 0 && prevN > 0) //different colors is neighbours
                {
                    borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
                    borderLen = borderStart * 2 + spacer;
                    borderStart += spacer + curN;
                    fillmask<8>(badmc, x - borderStart, borderLen, 1); //filing mask

                    prevN = 0;
                    spacer = 0;
                }

                spacer += count;
                if (spacer >= 3)
                {
                    curN = 0;
                    prevN = 0;
                    lastCol = 128;
                }
            }
            //
            // process a run of non-empty pixels
            //
            else
            {
                if (cur == lastCol)//same Color
                {
                    curN += count;
                }
                else
                {
                    if (lastCol == 128)//new Line
                    {
                        prevN = 0;
                        spacer = 0;
                    }
                    else              //color changing
                    {
                        if (prevN > 0) //second(or more) color change per line
                        {
                            borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
                            borderLen = borderStart * 2 + spacer;
                            borderStart += spacer + curN;
                            fillmask<8>(badmc, x - borderStart, borderLen, 1); //__stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

                            spacer = 0;
                        }
                        prevN = curN;
                    }
                    curN = count;
                    lastCol = cur;
                }
            }
            x += count;
        }
    }
}

#if 0
template <uint C>
inline vector<uchar, C> _GENX_ cm_FilterClip3x3(matrix_ref<uchar, 3, C> left, matrix_ref<uchar, 3, C> med, matrix_ref<uchar, 3, C> right) {
    vector<short, C> filtered;
    filtered = cm_add<short>(left.row(1), med.row(1));
    filtered = cm_add<short>(filtered, right.row(1));
    filtered = cm_shl<short>(filtered, 1);
    filtered = cm_add<short>(filtered, -left.row(0));
    filtered = cm_add<short>(filtered, -med.row(0));
    filtered = cm_add<short>(filtered, -right.row(0));
    filtered = cm_add<short>(filtered, -left.row(2));
    filtered = cm_add<short>(filtered, -med.row(2));
    filtered = cm_add<short>(filtered, -right.row(2));
    vector<uchar, C> out;
    if (C > 16) { // workaround a CM compiler bug to enable SIMD16 instructions.
        out.template select<C / 2, 2>(0).merge(10, 128, filtered.template select<C / 2, 2>(0) < -24);
        out.template select<C / 2, 2>(1).merge(10, 128, filtered.template select<C / 2, 2>(1) < -24);
        out.template select<C / 2, 2>(0).merge(255, filtered.template select<C / 2, 2>(0)>23);
        out.template select<C / 2, 2>(1).merge(255, filtered.template select<C / 2, 2>(1)>23);
    }
    else {
        out.merge(10, 128, filtered<-24);
        out.merge(255, filtered>23);
    }
    return out;
}
#endif

//read from median-filtered surface and generate TMP values. Temp values are 10, 128, 255 instead of 0, 128, 255.
//Purpose is to avoid confusion of 0/1 badmc result if vector is reused.
//Fixed width version.
template <uint WIDTH, int BotBase, CmSurfacePlaneIndex PID, int XSTEP>
void inline _GENX_ ReadTMPLine(SurfaceIndex surfFrame, vector_ref<uchar, WIDTH> tmp, int xoff, int y) {
    matrix<uchar, 3, 32> left, med, right;
    vector<short, 32> filtered;
    int topy = (y - 2) >> 1;
#ifdef CMRT_EMU
    topy <<= 1;
#endif
    for (int x = 0; x<WIDTH; x += 32) {
        if (BotBase) {
            read_plane(TOP_FIELD(surfFrame), PID, xoff + x - XSTEP, topy, left);
            filtered = left.row(1); // didn't use long expression to avoid conversion from uchar to int and back to short
            filtered += left.row(1);
            filtered -= left.row(0);
            filtered -= left.row(2);
            read_plane(TOP_FIELD(surfFrame), PID, xoff + x, topy, med);
            filtered += med.row(1);
            filtered += med.row(1);
            filtered -= med.row(0);
            filtered -= med.row(2);
            read_plane(TOP_FIELD(surfFrame), PID, xoff + x + XSTEP, topy, right);
            filtered += right.row(1);
            filtered += right.row(1);
            filtered -= right.row(0);
            filtered -= right.row(2);
        }
        else {
            read_plane(BOTTOM_FIELD(surfFrame), PID, xoff + x - XSTEP, topy, left);
            filtered = left.row(1); // didn't use long expression to avoid conversion from uchar to int and back to short
            filtered += left.row(1);
            filtered -= left.row(0);
            filtered -= left.row(2);
            read_plane(BOTTOM_FIELD(surfFrame), PID, xoff + x, topy, med);
            filtered += med.row(1);
            filtered += med.row(1);
            filtered -= med.row(0);
            filtered -= med.row(2);
            read_plane(BOTTOM_FIELD(surfFrame), PID, xoff + x + XSTEP, topy, right);
            filtered += right.row(1);
            filtered += right.row(1);
            filtered -= right.row(0);
            filtered -= right.row(2);
        }
        vector<uchar, 32> out;
#if 0
        out.merge(10, 128, filtered<-24);
        out.merge(255, filtered>23);
#else // workaround a CM compiler bug to generate SIMD16 instructions
        out.template select<16, 2>(0).merge(10, 128, filtered.template select<16, 2>(0) < -24);
        out.template select<16, 2>(1).merge(10, 128, filtered.template select<16, 2>(1) < -24);
        out.template select<16, 2>(0).merge(255, filtered.template select<16, 2>(0)>23);
        out.template select<16, 2>(1).merge(255, filtered.template select<16, 2>(1)>23);
#endif
        tmp.template select<32, 1>(x) = out;
    }
    tmp.template select<4, 1>(0) = 128; //left border
    tmp.template select<4, 1>(WIDTH - 4) = 128; // right border
}

//read from median-filtered surface and generate TMP values. Temp values are 10, 128, 255 instead of 0, 128, 255.
//Purpose is to avoid confusion of 0/1 badmc result if vector is reused.
//Able to process variable width, a little slower than fixed width version.
template <uint MAXWIDTH, int BotBase, CmSurfacePlaneIndex PID, int XSTEP>
void inline _GENX_ ReadTMPLine_VarWidth(SurfaceIndex surfFrame, vector_ref<uchar, MAXWIDTH> tmp, int xoff, int y, int width) {
    matrix<uchar, 3, 32> left, med, right;
    vector<short, 32> filtered;
    int topy = (y - 2) >> 1;
#ifdef CMRT_EMU
    topy <<= 1;
#endif
    for (int x = 0; x<width; x += 32) {
        if (BotBase) {
            read_plane(TOP_FIELD(surfFrame), PID, xoff + x - XSTEP, topy, left);
            filtered = left.row(1); // didn't use long expression to avoid conversion from uchar to int and back to short
            filtered += left.row(1);
            filtered -= left.row(0);
            filtered -= left.row(2);
            read_plane(TOP_FIELD(surfFrame), PID, xoff + x, topy, med);
            filtered += med.row(1);
            filtered += med.row(1);
            filtered -= med.row(0);
            filtered -= med.row(2);
            read_plane(TOP_FIELD(surfFrame), PID, xoff + x + XSTEP, topy, right);
            filtered += right.row(1);
            filtered += right.row(1);
            filtered -= right.row(0);
            filtered -= right.row(2);
        }
        else {
            read_plane(BOTTOM_FIELD(surfFrame), PID, xoff + x - XSTEP, topy, left);
            filtered = left.row(1); // didn't use long expression to avoid conversion from uchar to int and back to short
            filtered += left.row(1);
            filtered -= left.row(0);
            filtered -= left.row(2);
            read_plane(BOTTOM_FIELD(surfFrame), PID, xoff + x, topy, med);
            filtered += med.row(1);
            filtered += med.row(1);
            filtered -= med.row(0);
            filtered -= med.row(2);
            read_plane(BOTTOM_FIELD(surfFrame), PID, xoff + x + XSTEP, topy, right);
            filtered += right.row(1);
            filtered += right.row(1);
            filtered -= right.row(0);
            filtered -= right.row(2);
        }
        vector<uchar, 32> out;
#if 0
        out.merge(10, 128, filtered<-24);
        out.merge(255, filtered>23);
#else // workaround a CM compiler bug to generate SIMD16 instructions
        out.template select<16, 2>(0).merge(10, 128, filtered.template select<16, 2>(0) < -24);
        out.template select<16, 2>(1).merge(10, 128, filtered.template select<16, 2>(1) < -24);
        out.template select<16, 2>(0).merge(255, filtered.template select<16, 2>(0)>23);
        out.template select<16, 2>(1).merge(255, filtered.template select<16, 2>(1)>23);
#endif
        tmp.template select<32, 1>(x) = out;
    }
    tmp.template select<4, 1>(0) = 128;
    tmp.template select<4, 1>(width - 4) = 128;
}

//filter main function, will read a whole line of TMP data then create packed badmc mask and write to badmc
//fixed width version
template <uint WIDTH, int BotBase>
void inline _GENX_ cmk_FilterMask_Main_ReadAhead_Packed(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int threadid) {
    static_assert(WIDTH % 32 == 0, "WIDTH has to be multiple of 32");
    vector<uchar, WIDTH> tmp;
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        vector<uchar, WIDTH> badmc = 0;
        TMP2BADMC(tmp.template select<WIDTH, 1>(0), badmc.template select<WIDTH, 1>(0));
        cm_pack_inplace(badmc);
        blocky_write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, 0, y, badmc.template select<WIDTH/8, 1>(0));
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        vector<uchar, WIDTH> badmc = 0;
        TMP2BADMC(tmp.template select<WIDTH / 2, 2>(0), badmc.template select<WIDTH / 2, 2>(0));
        TMP2BADMC(tmp.template select<WIDTH / 2, 2>(1), badmc.template select<WIDTH / 2, 2>(1));
        cm_pack_inplace(badmc);
        blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, badmc.template select<WIDTH / 8, 1>(0));
    }
}

//filter main function, will read a whole line of TMP data then create packed badmc mask and write to badmc.
//Able to process variable width, but a little slower than fixed width version
template <uint MAXWIDTH, int BotBase>
void inline _GENX_ cmk_FilterMask_Main_ReadAhead_Packed_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int threadid, int width) {
    static_assert(MAXWIDTH % 32 == 0, "WIDTH has to be multiple of 32");
    vector<uchar, MAXWIDTH> tmp;
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine_VarWidth<MAXWIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<MAXWIDTH, 1>(0), 0, y, width);
        vector<uchar, MAXWIDTH> badmc = 0;
        TMP2BADMC_VarWidth(tmp.template select<MAXWIDTH, 1>(0), badmc.template select<MAXWIDTH, 1>(0), width);
        cm_pack_inplace(badmc);
        blocky_write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, 0, y, badmc.template select<MAXWIDTH / 8, 1>(0));
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine_VarWidth<MAXWIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<MAXWIDTH, 1>(0), 0, y, width);
        vector<uchar, MAXWIDTH> badmc = 0;
        TMP2BADMC_VarWidth(tmp.template select<MAXWIDTH / 2, 2>(0), badmc.template select<MAXWIDTH / 2, 2>(0), width >> 1);
        TMP2BADMC_VarWidth(tmp.template select<MAXWIDTH / 2, 2>(1), badmc.template select<MAXWIDTH / 2, 2>(1), width >> 1);
        cm_pack_inplace(badmc);
        blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, badmc.template select<MAXWIDTH / 8, 1>(0));
    }
}

//filter main function, will read a whole line of TMP data then create packed badmc mask and write to badmc
//reuse tmp vector as badmc buffer, so to fit in width higher than 1920. It can handle up to width of 3840.
//Beyond that, we would need to use read-on-fly version in deinterlace_buildlowedgemask_genx_legacy.h
//fixed width version
template <uint WIDTH, int BotBase>
void inline _GENX_ cmk_FilterMask_Main_ReadAhead_Reuse_Packed(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int threadid) {
    static_assert(WIDTH % 32 == 0, "WIDTH has to be multiple of 32");
    vector<uchar, WIDTH> tmp;
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        TMP2BADMC(tmp.template select<WIDTH, 1>(0), tmp.template select<WIDTH, 1>(0));
        cm_pack_inplace(tmp);
        blocky_write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, 0, y, tmp.template select<WIDTH / 8, 1>(0));
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        TMP2BADMC(tmp.template select<WIDTH / 2, 2>(0), tmp.template select<WIDTH / 2, 2>(0));
        TMP2BADMC(tmp.template select<WIDTH / 2, 2>(1), tmp.template select<WIDTH / 2, 2>(1));
        cm_pack_inplace(tmp);
        blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, tmp.template select<WIDTH / 8, 1>(0));
    }
}

//filter main function, will read a whole line of TMP data then create packed badmc mask and write to badmc
//reuse tmp vector as badmc buffer, so to fit in width higher than 1920. It can handle up to width of 3840.
//Beyond that, we would need to use read-on-fly version in deinterlace_buildlowedgemask_genx_legacy.h
//variable width version
template <uint MAXWIDTH, int BotBase>
void inline _GENX_ cmk_FilterMask_Main_ReadAhead_Reuse_Packed_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int threadid, int width) {
    static_assert(MAXWIDTH % 32 == 0, "WIDTH has to be multiple of 32");
    vector<uchar, MAXWIDTH> tmp;
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine_VarWidth<MAXWIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<MAXWIDTH, 1>(0), 0, y, width);
        TMP2BADMC_VarWidth(tmp.template select<MAXWIDTH, 1>(0), tmp.template select<MAXWIDTH, 1>(0), width);
        cm_pack_inplace(tmp);
        blocky_write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, 0, y, tmp.template select<MAXWIDTH / 8, 1>(0));
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        ReadTMPLine_VarWidth<MAXWIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<MAXWIDTH, 1>(0), 0, y, width);
        TMP2BADMC_VarWidth(tmp.template select<MAXWIDTH / 2, 2>(0), tmp.template select<MAXWIDTH / 2, 2>(0), width >> 1);
        TMP2BADMC_VarWidth(tmp.template select<MAXWIDTH / 2, 2>(1), tmp.template select<MAXWIDTH / 2, 2>(1), width >> 1);
        cm_pack_inplace(tmp);
        blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, tmp.template select<MAXWIDTH / 8, 1>(0));
    }
}

//main template to select fastest method for given width
//because number of threads is larger than 511, we change linear threadid to a matrix with width of THREADYSCALE.
template <uint WIDTH, int BotBase, uint THREADYSCALE>
void _GENX_MAIN_ cmk_FilterMask_Main(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width) {
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    if (WIDTH <= 1920)
        cmk_FilterMask_Main_ReadAhead_Packed<WIDTH, BotBase>(surfIn, surfBadMC, Height, threadid);
    else
        cmk_FilterMask_Main_ReadAhead_Reuse_Packed<WIDTH, BotBase>(surfIn, surfBadMC, Height, threadid);
}

// main template to select fastest method for given width
// variable width version
// because number of threads is larger than 511, we change linear threadid to a matrix with width of THREADYSCALE.
template <uint MAXWIDTH, int BotBase, uint THREADYSCALE>
void _GENX_MAIN_ cmk_FilterMask_Main_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width) {
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    if (MAXWIDTH <= 1920)
        cmk_FilterMask_Main_ReadAhead_Packed_VarWidth<MAXWIDTH, BotBase>(surfIn, surfBadMC, Height, threadid, Width);
    else
        cmk_FilterMask_Main_ReadAhead_Reuse_Packed_VarWidth<MAXWIDTH, BotBase>(surfIn, surfBadMC, Height, threadid, Width);
}

// process two fields in same kernel
// a little faster than 2 seperate kernels.
// because number of threads is larger than 511, we change linear threadid to a matrix with width of THREADYSCALE.
template <uint WIDTH, uint THREADYSCALE>
void _GENX_MAIN_ cmk_FilterMask_Main_2Fields(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width) {
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    if (WIDTH <= 1920) {
        if (threadid % 2 == 0) {
            cmk_FilterMask_Main_ReadAhead_Packed<WIDTH, 0>(surfIn, surfBadMC, Height, threadid >> 1);
        }
        else {
            cmk_FilterMask_Main_ReadAhead_Packed<WIDTH, 1>(surfIn2, surfBadMC, Height, threadid >> 1);
        }
    }
    else {
        if (threadid % 2 == 0) {
            cmk_FilterMask_Main_ReadAhead_Reuse_Packed<WIDTH, 0>(surfIn, surfBadMC, Height, threadid >> 1);
        }
        else {
            cmk_FilterMask_Main_ReadAhead_Reuse_Packed<WIDTH, 1>(surfIn2, surfBadMC, Height, threadid >> 1);
        }
    }
}

// process two fields in same kernel
// a little faster than 2 seperate kernels.
// variable width version
// because number of threads is larger than 511, we change linear threadid to a matrix with width of THREADYSCALE.
template <uint MAXWIDTH, uint THREADYSCALE>
void _GENX_MAIN_ cmk_FilterMask_Main_2Fields_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width) {
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    if (MAXWIDTH <= 1920) {
        if (threadid % 2 == 0) {
            cmk_FilterMask_Main_ReadAhead_Packed_VarWidth<MAXWIDTH, 0>(surfIn, surfBadMC, Height, threadid >> 1, Width);
        }
        else {
            cmk_FilterMask_Main_ReadAhead_Packed_VarWidth<MAXWIDTH, 1>(surfIn2, surfBadMC, Height, threadid >> 1, Width);
        }
    }
    else {
        if (threadid % 2 == 0) {
            cmk_FilterMask_Main_ReadAhead_Reuse_Packed_VarWidth<MAXWIDTH, 0>(surfIn, surfBadMC, Height, threadid >> 1, Width);
        }
        else {
            cmk_FilterMask_Main_ReadAhead_Reuse_Packed_VarWidth<MAXWIDTH, 1>(surfIn2, surfBadMC, Height, threadid >> 1, Width);
        }
    }
}
#endif //_BUILD_LOW_EDGE_MASK_GENX_H_