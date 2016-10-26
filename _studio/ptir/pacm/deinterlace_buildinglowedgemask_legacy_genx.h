//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

template <uint IMAGEWIDTH, int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV, uint BLOCKWIDTH>
void inline _GENX_ ReadTMPBLock(SurfaceIndex surfFrame, vector<uchar, BLOCKWIDTH> &tmp, int xoff, int y) {
    matrix<uchar, 3, BLOCKWIDTH*XSTEP> left, med, right;
    static_assert(IMAGEWIDTH % BLOCKWIDTH == 0, "Image Size has to be multiply of block width.");
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

    tmp = cm_FilterClip3x3(left.select_all(), med.select_all(), right.select_all()).template select<BLOCKWIDTH, XSTEP>(UV);
    if (xoff == 0)
        tmp.template select<4 / XSTEP, 1>(0) = 128;
    if (xoff == IMAGEWIDTH / XSTEP - BLOCKWIDTH)
        tmp.template select<4 / XSTEP, 1>(BLOCKWIDTH - 4 / XSTEP) = 128;
}
template <int STEP, uint WIDTH>
void inline _GENX_ fillmask2(vector_ref<uchar, WIDTH> badmc, uint x, uint curN, uint prevN, uint spacer, uchar content) {
    uint borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
    uint borderLen = borderStart * 2 + spacer;
    borderStart += spacer + curN;
    fillmask<STEP, WIDTH>(badmc, x - borderStart, borderLen, content);
}

template <int STEP, int START, uint WIDTH>
void inline _GENX_ fillmaskbits(vector_ref<uchar, WIDTH / 8> badmc, uint begin, uint length, uchar content) {
    static_assert(STEP <= 2, "only support step less than 2");
    static_assert(START<STEP, "START can not be greater than STEP");
    uchar ONES;
    if (STEP == 1)
        ONES = 0xFF;
    else if (STEP == 2 && START == 0)
        ONES = 0x55;
    else
        ONES = 0xAA;
    const int BPUCHAR = 8 / STEP;
    const int end = begin + length;
    const int beginbytepos = begin >> (4 - STEP);
    const int endbytepos = end >> (4 - STEP);
    const int beginzeros = begin % BPUCHAR;
    const int endzeros = BPUCHAR - end % BPUCHAR;

    uchar beginbyte = cm_shl<uchar>(ONES, beginzeros * STEP);
    uchar endbyte = cm_shr<uchar>(ONES, endzeros * STEP);
    if (beginbytepos == endbytepos) {
        badmc[beginbytepos] |= (beginbyte & endbyte);
    }
    else {
        badmc[beginbytepos] |= beginbyte;
        badmc[endbytepos] |= endbyte;
        for (int i = beginbytepos + 1; i<endbytepos; i++)
            badmc[i] |= ONES;
    }
}
template <int STEP, int START, uint WIDTH>
void inline _GENX_ fillmaskbits2(vector_ref<uchar, WIDTH> badmc, uint x, uint curN, uint prevN, uint spacer, uchar content) {
    uint borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
    uint borderLen = borderStart * 2 + spacer;
    borderStart += spacer + curN;
    fillmaskbits<STEP, START, WIDTH>(badmc, x - borderStart, borderLen, content);
}


