//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505) 
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>



struct MiniVideoParam
{
	int4 Width;
	int4 Height;	
	int4 PicWidthInCtbs;	
	int4 PicHeightInCtbs;		

	int4 chromaFormatIdc;
	int4 MaxCUSize;		
	int4 bitDepthLuma;
	int4 saoOpt;

	float m_rdLambda;
	int4 SAOChromaFlag;
    int4 reserved1;
    int4 reserved2;
}; // sizeof == 48 bytes

_GENX_ inline
void ReadParam(SurfaceIndex SURF_PARAM, MiniVideoParam& param)
{    
    vector<uint1, 48> data_pack;
    read(SURF_PARAM, 0, data_pack);

    param.Width  = data_pack.format<int4>()[0];
    param.Height = data_pack.format<int4>()[1];
    param.PicWidthInCtbs  = data_pack.format<int4>()[2];
    param.PicHeightInCtbs = data_pack.format<int4>()[3];

    //param.chromaFormatIdc = data_pack.format<int4>()[4];
    param.MaxCUSize = data_pack.format<int4>()[5];   
    param.bitDepthLuma = data_pack.format<int4>()[6];   
    param.saoOpt = data_pack.format<int4>()[7];

    param.m_rdLambda  = data_pack.format<float>()[8];
    param.SAOChromaFlag = data_pack.format<int4>()[9];
    //param.reserved1  = data_pack.format<int4>()[10];
    //param.reserved2 = data_pack.format<int4>()[11];    
}

enum { SAO_MODE_OFF = 0, SAO_MODE_ON, SAO_MODE_MERGE, NUM_SAO_MODES };
enum { SAO_TYPE_EO_0 = 0, SAO_TYPE_EO_90, SAO_TYPE_EO_135, SAO_TYPE_EO_45, SAO_TYPE_BO, NUM_SAO_BASE_TYPES };

enum { W=32, H=8 };

_GENX_ inline void GetSign(vector_ref<uint1,W> blk1, vector_ref<uint1,W> blk2, vector_ref<int2,W> sign)
{
    static_assert(W % 16 == 0, "W must be divisible by 16");
    vector<int2,W> q;
    #pragma unroll
    for (int i = 0; i < W; i += 16) // do this by 16 elements to get add.g.f instruction instead of 2 add + cmp
        q.select<16,1>(i) = blk1.select<16,1>(i) - blk2.select<16,1>(i);
    sign = q >> 15;
    sign.merge(1, q > 0);
}

_GENX_ inline void AddOffset(vector_ref<uint1,W> recon, vector_ref<int2,4> offsets, vector_ref<int2,W> cls)
{
    vector<int2,W> offset;
    offset.merge(offsets[0], 0, cls == -2);
    offset.merge(offsets[1], cls == -1);
    offset.merge(offsets[2], cls == 1);
    offset.merge(offsets[3], cls == 2);
    recon = cm_add<uint1>(recon, offset, SAT);
}

