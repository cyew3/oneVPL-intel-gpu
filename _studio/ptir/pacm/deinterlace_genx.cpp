//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include <cm/cm.h>
//#include <cm/cmtl.h>
#include "common_genx.h"
#include "deinterlace_genx.h"
#include "cmut/cmutility_genx.h"
#include "blocky_io_genx.h"

#include "deinterlace_buildlowedgemask_genx.h"

constexpr uint GetBestReadWidth(uint x) {
    return (x <= 4) ? 4 : ((x <= 8) ? 8 : (x < 16 ? 16 : 32));
}


inline constexpr uint divUp(uint x, uint y) {
    return (x + y - 1) / y;
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, uint IS_TOP_FIELD, CmSurfacePlaneIndex PLANEID>
inline _GENX_ void undo2frame_plane(SurfaceIndex srcFrameSurfaceId, SurfaceIndex srcFrameSurfaceId2,
    SurfaceIndex dstFrameSurfaceId)
{
    uint x = get_thread_origin_x() * PLANE_WIDTH;
    uint y = get_thread_origin_y() * PLANE_HEIGHT;

    matrix<uchar, PLANE_HEIGHT, PLANE_WIDTH> dstBlock;
    matrix<uchar, PLANE_HEIGHT / 2, PLANE_WIDTH> srcBlock;
    matrix<uchar, PLANE_HEIGHT / 2, PLANE_WIDTH> srcBlock2;


#ifdef CMRT_EMU
    y = (y >> 1) << 2;
#endif

    if (IS_TOP_FIELD) {
        read_plane(TOP_FIELD(srcFrameSurfaceId), PLANEID, x, y >> 1, srcBlock);
        read_plane(BOTTOM_FIELD(srcFrameSurfaceId2), PLANEID, x, y >> 1, srcBlock2);
        cmut::top_field_ref(dstBlock) = srcBlock;
        cmut::bot_field_ref(dstBlock) = srcBlock2;
    }
    else {
        read_plane(BOTTOM_FIELD(srcFrameSurfaceId), PLANEID, x, y >> 1, srcBlock);
        read_plane(TOP_FIELD(srcFrameSurfaceId2), PLANEID, x, y >> 1, srcBlock2);
        cmut::bot_field_ref(dstBlock) = srcBlock;
        cmut::top_field_ref(dstBlock) = srcBlock2;
    }

    write_plane(dstFrameSurfaceId, PLANEID, x, y, dstBlock);
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, uint IS_TOP_FIELD>
void _GENX_MAIN_ cmk_undo2frame_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex srcFrameSurfaceId2, SurfaceIndex dstFrameSurfaceId)
{
    undo2frame_plane<PLANE_WIDTH, PLANE_HEIGHT, IS_TOP_FIELD, GENX_SURFACE_Y_PLANE>(scrFrameSurfaceId, srcFrameSurfaceId2, dstFrameSurfaceId);
    undo2frame_plane<PLANE_WIDTH, PLANE_HEIGHT / 2, IS_TOP_FIELD, GENX_SURFACE_UV_PLANE>(scrFrameSurfaceId, srcFrameSurfaceId2, dstFrameSurfaceId);
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, CmSurfacePlaneIndex PLANEID>
inline _GENX_ void cmk_DeinterlaceHBorder_YUVPlane(SurfaceIndex surfaceId, int topBolder, int widthIndex, uint topField, uint WIDTH, uint HEIGHT)
{        
    matrix<uchar, PLANE_HEIGHT + 1, PLANE_WIDTH> srcBlock;
    matrix<uchar, PLANE_HEIGHT, PLANE_WIDTH> dstBlock;

    if (topField && topBolder)
    {
        read_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, 0, srcBlock);

#pragma unroll
        for (uint i = 1; i < PLANE_HEIGHT; i += 2) 
        {
            srcBlock.row(i) = cm_quot((srcBlock.row(i-1) + srcBlock.row(i+1)),2);       
        }

#pragma unroll
        for (uint i = 0; i < PLANE_HEIGHT; i++) 
        {
            dstBlock.row(i) = srcBlock.row(i);
        }
        write_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, 0, dstBlock);
    }    
    else if (topField && !topBolder)
    {
        read_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, HEIGHT - PLANE_HEIGHT - 1, srcBlock);

#pragma unroll
        for (uint i = 2; i < PLANE_HEIGHT; i += 2) 
        {
            srcBlock.row(i) = cm_quot((srcBlock.row(i-1) + srcBlock.row(i+1)),2);       
        }
        srcBlock.row(PLANE_HEIGHT) = srcBlock.row(PLANE_HEIGHT-1);        

        write_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, HEIGHT - PLANE_HEIGHT - 1, srcBlock);
    }
    else if (!topField && topBolder)
    {
        read_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, 0, srcBlock);

        srcBlock.row(0) = srcBlock.row(1); 
