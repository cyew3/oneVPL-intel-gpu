// Copyright (c) 2014-2018 Intel Corporation
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

#pragma once
template <int WIDTH, int HEIGHT, int INWIDTH, short PROT, int DIR, int XSTEP, int BotBase, CmSurfacePlaneIndex PLANEID>
inline _GENX_ void
FixEdgeDirectionalIYUV_Plane_UnpackedBadMC(SurfaceIndex surfFrame, SurfaceIndex surfBadMC, int blockx, int blocky) {
    const int outx = blockx*WIDTH + DIR*XSTEP; // also deal with left edge...
    const int outy = blocky*HEIGHT * 2 + (BotBase ? 0 : 1) + DIR;
    const int inx = outx - DIR*XSTEP; //cover dir search
    const int iny = outy - 1; //out is based on interlaced lines, in is based on normal lines.

    matrix<uchar, HEIGHT * 2 + 1, INWIDTH> inBlock;
    matrix<uchar, HEIGHT, WIDTH / 8> inBadMCBlockPacked;
    matrix<uchar, HEIGHT, WIDTH> inBadMCBlock;
    matrix_ref<uchar, HEIGHT, WIDTH> inBadMCBlockField = inBadMCBlock.select_all();
#ifndef CMRT_EMU
    if (BotBase)
        read_plane(TOP_FIELD(surfBadMC), PLANEID, outx, outy >> 1, inBadMCBlock);
    else
        read_plane(BOTTOM_FIELD(surfBadMC), PLANEID, outx, outy >> 1, inBadMCBlock);
#else
    if (BotBase)
        read_plane(TOP_FIELD(surfBadMC), PLANEID, outx, (outy >> 1) << 1, inBadMCBlock);
    else
        read_plane(BOTTOM_FIELD(surfBadMC), PLANEID, outx, (outy >> 1) << 1, inBadMCBlock);

#endif
    if (inBadMCBlock.any()) {
        matrix<ushort, HEIGHT, WIDTH> inBadMC = inBadMCBlockField.template select<HEIGHT, 1, WIDTH, 1>(0, 0);
        blocky_read_plane(surfFrame, PLANEID, inx, iny, inBlock);

        FixEdgeDirectionalIYUV<-DIR*XSTEP, DIR*XSTEP, XSTEP, DIR*XSTEP>(inBadMC.select_all(), inBlock.select_all(), PROT);

        if (BotBase) {
            write_plane(TOP_FIELD(surfFrame), PLANEID, outx, outy >> 1, inBlock.template select<HEIGHT, 2, WIDTH, 1>(1, 4));
        }
        else {
            write_plane(BOTTOM_FIELD(surfFrame), PLANEID, outx, outy >> 1, inBlock.template select<HEIGHT, 2, WIDTH, 1>(1, 4));
        }

    }
}