template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV, uint WIDTH>
void inline _GENX_ TMP2BADMCReadOnFly(SurfaceIndex surfFrame, int y, vector_ref<uchar, WIDTH> badmc) {
    unsigned int x, i, j;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    {

        prevN = 0;
        curN = 0;
        spacer = 0;

        lastCol = 128;
        vector<uchar, 16> temp;

        ReadTMPBLock<WIDTH, BotBase, PID, XSTEP, UV>(surfFrame, temp, 0, y);
        for (x = 2; x < WIDTH - 2;)
        {
            vector<ushort, 16> mask;
            uint thisrun;
            uint count;

            //
            // process a run of empty pixels
            //

            enum {
                LOCALRUNSIZE = 16,
            };
            uchar cur = temp[x % LOCALRUNSIZE];
            uint thiscount; // count in a short run
            uint pastmask; // mask out pixel processed in short run
            uint localx;
            uint xafterrun;

            count = 0; xafterrun = x;
            while (1) {
                //get run from temp;
                localx = xafterrun % LOCALRUNSIZE;
                //get run
                pastmask = 0xffffffffU << localx;
                thiscount = cm_fbl((cm_pack_mask(temp != cur) | (0xffffffffU << LOCALRUNSIZE)) & pastmask) - localx;
                count += thiscount;

                if (thiscount + localx == LOCALRUNSIZE && x + count<WIDTH - 2) // nothing left
                {
                    xafterrun += thiscount;
                    ReadTMPBLock<WIDTH, BotBase, PID, XSTEP, UV>(surfFrame, temp, xafterrun, y);
                }
                else
                    break;
            }

            if (cur == 128)
            {
                if (spacer <= 2 && curN > 0 && prevN > 0) //different colors is neighbours
                {
                    borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
                    borderLen = borderStart * 2 + spacer;
                    borderStart += spacer + curN;
                    fillmask<8>(badmc, x - borderStart, borderLen, 1);// __stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

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

template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV, uint WIDTH>
void inline _GENX_ TMP2BADMCReadOnFlyBits(SurfaceIndex surfFrame, int y, vector_ref<uchar, WIDTH / 8> badmc) {
    unsigned int x, i, j;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    {

        prevN = 0;
        curN = 0;
        spacer = 0;

        lastCol = 128;
        vector<uchar, 16> temp;

        ReadTMPBLock<WIDTH, BotBase, PID, XSTEP, UV>(surfFrame, temp, 0, y);
        for (x = 2; x < (WIDTH / XSTEP) - 2;)
        {
            vector<ushort, 16> mask;
            uint thisrun;
            uint count;

            //
            // process a run of empty pixels
            //

            enum {
                LOCALRUNSIZE = 16,
            };
            uchar cur = temp[x % LOCALRUNSIZE];
            uint thiscount; // count in a short run
            uint pastmask; // mask out pixel processed in short run
            uint localx;
            uint xafterrun;

            count = 0; xafterrun = x;
            while (1) {
                //get run from temp;
                localx = xafterrun % LOCALRUNSIZE;
                //get run
                pastmask = 0xffffffffU << localx;
                thiscount = cm_fbl((cm_pack_mask(temp != cur) | (0xffffffffU << LOCALRUNSIZE)) & pastmask) - localx;
                count += thiscount;

                if (thiscount + localx == LOCALRUNSIZE && x + count<WIDTH - 2) // nothing left
                {
                    xafterrun += thiscount;
                    ReadTMPBLock<WIDTH, BotBase, PID, XSTEP, UV>(surfFrame, temp, xafterrun, y);
                }
                else
                    break;
            }

            if (cur == 128)
            {
                if (spacer <= 2 && curN > 0 && prevN > 0) //different colors is neighbours
                {
                    borderStart = (cm_min<uint>(curN, prevN) * 3 + 2) >> 2;
                    borderLen = borderStart * 2 + spacer;
                    borderStart += spacer + curN;
                    fillmaskbits<XSTEP, UV, WIDTH>(badmc, x - borderStart, borderLen, 1);// __stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

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
                            fillmaskbits<XSTEP, UV, WIDTH>(badmc, x - borderStart, borderLen, 1); //__stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

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

template <int BotBase, CmSurfacePlaneIndex PID, int XSTEP, int UV, uint WIDTH>
void inline _GENX_ TMP2BADMCReadOnFlyBits_void(SurfaceIndex surfFrame, int y, vector_ref<uchar, WIDTH / 8> badmc) {
    unsigned int x, i, j;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    {

        prevN = 0;
        curN = 0;
        spacer = 0;

        lastCol = 128;
        vector<uchar, 16> temp;

        ReadTMPBLock<WIDTH, BotBase, PID, XSTEP, UV>(surfFrame, temp, 0, y);
        for (x = 2; x < WIDTH - 2;)
        {
            vector<ushort, 16> mask;
            uint thisrun;
            uint count;

            //
            // process a run of empty pixels
            //

            enum {
                LOCALRUNSIZE = 16,
            };
            uchar cur = temp[x % LOCALRUNSIZE];
            uint thiscount; // count in a short run
            uint pastmask; // mask out pixel processed in short run
            uint localx;
            uint xafterrun;

            count = 0; xafterrun = x;
            while (1) {
                //get run from temp;
                localx = xafterrun % LOCALRUNSIZE;
                //get run
                pastmask = 0xffffffffU << localx;
                thiscount = cm_fbl((cm_pack_mask(temp != cur) | (0xffffffffU << LOCALRUNSIZE)) & pastmask) - localx;
                count += thiscount;

                if (thiscount + localx == LOCALRUNSIZE && x + count<WIDTH - 2) // nothing left
                {
                    xafterrun += thiscount;
                    ReadTMPBLock<WIDTH, BotBase, PID, XSTEP, UV>(surfFrame, temp, xafterrun, y);
                }
                else
                    break;
            }
            x += count;
        }
        fillmaskbits<XSTEP, UV, WIDTH>(badmc, x - 4, 8, 255);// __stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask
    }
}


void inline _GENX_ SetupTransitionMap(vector_ref<uchar, 27> TransitionMap) {
    TransitionMap[0] = 0;
    TransitionMap[1] = 3;
    TransitionMap[2] = 15;
    TransitionMap[3] = 6;
    TransitionMap[4] = 3;
    TransitionMap[5] = 9;
    TransitionMap[6] = 6;
    TransitionMap[7] = 6;
    TransitionMap[8] = 12;
    TransitionMap[9] = 21;
    TransitionMap[10] = 18;
    TransitionMap[11] = 9;
    TransitionMap[12] = 21;
    TransitionMap[13] = 18;
    TransitionMap[14] = 12;
    TransitionMap[15] = 21;
    TransitionMap[16] = 18;
    TransitionMap[17] = 15;
    TransitionMap[18] = 6;
    TransitionMap[19] = 18;
    TransitionMap[20] = 9;
    TransitionMap[21] = 21;
    TransitionMap[22] = 24;
    TransitionMap[23] = 21;
    TransitionMap[24] = 6;
    TransitionMap[25] = 24;
    TransitionMap[26] = 9;
}

template <uint WIDTH>
void inline _GENX_ TMP2BADMC2StateMachine(vector_ref<uchar, WIDTH + 16> temp, vector_ref<uchar, WIDTH> badmc) {
    unsigned int x, y, i, j;
    uint borderLen, borderStart;
    uint state = 0;
    uint inevent, transition, transitionMask;
    const int writeMask = 0x5143600; //101 0001 0100 0011 0110 0000 0000, will perform writeout, Spacer = 0, PrevN = 0;
    const int op1Mask = 0x4512520;   //100 0101 0001 0010 0101 0010 0000, will perform PrevN = CurN, CurN = inCount;
    const int op2Mask = 0x28A4896;   //010 1000 1010 0100 1000 1001 0110, will perform CurN += inCount;
    const int op3Mask = 0x1249248;   //001 0010 0100 1001 0010 0100 1000, will perform Spacer+= inCount, if spacer>=3, CurN = 0, Spacer = 0;
    vector<uchar, 27> TransitionMap;
    SetupTransitionMap(TransitionMap.select<27, 1>(0));
    {
        vector<uint, 4> data; //prevN = 1, curN = 2, spacer = 0, In = 3
        enum {
            SPACER = 0,
            PREVN = 1,
            CURN = 2,
            INCOUNT = 3,
        };
        uint& prevN = data[PREVN];
        uint& curN = data[CURN];
        uint& spacer = data[SPACER];
        uint& inCount = data[INCOUNT];
        data = 0;

        for (x = 2; x < WIDTH - 2;)
        {
            uchar cur = temp[x];

            inevent = (cur >> 6) ^ 2; // 128->0, 10->2, 255->1
            uint thisrun;
            inCount = cm_fbl(cm_pack_mask(temp.template select<16, 1>(x) != cur) | 0xffff0000);
            //inCount = 0;
            //do {
            //    thisrun = cm_fbl(cm_pack_mask(temp.template select<16, 1>(x+inCount) != cur) | 0xffff0000);
            //    inCount += thisrun;
            //} while (thisrun == 16 && x+inCount<WIDTH-2);

            //inCount = 1;
            transition = (state + inevent);
            transitionMask = 1 << transition;
            state = TransitionMap[transition];
            if (transitionMask & writeMask && (inevent != 0 || spacer <= 2)) {
                fillmask2<8>(badmc, x, curN, prevN, spacer, 255);
                data.select<2, 1>(SPACER) = 0; //Spacer = 0, PrevN = 0;
            }
            if (transitionMask & op1Mask) {
                data.select<2, 1>(PREVN) = data.select<2, 1>(CURN); //PrevN = CurN, CurN = inCount;
            }
            else
                if (transitionMask & op2Mask) {
                    curN += inCount;
                }
                else
                    if (transitionMask & op3Mask) {
                        spacer += inCount;
                        if (spacer>3) {
                            data.select<2, 2>(SPACER) = 0;
                            state = 0;
                        }
                    }
            x += inCount;
        }
    }
}

template <uint WIDTH, int BotBase, uint THREADYSCALE>
void inline _GENX_ cmk_FilterMask_Main_ReadAhead_Unpacked(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height) {
    static_assert(WIDTH % 32 == 0, "WIDTH has to be multiple of 32");
    vector<uchar, WIDTH + 32> tmp;
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        //blocky_read_plane(surfIn, GENX_SURFACE_Y_PLANE, 0, y, tmp.template select<WIDTH, 1>(0));
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        vector<uchar, WIDTH> badmc = 0;
        TMP2BADMC(tmp.template select<WIDTH + 16, 1>(0), badmc.template select<WIDTH, 1>(0));
        //vector<uchar, WIDTH> badmc1=0;
        //TMP2BADMC2StateMachine(tmp.template select<WIDTH+16, 1>(0), badmc.template select<WIDTH, 1>(0));
        //if ((badmc != badmc1).any())
        //    printf("Error");

        blocky_write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, 0, y, badmc.select_all());
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        //blocky_read_plane(surfIn, GENX_SURFACE_UV_PLANE, 0, y, tmp.template select<WIDTH, 1>(0));
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        vector<uchar, WIDTH> badmc = 0;
        TMP2BADMC(tmp.template select<WIDTH / 2 + 16, 2>(0), badmc.template select<WIDTH / 2, 2>(0));
        TMP2BADMC(tmp.template select<WIDTH / 2 + 16, 2>(1), badmc.template select<WIDTH / 2, 2>(1));
        //TMP2BADMC2StateMachine(tmp.template select<WIDTH/2+16, 2>(0), badmc.template select<WIDTH/2, 2>(0));
        //TMP2BADMC2StateMachine(tmp.template select<WIDTH/2+16, 2>(1), badmc.template select<WIDTH/2, 2>(1));
        //blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, badmc.select_all());
        blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, badmc.select_all());
    }
}

template <uint WIDTH, int BotBase, uint THREADYSCALE>
void inline _GENX_ cmk_FilterMask_Main_ReadAhead_Reuse_Unpacked(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height) {
    static_assert(WIDTH % 32 == 0, "WIDTH has to be multiple of 32");
    vector<uchar, WIDTH> tmp;
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        //blocky_read_plane(surfIn, GENX_SURFACE_Y_PLANE, 0, y, tmp.template select<WIDTH, 1>(0));
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        //vector<uchar, WIDTH> badmc = 0;
        TMP2BADMC(tmp.template select<WIDTH + 16, 1>(0), tmp.template select<WIDTH, 1>(0));
        //vector<uchar, WIDTH> badmc1=0;
        //TMP2BADMC2StateMachine(tmp.template select<WIDTH+16, 1>(0), badmc.template select<WIDTH, 1>(0));
        //if ((badmc != badmc1).any())
        //    printf("Error");
        tmp.merge(0, tmp>2);
        blocky_write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, 0, y, tmp.select_all());
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        //blocky_read_plane(surfIn, GENX_SURFACE_UV_PLANE, 0, y, tmp.template select<WIDTH, 1>(0));
        ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        TMP2BADMC(tmp.template select<WIDTH / 2 + 16, 2>(0), tmp.template select<WIDTH / 2, 2>(0));
        TMP2BADMC(tmp.template select<WIDTH / 2 + 16, 2>(1), tmp.template select<WIDTH / 2, 2>(1));
        //TMP2BADMC2StateMachine(tmp.template select<WIDTH/2+16, 2>(0), badmc.template select<WIDTH/2, 2>(0));
        //TMP2BADMC2StateMachine(tmp.template select<WIDTH/2+16, 2>(1), badmc.template select<WIDTH/2, 2>(1));
        tmp.merge(0, tmp>2);
        blocky_write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, 0, y, tmp.template select<WIDTH, 1>(0));
    }
}


template <uint WIDTH, int BotBase, uint THREADYSCALE>
void inline _GENX_ cmk_FilterMask_Main_ReadOnFly_Packed(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height) {
    static_assert(WIDTH % 16 == 0, "WIDTH has to be multiple of 16");
    //vector<uchar, WIDTH + 32> tmp;
    int threadid = (get_thread_origin_y()*THREADYSCALE + get_thread_origin_x());
    int y;

    //process Y/Y/UV in each group of 3 threads.
    if (threadid % 3 != 2) {
        int ybegin = BotBase ? 8 : 5;
        int yend = Height - (BotBase ? 5 : 8);
        y = (threadid - threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;

        //ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_Y_PLANE, 1>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        //vector<uchar, WIDTH> badmc = 0;
        //TMP2BADMC(tmp.template select<WIDTH + 16, 1>(0), badmc.template select<WIDTH, 1>(0));

        vector<uchar, WIDTH / 8> badmc = 0;
        TMP2BADMCReadOnFlyBits<BotBase, GENX_SURFACE_Y_PLANE, 1, 0, WIDTH>(surfIn, y, badmc.template select<WIDTH / 8, 1>(0));
#pragma unroll
        for (int i = 0; i<WIDTH / 32; i++) {
            vector<uchar, 32> badmcexpanded;
            badmcexpanded.merge(255, 0, badmc.format<uint>()[i]);
            write_plane(surfBadMC, GENX_SURFACE_Y_PLANE, i * 32, y, badmcexpanded);
        }
        //for (int i = 0; i<WIDTH; i+=8) {
        //    if (cm_pack_mask(badmc.select<8, 1>(i) == 255) != badmc1[i/8])
        //        printf("Error");
        //}
        //
    }
    else {
        int ybegin = BotBase ? 4 : 3;
        int yend = Height / 2 - (BotBase ? 3 : 4);
        y = (threadid / 3) * 2 + (BotBase ? 0 : 1);
        if (y<ybegin || y >= yend) return;
        //ReadTMPLine<WIDTH, BotBase, GENX_SURFACE_UV_PLANE, 2>(surfIn, tmp.template select<WIDTH, 1>(0), 0, y);
        //vector<uchar, WIDTH> badmc=0;
        //TMP2BADMC(tmp.template select<WIDTH/2+16, 2>(0), badmc.template select<WIDTH/2, 2>(0));
        //TMP2BADMC(tmp.template select<WIDTH/2+16, 2>(1), badmc.template select<WIDTH/2, 2>(1));

        vector<uchar, WIDTH / 8> badmc = 0;
        TMP2BADMCReadOnFlyBits<BotBase, GENX_SURFACE_UV_PLANE, 2, 0, WIDTH>(surfIn, y, badmc.template select<WIDTH / 8, 1>(0));
        TMP2BADMCReadOnFlyBits<BotBase, GENX_SURFACE_UV_PLANE, 2, 1, WIDTH>(surfIn, y, badmc.template select<WIDTH / 8, 1>(0));
#pragma unroll
        for (int i = 0; i<WIDTH / 32; i++) {
            vector<uchar, 32> badmcexpanded;
            badmcexpanded.merge(255, 0, badmc.format<uint>()[i]);
            write_plane(surfBadMC, GENX_SURFACE_UV_PLANE, i * 32, y, badmcexpanded);
        }
        //for (int i = 0; i<WIDTH; i+=8) {
        //    if (cm_pack_mask(badmc.select<8, 1>(i) == 255) != badmc1[i/8])
        //        printf("Error");
        //}

        //
    }
}

template <uint WIDTH>
void inline _GENX_ TMP2BADMC_void(vector_ref<uchar, WIDTH + 16> temp, vector_ref<uchar, WIDTH> badmc) {
    unsigned int x, y, i, j;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    {

        prevN = 0;
        curN = 0;
        spacer = 0;

        lastCol = 128;

        for (x = 2; x < WIDTH - 2;)
        {
            vector<ushort, 16> mask;
            uint thisrun;
            uint count;

            //
            // process a run of empty pixels
            //
            uchar cur = temp[x];
            count = 0;
            do {
                thisrun = cm_fbl(cm_pack_mask(temp.template select<16, 1>(x + count) != cur) | 0xffff0000);
                count += thisrun;
            } while (thisrun == 16 && x + count<WIDTH - 2);

            x += count;
        }
        fillmask<8>(badmc, x - 8, 8, 1);// __stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask
    }
}