extern "C" _GENX_MAIN_
void SaoApply(SurfaceIndex SRC, SurfaceIndex DST, SurfaceIndex PARAM, SurfaceIndex SAO_MODES)
{
    MiniVideoParam par;
    ReadParam(PARAM, par);  
       
    int2 ox = get_thread_origin_x();
    int2 oy = get_thread_origin_y();

    vector<int1,16> saoParam;
    read(SAO_MODES, (ox + oy * par.PicWidthInCtbs) * 16, saoParam);

    vector_ref<int2,4> offsets = saoParam.format<int2>().select<4,1>(2);

    vector<uint2,64> pely;
    cmtl::cm_vector_assign<uint2,64>(pely,0,1);
    pely += oy*64;
    vector<uint2,64> maskVer = (pely == 0) | (pely >= par.Height - 1);

    matrix<int2,2,W> sign;
    vector<int2,W> cls;
    vector_ref<int2,W> sign1 = sign.row(0);
    vector_ref<int2,W> sign2 = sign.row(1);

    // no sao for chroma yet, just copy
    matrix<uint1,8,32> chroma;
    #pragma unroll
    for (int row = 0; row < 32; row += 8) {
        #pragma unroll
        for (int col = 0; col < 64; col += 32) {
            read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
            write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
        }
    }

    if (saoParam[0] == SAO_MODE_OFF) {

        matrix<uint1,8,32> luma;
        #pragma unroll
        for (int row = 0; row < 64; row += 8) {
            #pragma unroll
            for (int col = 0; col < 64; col += 32) {
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, luma);
                write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, luma);
            }
        }

    } else if (saoParam[1] == SAO_TYPE_EO_90) {

        for (int row = 0; row < 64; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                matrix<uint1,H+2,W> recon;
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row-1,   recon.select<H,1,W,1>(0,0));
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row-1+H, recon.select<2,1,W,1>(H,0));

                GetSign(recon.row(0), recon.row(1), sign.row(1));
                #pragma unroll
                for (int y = 0; y < H; y++) {
                    GetSign(recon.row(y+1), recon.row(y+2), sign.row(y&1));
                    cls = sign.row(y&1) - sign.row(1-(y&1));
                    cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                    AddOffset(recon.row(y+1), offsets, cls);
                }

                write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, recon.select<H,1,W,1>(1,0));
            }
        }

    } else if (saoParam[1] == SAO_TYPE_BO) {

        vector<uint1,4> band;
        band.format<uint4>()[0] = 0x03020100;
        band += saoParam[2];
        band &= 31;

        for (int row = 0; row < 64; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                matrix<uint1,H,W> recon;
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, recon);

                matrix<uint1,H,W> reconShifted = recon >> 3;

                #pragma unroll
                for (int y = 0; y < H; y++) {
                    vector<int2,W> off;
                    off.merge(offsets[0], 0, reconShifted.row(y) == band[0]);
                    off.merge(offsets[1], reconShifted.row(y) == band[1]);
                    off.merge(offsets[2], reconShifted.row(y) == band[2]);
                    off.merge(offsets[3], reconShifted.row(y) == band[3]);
                    recon.row(y) = cm_add<uint1>(recon.row(y), off, SAT);
                }

                write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, recon);
            }
        }

    } else {
        
        vector<uint2,64> pelx;
        cmtl::cm_vector_assign<uint2,64>(pelx,0,1);
        pelx += ox*64;
        vector<uint2,64> maskHor = (pelx == 0) | (pelx >= par.Width - 1);

        if (saoParam[1] == SAO_TYPE_EO_0) {

            for (int row = 0; row < 64; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> left;
                    matrix<uint1,H,W> right;
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col,   oy*64+row, center);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col-1, oy*64+row, left);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col+1, oy*64+row, right);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), left.row(y), sign1);
                        GetSign(center.row(y), right.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, maskHor.select<W,1>(col));
                        AddOffset(center.row(y), offsets, cls);
                    }

                    write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, center);
                }
            }

        } else if (saoParam[1] == SAO_TYPE_EO_135) {

            for (int row = 0; row < 64; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> leftUp;
                    matrix<uint1,H,W> rightDown;
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col,   oy*64+row,   center);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col-1, oy*64+row-1, leftUp);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col+1, oy*64+row+1, rightDown);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), leftUp.row(y), sign1);
                        GetSign(center.row(y), rightDown.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                        cls.merge(0, maskHor.select<W,1>(col));
                        AddOffset(center.row(y), offsets, cls);
                    }

                    write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, center);
                }
            }

        } else if (saoParam[1] == SAO_TYPE_EO_45) {

            for (int row = 0; row < 64; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> leftDown;
                    matrix<uint1,H,W> rightUp;
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col,   oy*64+row,   center);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col-1, oy*64+row+1, leftDown);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col+1, oy*64+row-1, rightUp);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), leftDown.row(y), sign1);
                        GetSign(center.row(y), rightUp.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                        cls.merge(0, maskHor.select<W,1>(col));
                        AddOffset(center.row(y), offsets, cls);
                    }

                    write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, center);
                }
            }
        }
    }
}