#pragma unroll
        for (uint i = 2; i < PLANE_HEIGHT; i += 2) 
        {
            srcBlock.row(i) = cm_quot((srcBlock.row(i-1) + srcBlock.row(i+1)),2);       
        }

#pragma unroll
        for (uint i = 0; i < PLANE_HEIGHT; i++) 
        {
            dstBlock.row(i) = srcBlock.row(i);
        }
        write_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, 0, dstBlock);
    }
    else if (!topField && !topBolder)
    {
        read_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, HEIGHT - PLANE_HEIGHT - 1, srcBlock);

#pragma unroll
        for (uint i = 1; i < PLANE_HEIGHT; i += 2) 
        {
            srcBlock.row(i) = cm_quot((srcBlock.row(i-1) + srcBlock.row(i+1)),2);       
        }

        write_plane(surfaceId, PLANEID, widthIndex * PLANE_WIDTH, HEIGHT - PLANE_HEIGHT - 1, srcBlock);
    }
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, CmSurfacePlaneIndex PLANEID>
inline _GENX_ void cmk_DeinterlaceVBorder_YPlane(SurfaceIndex surfaceId, int leftBolder, int heightIndex, uint topField, uint WIDTH, uint HEIGHT)
{
    int x_pos = 0;
    int y_pos = 0;

    matrix<uchar, PLANE_HEIGHT + 1, PLANE_WIDTH * 2> srcBlock;

    if (topField)
    {
        y_pos = heightIndex * PLANE_HEIGHT + PLANE_HEIGHT;
    }
    else
    {
        y_pos = heightIndex * PLANE_HEIGHT + PLANE_HEIGHT - 1;
    }


    if (leftBolder)
    {
        x_pos = 0;   
    }
    else
    {
        x_pos = WIDTH - PLANE_WIDTH * 2;
    }

    read_plane(surfaceId, PLANEID, x_pos, y_pos, srcBlock);

    int xShift = leftBolder? 0:PLANE_WIDTH - 1;
    matrix_ref<uchar, PLANE_HEIGHT + 1, PLANE_WIDTH + 1> targetBlock = 
        srcBlock.template select<PLANE_HEIGHT + 1, 1, PLANE_WIDTH + 1, 1>(0, xShift);

#pragma unroll
    for (uint i = 1; i < PLANE_HEIGHT; i += 2) 
    {
        targetBlock.row(i) = cm_quot((targetBlock.row(i-1) + targetBlock.row(i+1)),2);
    }

    write_plane(surfaceId, PLANEID, x_pos, y_pos, srcBlock); 
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, CmSurfacePlaneIndex PLANEID>
inline _GENX_ void cmk_DeinterlaceVBorder_UVPlane(SurfaceIndex surfaceId, int leftBolder, int heightIndex, uint topField, uint WIDTH, uint HEIGHT)
{
    int x_pos = 0;
    int y_pos = 0;

    matrix<uchar, PLANE_HEIGHT + 1, PLANE_WIDTH> srcBlock;
    matrix<uchar, 1, PLANE_WIDTH> dstBlock;

    if (topField)
    {
        y_pos = heightIndex * PLANE_HEIGHT;
    }
    else
    {
        y_pos = heightIndex * PLANE_HEIGHT + 1;
    }

    if (leftBolder)
    {
        x_pos = 0;
    }
    else
    {
        x_pos = WIDTH - PLANE_WIDTH;
    }

    read_plane(surfaceId, PLANEID, x_pos, y_pos, srcBlock);

#pragma unroll
    for (uint i = 1; i < PLANE_HEIGHT; i += 2) 
    {
        srcBlock.row(i) = cm_quot((srcBlock.row(i-1) + srcBlock.row(i+1)),2);
    }

    dstBlock.row(0) = srcBlock.row(1);

    write_plane(surfaceId, PLANEID, x_pos, y_pos+1, dstBlock);
}

template <uint PLANE_WIDTH, uint PLANE_HEIGHT, uint TOP_FIELD>
void _GENX_MAIN_ cmk_DeinterlaceBorder(SurfaceIndex inputSurface, uint WIDTH, uint HEIGHT)
{
    int threadIndex1D = get_thread_origin_x() + get_thread_origin_y() * 32;

    unsigned int spaceHeightSeg1 = divUp(WIDTH,     PLANE_WIDTH);        // Top/Bottom border of PlaneY
    unsigned int spaceHeightSeg2 = divUp(HEIGHT,    PLANE_HEIGHT) - 2;   // Left/Right border of PlaneY
    unsigned int spaceHeightSeg3 = divUp(WIDTH,     PLANE_WIDTH);        // Top/Bottom border of PlaneUV
    unsigned int spaceHeightSeg4 = divUp(HEIGHT/2,  PLANE_HEIGHT/2) - 1; // Left/Right border of PlaneUV
    unsigned int totalYPlaneSeg = spaceHeightSeg1 + spaceHeightSeg2;
    unsigned int totalHeight = spaceHeightSeg1 + spaceHeightSeg2 + spaceHeightSeg3 + spaceHeightSeg4;

    int subInY = threadIndex1D >> 1;
    int inX = threadIndex1D & 1;

    if (subInY > totalHeight-1) return;

    if (subInY < totalYPlaneSeg)
    {
        if (subInY < spaceHeightSeg1)
        {
            cmk_DeinterlaceHBorder_YUVPlane<PLANE_WIDTH, PLANE_HEIGHT, GENX_SURFACE_Y_PLANE>
                (inputSurface, inX, subInY, TOP_FIELD, WIDTH, HEIGHT);
        }
        else
        {       
            subInY = subInY - spaceHeightSeg1;
            cmk_DeinterlaceVBorder_YPlane<PLANE_WIDTH, PLANE_HEIGHT, GENX_SURFACE_Y_PLANE>
                (inputSurface, inX, subInY, TOP_FIELD, WIDTH, HEIGHT);
        }
    }
    else
    {
        subInY = subInY - totalYPlaneSeg;
        if (subInY < spaceHeightSeg3)
        {
            cmk_DeinterlaceHBorder_YUVPlane<PLANE_WIDTH, PLANE_HEIGHT/2, GENX_SURFACE_UV_PLANE>
                (inputSurface, inX, subInY, TOP_FIELD, WIDTH, HEIGHT / 2);
        }
        else
        {    
            subInY = subInY - spaceHeightSeg3;
            cmk_DeinterlaceVBorder_UVPlane<PLANE_WIDTH, PLANE_HEIGHT/2, GENX_SURFACE_UV_PLANE>
                (inputSurface, inX, subInY, TOP_FIELD, WIDTH, HEIGHT / 2);
        }
    }
}

template <int WIDTH, int HEIGHT, int INWIDTH, short PROT, int DIR, int XSTEP, int BotBase, CmSurfacePlaneIndex PLANEID>
inline _GENX_ void
    FixEdgeDirectionalIYUV_Plane_PackedBadMC(SurfaceIndex surfFrame, SurfaceIndex surfBadMC, int blockx, int blocky) {
        const uint outx = blockx*WIDTH; // left edge is not taken cared
        const uint outy = blocky*HEIGHT * 2 + (BotBase ? 0 : 1) + DIR;
        const uint inx = outx - DIR*XSTEP; //cover dir search
        const uint iny = outy - 1; //out is based on interlaced lines, in is based on normal lines.
        const uint outyby2 = outy>>1;

        matrix<uchar, HEIGHT * 2 + 1, INWIDTH> inBlock;
        matrix<ushort, HEIGHT, WIDTH / 16> inBadMCBlockPacked;
        cmut::cm_read_plane_field<PLANEID, BotBase>(surfBadMC, outx >> 3, outyby2, inBadMCBlockPacked);

        if (inBadMCBlockPacked.any()) {
            blocky_read_plane(surfFrame, PLANEID, inx, iny, inBlock);

            FixEdgeDirectionalIYUV_PackedBadMC<-DIR*XSTEP, DIR*XSTEP, XSTEP, DIR*XSTEP>(inBadMCBlockPacked.select_all(), inBlock.select_all(), PROT);

            if (BotBase) {
                write_plane(TOP_FIELD(surfFrame), PLANEID, outx, outyby2, inBlock.template select<HEIGHT, 2, WIDTH, 1>(1, 4));
            }
            else {
                write_plane(BOTTOM_FIELD(surfFrame), PLANEID, outx, outyby2, inBlock.template select<HEIGHT, 2, WIDTH, 1>(1, 4));
            }
        }
}

template <int VHEIGHT, int VWIDTH, int BotBase>
inline _GENX_ void
    cmk_FixEdgeDirectionalIYUV_Main(SurfaceIndex surfIn, SurfaceIndex surfBadMC) {
        int blockx = get_thread_origin_x();
        int blocky = get_thread_origin_y();
#define YPROT 15
#define UVPROT 15
  FixEdgeDirectionalIYUV_Plane_PackedBadMC<VWIDTH * 2, VHEIGHT, 32, UVPROT, 2, 2, BotBase, GENX_SURFACE_UV_PLANE>(surfIn, surfBadMC, blockx, blocky);
        FixEdgeDirectionalIYUV_Plane_PackedBadMC<VWIDTH * 2, VHEIGHT * 2, 32, YPROT, 4, 1, BotBase, GENX_SURFACE_Y_PLANE>(surfIn, surfBadMC, blockx, blocky);
}

void _GENX_MAIN_ cmk_FixEdgeDirectionalIYUV_Main_Bottom_Instance(SurfaceIndex surfIn, SurfaceIndex surfBadMC) {
    cmk_FixEdgeDirectionalIYUV_Main<3, 8, 1>(surfIn, surfBadMC);
}
void _GENX_MAIN_ cmk_FixEdgeDirectionalIYUV_Main_Top_Instance(SurfaceIndex surfIn, SurfaceIndex surfBadMC) {
    cmk_FixEdgeDirectionalIYUV_Main<3, 8, 0>(surfIn, surfBadMC);
}

template<int IS_TOP_FIELD, typename T, uint HEIGHT, uint WIDTH>
inline _GENX_ matrix<T, HEIGHT, WIDTH> median_deinterlace_block(matrix_ref<T, HEIGHT + 2, WIDTH> srcFrame)
{
    enum
    {
        FRAME_TOP_EXTRA = 1,
        TOP_FIELD_FIRST_ROW = 0,
        BOT_FIELD_FIRST_ROW = 1,
        DEINTERLACED_START_ROW = IS_TOP_FIELD ? BOT_FIELD_FIRST_ROW : TOP_FIELD_FIRST_ROW,
        RAW_START_ROW = IS_TOP_FIELD ? TOP_FIELD_FIRST_ROW : BOT_FIELD_FIRST_ROW
    };

    auto prevRows = srcFrame.template select<HEIGHT / 2, 2, WIDTH, 1>(DEINTERLACED_START_ROW + FRAME_TOP_EXTRA - 1, 0);
    auto deinterlacedRows = srcFrame.template select<HEIGHT / 2, 2, WIDTH, 1>(DEINTERLACED_START_ROW + FRAME_TOP_EXTRA, 0);
    auto nextRows = srcFrame.template select<HEIGHT / 2, 2, WIDTH, 1>(DEINTERLACED_START_ROW + FRAME_TOP_EXTRA + 1, 0);

    matrix<T, HEIGHT, WIDTH> dstFrame;
    dstFrame.template select<HEIGHT / 2, 2, WIDTH, 1>(DEINTERLACED_START_ROW, 0) = cmut::cm_median<HEIGHT / 2, WIDTH>(prevRows, deinterlacedRows, nextRows);
    dstFrame.template select<HEIGHT / 2, 2, WIDTH, 1>(RAW_START_ROW, 0) = srcFrame.template select<HEIGHT / 2, 2, WIDTH, 1>(RAW_START_ROW + FRAME_TOP_EXTRA, 0);

    return dstFrame;
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, uint BORDER, CmSurfacePlaneIndex PLANE>
inline _GENX_ void media_deinterlace_plane(SurfaceIndex srcFrameSurfaceId,
                                           SurfaceIndex dstTopFieldSurfaceId,
                                           SurfaceIndex dstBotFieldSurfaceId)
{
    enum
    {
        FRAME_TOP_EXTRA = 1,
        BLOCK_WIDTH = PLANE_WIDTH,
        BLOCK_HEIGHT = 8
    };

    uint x = get_thread_origin_x() * PLANE_WIDTH;
    uint y = get_thread_origin_y() * PLANE_HEIGHT;

    matrix<uchar, BLOCK_HEIGHT + 2, BLOCK_WIDTH> srcFrame;
    matrix<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> dstFrame;

    if (y == 0) {
        //Copy border lines to dst
        cmut::cm_copy_plane<0, BORDER, PLANE_WIDTH, PLANE>(dstTopFieldSurfaceId, srcFrameSurfaceId, x);
        cmut::cm_copy_plane<0, BORDER, PLANE_WIDTH, PLANE>(dstBotFieldSurfaceId, srcFrameSurfaceId, x);

        y = BORDER;
    }

    //Prepare 2 rows in the src buffer
    cmut::cm_read_plane<PLANE>(srcFrameSurfaceId, x, y - 1, srcFrame.template select<2, 1, BLOCK_WIDTH, 1>(0, 0));

    for (uint index = 0; index < PLANE_HEIGHT; index += BLOCK_HEIGHT) {
        cmut::cm_read_plane<PLANE>(srcFrameSurfaceId, x, y + 1, srcFrame.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(2, 0));

        dstFrame = median_deinterlace_block<true, uchar, BLOCK_HEIGHT, BLOCK_WIDTH>(srcFrame);
        cmut::cm_write_plane<PLANE>(dstTopFieldSurfaceId, x, y, dstFrame);

        dstFrame = median_deinterlace_block<false, uchar, BLOCK_HEIGHT, BLOCK_WIDTH>(srcFrame);
        cmut::cm_write_plane<PLANE>(dstBotFieldSurfaceId, x, y, dstFrame);

        //Copy last 2 lines to first 2 lines
        srcFrame.template select<2, 1, BLOCK_WIDTH, 1>(0, 0) = srcFrame.template select<2, 1, BLOCK_WIDTH, 1>(BLOCK_HEIGHT, 0);

        y += BLOCK_HEIGHT;
    }
}

template<int IS_TOP_FIELD, uint PLANE_WIDTH, uint PLANE_HEIGHT, uint BORDER, CmSurfacePlaneIndex PLANE>
inline _GENX_ void media_deinterlace_plane_single_field(SurfaceIndex srcFrameSurfaceId,
                                                        SurfaceIndex dstFieldSurfaceId)
{
    enum
    {
        FRAME_TOP_EXTRA = 1,
        BLOCK_WIDTH = PLANE_WIDTH,
        BLOCK_HEIGHT = 8
    };

    uint x = get_thread_origin_x() * PLANE_WIDTH;
    uint y = get_thread_origin_y() * PLANE_HEIGHT;

    matrix<uchar, BLOCK_HEIGHT + 2, BLOCK_WIDTH> srcFrame;
    matrix<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> dstFrame;

    if (y == 0) {
        //Copy border lines to dst
        cmut::cm_copy_plane<0, BORDER, PLANE_WIDTH, PLANE>(dstFieldSurfaceId, srcFrameSurfaceId, x);

        y = BORDER;
    }

    //Prepare 2 rows in the src buffer
    cmut::cm_read_plane<PLANE>(srcFrameSurfaceId, x, y - 1, srcFrame.template select<2, 1, BLOCK_WIDTH, 1>(0, 0));

    for (uint index = 0; index < PLANE_HEIGHT; index += BLOCK_HEIGHT) {
        cmut::cm_read_plane<PLANE>(srcFrameSurfaceId, x, y + 1, srcFrame.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(2, 0));

        dstFrame = median_deinterlace_block<IS_TOP_FIELD, uchar, BLOCK_HEIGHT, BLOCK_WIDTH>(srcFrame);
        cmut::cm_write_plane<PLANE>(dstFieldSurfaceId, x, y, dstFrame);

        //Copy last 2 lines to first 2 lines
        srcFrame.template select<2, 1, BLOCK_WIDTH, 1>(0, 0) = srcFrame.template select<2, 1, BLOCK_WIDTH, 1>(BLOCK_HEIGHT, 0);

        y += BLOCK_HEIGHT;
    }
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT>
_GENX_MAIN_ void cmk_media_deinterlace_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstTopFieldSurfaceId, SurfaceIndex dstBotFieldSurfaceId)
{
    media_deinterlace_plane<PLANE_WIDTH, PLANE_HEIGHT, 4, GENX_SURFACE_Y_PLANE>(scrFrameSurfaceId, dstTopFieldSurfaceId, dstBotFieldSurfaceId);
    media_deinterlace_plane<PLANE_WIDTH, PLANE_HEIGHT / 2, 2, GENX_SURFACE_UV_PLANE>(scrFrameSurfaceId, dstTopFieldSurfaceId, dstBotFieldSurfaceId);
}

template<int IS_TOP_FIELD, uint PLANE_WIDTH, uint PLANE_HEIGHT>
_GENX_MAIN_ void cmk_media_deinterlace_single_field_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFieldSurfaceId)
{
    static_assert (0 == IS_TOP_FIELD || 1 == IS_TOP_FIELD, "IS_TOP_FIELD can only be 0 or 1");
    media_deinterlace_plane_single_field<IS_TOP_FIELD, PLANE_WIDTH, PLANE_HEIGHT, 4, GENX_SURFACE_Y_PLANE>(scrFrameSurfaceId, dstFieldSurfaceId);
    media_deinterlace_plane_single_field<IS_TOP_FIELD, PLANE_WIDTH, PLANE_HEIGHT / 2, 2, GENX_SURFACE_UV_PLANE>(scrFrameSurfaceId, dstFieldSurfaceId);
}

template<typename T, uint R, uint C>
void _GENX_ inline cm_sad(matrix_ref<uchar, R, C> a, matrix_ref<uchar, R, C> b, vector<T, C> &sum) {
#pragma unroll
    for (int i = 0; i<R; i++) {
        sum = cm_sada2<ushort>(a.row(i), b.row(i), sum);
    }
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, CmSurfacePlaneIndex PLANE>
inline _GENX_ void sad_plane(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId)
{
    enum
    {
        BLOCK_WIDTH = 32,
        BLOCK_HEIGHT = 8
    };

    static_assert((PLANE_WIDTH % BLOCK_WIDTH) == 0, "PLANE_WIDTH is not compitable to BLOCK_WIDTH");
    static_assert((PLANE_HEIGHT % BLOCK_HEIGHT) == 0, "PLANE_HEIGHT is not compitable to BLOCK_HEIGHT");

    matrix<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> currFrame;
    matrix<uchar, BLOCK_HEIGHT, BLOCK_WIDTH> prevFrame;
    matrix<uint, 1, 8> sad = 0;

    uint x = get_thread_origin_x() * PLANE_WIDTH;

    vector<ushort, BLOCK_WIDTH> interStraightTopSAD=0;
    vector<ushort, BLOCK_WIDTH> interStraightBotSAD=0;
    vector<ushort, BLOCK_WIDTH> interCrossTopSAD=0;
    vector<ushort, BLOCK_WIDTH> interCrossBotSAD=0;
    vector<ushort, BLOCK_WIDTH> intraSAD=0;


#pragma unroll
    for (uint pc = 0; pc < PLANE_WIDTH; pc += BLOCK_WIDTH) {
        uint y = get_thread_origin_y() * PLANE_HEIGHT;
#pragma unroll
        for (uint pr = 0; pr < PLANE_HEIGHT; pr += BLOCK_HEIGHT) {
            cmut::cm_read_plane<PLANE>(currFrameSurfaceId, x, y, currFrame);
            cmut::cm_read_plane<PLANE>(prevFrameSurfaceId, x, y, prevFrame);

            cm_sad(cmut::top_field_ref(currFrame), cmut::top_field_ref(prevFrame), interStraightTopSAD);
            cm_sad(cmut::bot_field_ref(currFrame), cmut::bot_field_ref(prevFrame), interStraightBotSAD);
            cm_sad(cmut::top_field_ref(currFrame), cmut::bot_field_ref(prevFrame), interCrossTopSAD);
            cm_sad(cmut::bot_field_ref(currFrame), cmut::top_field_ref(prevFrame), interCrossBotSAD);
            cm_sad(cmut::top_field_ref(currFrame), cmut::bot_field_ref(currFrame), intraSAD);
            y += BLOCK_HEIGHT;
        }
        x += BLOCK_WIDTH;
    }
    sad[0][0] = cm_sum<uint>(interStraightTopSAD.template select<BLOCK_WIDTH/2, 2>(0));
    sad[0][1] = cm_sum<uint>(interStraightBotSAD.template select<BLOCK_WIDTH/2, 2>(0));
    sad[0][2] = cm_sum<uint>(interCrossTopSAD   .template select<BLOCK_WIDTH/2, 2>(0));
    sad[0][3] = cm_sum<uint>(interCrossBotSAD   .template select<BLOCK_WIDTH/2, 2>(0));
    sad[0][4] = cm_sum<uint>(intraSAD           .template select<BLOCK_WIDTH/2, 2>(0));

    cmut::cm_write(sadSurfaceId, get_thread_origin_x() * 8 * 4, get_thread_origin_y(), sad);
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT>
_GENX_MAIN_ void cmk_sad_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId)
{
    sad_plane<PLANE_WIDTH, PLANE_HEIGHT, GENX_SURFACE_Y_PLANE>(currFrameSurfaceId, prevFrameSurfaceId, sadSurfaceId);
}

const int init_Data[16] = {31, 31, 63, 63, 127, 127, 255, 255, 512, 512, 1023, 1023, 2047, 2047, 4095, 4095};

template<uint PLANE_WIDTH, uint PLANE_HEIGHT, CmSurfacePlaneIndex PLANE>
inline _GENX_ void rs_plane(SurfaceIndex frameSurfaceId, SurfaceIndex rsSurfaceId, int Height)
{
    enum
    {
        BLOCK_WIDTH = 32,
        BLOCK_HEIGHT = 8,
        RS_UNIT_SIZE = 32,
        TVBLACK = 16
    };  

    static_assert((PLANE_WIDTH % BLOCK_WIDTH) == 0, "PLANE_WIDTH is not compitable to BLOCK_WIDTH");
    static_assert((PLANE_HEIGHT % BLOCK_HEIGHT) == 0, "PLANE_HEIGHT is not compitable to BLOCK_HEIGHT");

    //Output values for each 8x8 block
    matrix<int, 1, RS_UNIT_SIZE/2> rs_1 = 0;
    matrix<int, 1, RS_UNIT_SIZE/2> rs_2 = 0; 
    vector<int, RS_UNIT_SIZE/2> data(init_Data);
    vector_ref<int, RS_UNIT_SIZE/2> v_rs_1 = rs_1.row(0);
    vector_ref<int, RS_UNIT_SIZE/2> v_rs_2 = rs_2.row(0);
    vector<ushort, RS_UNIT_SIZE/2>  v_mask = 1;
    vector<int, RS_UNIT_SIZE/2>     v_diff;

    matrix<uchar, BLOCK_HEIGHT + 2, BLOCK_WIDTH> frame;
    uint x = get_thread_origin_x() * PLANE_WIDTH;

    matrix<uchar, 1, BLOCK_WIDTH> frame_topleft;
    cmut::cm_read_plane<PLANE>(frameSurfaceId, 0, get_thread_origin_y() * PLANE_HEIGHT, frame_topleft);

    //pr: plane row index, pc: plane col index
#pragma unroll
    for (uint pc = 0; pc < PLANE_WIDTH; pc += BLOCK_WIDTH) {
        uint y = get_thread_origin_y() * PLANE_HEIGHT;

        //Read first 2 lines
        cmut::cm_read_plane<PLANE>(frameSurfaceId, x, y, frame.template select<2, 1, BLOCK_WIDTH, 1>(0, 0));

#pragma unroll
        for (uint pr = 0; pr < PLANE_HEIGHT; pr += BLOCK_HEIGHT) {
            //Read rest 8 lines
            if(Height == (y + 8)/* && pr > 5*/){
                cmut::cm_read_plane<PLANE>(frameSurfaceId, x, y + 2, frame.template select<6, 1, BLOCK_WIDTH, 1>(2, 0)); //read next 6 real lines
                cmut::cm_read_plane<PLANE>(frameSurfaceId, x, y + 7, frame.template select<1, 1, BLOCK_WIDTH, 1>(8, 0)); //copy the last line
                cmut::cm_read_plane<PLANE>(frameSurfaceId, x, y + 7, frame.template select<1, 1, BLOCK_WIDTH, 1>(9, 0)); //copy the last line
            }                                               //1072+7 = 1079
            else //normal case
                cmut::cm_read_plane<PLANE>(frameSurfaceId, x, y + 2, frame.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(2, 0));

            matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> global = frame.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(0, 0) - frame.template select<BLOCK_HEIGHT, 1, BLOCK_WIDTH, 1>(1, 0);
            matrix<short, BLOCK_HEIGHT / 2, BLOCK_WIDTH> top = frame.template select<BLOCK_HEIGHT / 2, 2, BLOCK_WIDTH, 1>(0, 0) - frame.template select<BLOCK_HEIGHT / 2, 2, BLOCK_WIDTH, 1>(2, 0);
            matrix<short, BLOCK_HEIGHT / 2, BLOCK_WIDTH> bot = frame.template select<BLOCK_HEIGHT / 2, 2, BLOCK_WIDTH, 1>(1, 0) - frame.template select<BLOCK_HEIGHT / 2, 2, BLOCK_WIDTH, 1>(3, 0);

#pragma unroll
            for (uint c = 0; c < BLOCK_WIDTH; c += 8) {
#pragma unroll
                for (uint r = 0; r < BLOCK_HEIGHT; r += 8) {   

                    int global_sum   = cm_sum<int>(global.template select<8, 1, 8, 1>(r, c) * global.template select<8, 1, 8, 1>(r, c));
                    int top_sum      = cm_sum<int>(top.template select<4, 1, 8, 1>(r / 2, c) * top.template select<4, 1, 8, 1>(r / 2, c));
                    int bottom_sum   = cm_sum<int>(bot.template select<4, 1, 8, 1>(r / 2, c) * bot.template select<4, 1, 8, 1>(r / 2, c));
                    int diff_tb_abs  = cm_abs<int>(top_sum-bottom_sum);
                    int diff_gt      = global_sum-top_sum; 

                    v_mask = 1;
                    v_diff = 1;                //Rs[4] Rs[9] Rs[10]
                    v_diff(0) = global_sum;    //Rs[0]
                    v_diff(1) = top_sum;       //Rs[2]
                    v_diff(2) = bottom_sum;    //Rs[1]       
                    v_diff(3) = diff_tb_abs;   //Rs[3]
                    v_diff(5) = diff_gt;

                    if (diff_gt <= 4095) {         
                        v_mask(4) = 0;           //Rs[4]
                        v_mask(5) = 0;           //Rs[5]         
                    }
                    if (!((global_sum == 0) && frame_topleft[0][0] != TVBLACK)) {
                        v_mask(6) = 0;           //Rs[9]
                    }

                    if (global_sum < 16383) {
                        v_mask(7) = 0;          //Rs[10]
                    } 
                    v_rs_1 += v_diff * v_mask;

                    //Rs[6] Rs[7]
                    v_mask = 1;
                    v_diff = diff_gt;
                    v_diff.template select<RS_UNIT_SIZE/4, 2>(0) = 1;
                    v_mask = diff_gt > data;
                    v_rs_2 += v_diff * v_mask;

                }
            }

            y += BLOCK_HEIGHT;

            //Copy last 2 lines to first
            frame.template select<2, 1, BLOCK_WIDTH, 1>(0, 0) = frame.template select<2, 1, BLOCK_WIDTH, 1>(BLOCK_HEIGHT, 0);
        }

        x += BLOCK_WIDTH;
    }

    cmut::cm_write(rsSurfaceId, get_thread_origin_x() * RS_UNIT_SIZE * sizeof(int), get_thread_origin_y(), rs_1);
    cmut::cm_write(rsSurfaceId, get_thread_origin_x() * RS_UNIT_SIZE * sizeof(int) + RS_UNIT_SIZE/2 * sizeof(int), get_thread_origin_y(), rs_2);
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT>
_GENX_MAIN_ void cmk_rs_nv12(SurfaceIndex frameSurfaceId, SurfaceIndex rsSurfaceId, int Height)
{
    rs_plane<PLANE_WIDTH, PLANE_HEIGHT, GENX_SURFACE_Y_PLANE>(frameSurfaceId, rsSurfaceId, Height);
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT>
_GENX_MAIN_ void cmk_sad_rs_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId, SurfaceIndex rsSurfaceId, int Height)
{
    sad_plane<PLANE_WIDTH, PLANE_HEIGHT, GENX_SURFACE_Y_PLANE>(currFrameSurfaceId, prevFrameSurfaceId, sadSurfaceId);
    rs_plane<PLANE_WIDTH, PLANE_HEIGHT, GENX_SURFACE_Y_PLANE>(currFrameSurfaceId, rsSurfaceId, Height);
}

template void cmk_media_deinterlace_nv12<64, 16>(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstTopFieldSurfaceId, SurfaceIndex dstBotFieldSurfaceId);
template void cmk_media_deinterlace_single_field_nv12<1, 64, 16>(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFieldSurfaceId);
template void cmk_media_deinterlace_single_field_nv12<0, 64, 16>(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFieldSurfaceId);
template void cmk_sad_nv12<64, 8>(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId);
template void cmk_rs_nv12<64, 8>(SurfaceIndex frameSurfaceId, SurfaceIndex rsSurfaceId, int Height);
template void cmk_undo2frame_nv12<32, 8, 0>(SurfaceIndex scrFrameSurfaceId, SurfaceIndex srcFrameSurfaceId2, SurfaceIndex dstFrameSurfaceId);
template void cmk_undo2frame_nv12<32, 8, 1>(SurfaceIndex scrFrameSurfaceId, SurfaceIndex srcFrameSurfaceId2, SurfaceIndex dstFrameSurfaceId);
template void cmk_sad_rs_nv12<64, 8>(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId, SurfaceIndex rsSurfaceId, int Height);
template void cmk_DeinterlaceBorder<4U, 4U, 0U>(SurfaceIndex inputSurface, uint WIDTH, uint HEIGHT);
template void cmk_DeinterlaceBorder<4U, 4U, 1U>(SurfaceIndex inputSurface, uint WIDTH, uint HEIGHT);

template void cmk_FilterMask_Main <704, 0, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main<1920, 0, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main <704, 1, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main<1920, 1, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_2Fields <704, 32>(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_2Fields<1920, 32>(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);

template void cmk_FilterMask_Main_VarWidth <704, 0, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_VarWidth<1920, 0, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_VarWidth <704, 1, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_VarWidth<1920, 1, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_2Fields_VarWidth <704, 32>(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_2Fields_VarWidth<1920, 32>(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);

template void cmk_FilterMask_Main_VarWidth<3840, 0, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_VarWidth<3840, 1, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_2Fields_VarWidth<3840, 32>(SurfaceIndex surfTmp, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);

template void cmk_FilterMask_Main<3840, 0, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main<3840, 1, 32>(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);
template void cmk_FilterMask_Main_2Fields<3840, 32>(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);
