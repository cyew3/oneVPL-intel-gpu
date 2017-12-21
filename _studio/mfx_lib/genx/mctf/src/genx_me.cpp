/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2018 Intel Corporation. All Rights Reserved.
//
*/


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
//#include <cm/cm.h>
//#include <cm/cmtl.h>
//#include <cm/genx_vme.h>
#include "../include/genx_me_common.h"
#define MRE           0
#define COMPLEX_BIDIR 1
#define INVERTMOTION  1

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(target_gen9_5) && !defined(target_gen10) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9, target_gen9_5, target_gen10
#endif

#ifdef target_gen7_5
typedef matrix<uchar, 3, 32> UniIn;
#elif CMRT_EMU
typedef matrix<uchar, 3, 32> UniIn;
#else
typedef matrix<uchar, 4, 32> UniIn;
#endif

extern "C" _GENX_MAIN_
void MeP16_1MV_MRE(SurfaceIndex SURF_CONTROL,
	SurfaceIndex SURF_SRC_AND_REF,
	SurfaceIndex SURF_DIST16x16,
	SurfaceIndex SURF_MV16x16,
	SurfaceIndex SURF_MRE1,
	uint start_xy, uchar blSize) {
	vector<uint, 1>
		start_mbXY = start_xy;
	uint mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0];
	uint mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1];
	uint x = mbX * blSize;
	uint y = mbY * blSize;

	vector<uchar, 96> control;
	read(SURF_CONTROL, 0, control);

	uchar maxNumSu = control.format<uchar>()[56];
	uchar lenSp = control.format<uchar>()[57];
	ushort width = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
	ushort mre_width = control.format<ushort>()[33];
	ushort mre_height = control.format<ushort>()[34];
	ushort precision = control.format<ushort>()[36];

	//cm_assert(x > width);
	// read MB record data
	UniIn uniIn = 0;
	matrix<uchar, 9, 32> imeOut;
	matrix<uchar, 2, 32> imeIn = 0;
	matrix<uchar, 4, 32> fbrIn;

	// declare parameters for VME
	matrix<uint4, 16, 2> costs = 0;
	vector<short, 2>
		mvPred = 0,
		mvPred2 = 0;
	//read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here
#if MRE
	vector<short, 2> mre2d = 0;
	uint posX = mbX / mre_width;
	uint posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred = mre2d * 4;
#endif
	uchar x_r = 64;//((mre == 0) * 16) + ((mre > 0) * 48);
	uchar y_r = 32;//((mre == 0) * 16) + ((mre > 0) * 40);

	// load search path
	SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

	// M0.2
	VME_SET_UNIInput_SrcX(uniIn, x);
	VME_SET_UNIInput_SrcY(uniIn, y);

	// M0.3 various prediction parameters
	VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 8x8 16x16
											//VME_SET_DWORD(uniIn, 0, 3, 0x76a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x76: 8x8 16x16
											//VME_SET_UNIInput_BMEDisableFBR(uniIn);
											// M1.1 MaxNumMVs
	VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
	// M0.5 Reference Window Width & Height
	VME_SET_UNIInput_RefW(uniIn, x_r);//48);
	VME_SET_UNIInput_RefH(uniIn, y_r);//40);
	VME_SET_UNIInput_EarlyImeSuccessEn(uniIn);

									  // M0.0 Ref0X, Ref0Y
	vector_ref<short, 2> sourceXY = uniIn.row(0).format<short>().select<2, 1>(4);
	vector<uchar, 2> widthHeight;
	widthHeight[0] = (height >> 4) - 1;
	widthHeight[1] = (width >> 4);
	vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2, 1>(22);

	vector_ref<short, 2> ref0XY = uniIn.row(0).format<short>().select<2, 1>(0);
	SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

	vector_ref<short, 2> ref1XY = uniIn.row(0).format<short>().select<2, 1>(2);
	SetRef(sourceXY, mvPred2, searchWindow, widthHeight, ref1XY);

	// M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
	VME_SET_UNIInput_AdaptiveEn(uniIn);
	VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
	VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
	VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
	VME_SET_UNIInput_LenSP(uniIn, lenSp);
	//VME_SET_UNIInput_BiWeight(uniIn, 32);

	// M1.2 Start0X, Start0Y
	vector<int1, 2> start0 = searchWindow;
	start0 = ((start0 - 16) >> 3) & 0x0f;
	uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

	uniIn.row(1)[6] = 0x20;
	uniIn.row(1)[31] = 0x1;

	vector<short, 2>  ref0 = uniIn.row(0).format<short>().select<2, 1>(0);
#ifdef target_gen7_5
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#elif CMRT_EMU
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#else
	vector<ushort, 16> costCenter = uniIn.row(3).format<ushort>().select<16, 1>(0);
#endif
	vector<short, 2> mv16;
	matrix<uint, 1, 1> dist16x16;
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16);
#if MRE
	vector<short, 2> diff1 = cm_abs<short>(mvPred);
	diff1 = diff1 > 64;
	if (diff1.any()) {
		mvPred = 0;
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref1XY);
		vector<short, 2> mv16_0;
		matrix<uint, 2, 2> mv8_0;
		matrix<uint, 1, 1> dist16x16_0;
		run_vme_ime(uniIn, imeIn,
			VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
			SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
		VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
		VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16_0);

		mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16x16_0 < dist16x16);
		dist16x16.merge(dist16x16_0, dist16x16_0 < dist16x16);
	}
#endif

	// distortions calculated before updates (subpel, bidir search)
	write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16); //16x16 Forward SAD

	if (precision) {//QPEL
		VME_SET_UNIInput_SubPelMode(uniIn, 3);
		VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);
		SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors
		VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
		VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
		VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 3);
		matrix<uchar, 7, 32> fbrOut16x16;
		fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0]; // motion vectors 16x16
		run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);
		VME_GET_FBROutput_Rec0_16x16_Mv(fbrOut16x16, mv16);
		VME_GET_FBROutput_Dist_16x16_Bi(fbrOut16x16, dist16x16);
	}
	

	// distortions Actual complete distortion
	//write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16);

	//printf("%i\t%i\t%i\t%i\t", dist8[0], dist8[1], dist8[2], dist8[3]);
	// motion vectors
	write(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mv16);      //16x16mv Ref0
}

extern "C" _GENX_MAIN_
void MeP16_1MV_MRE_8x8(SurfaceIndex SURF_CONTROL,
	SurfaceIndex SURF_SRC_AND_REF,
	SurfaceIndex SURF_DIST8x8,
	SurfaceIndex SURF_MV8x8,
	SurfaceIndex SURF_MRE1,
	uint start_xy, uchar blSize) {
	vector<uint, 1>
		start_mbXY = start_xy;
	uint mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0];
	uint mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1];
	uint x = mbX * blSize;
	uint y = mbY * blSize;

	vector<uchar, 96> control;
	read(SURF_CONTROL, 0, control);

	uchar maxNumSu = control.format<uchar>()[56];
	uchar lenSp = control.format<uchar>()[57];
	ushort width = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
	ushort mre_width = control.format<ushort>()[33];
	ushort mre_height = control.format<ushort>()[34];
	ushort precision = control.format<ushort>()[36];


	//cm_assert(x > width);
	// read MB record data
	UniIn uniIn = 0;
	matrix<uchar, 9, 32> imeOut;
	matrix<uchar, 2, 32> imeIn = 0;
	matrix<uchar, 4, 32> fbrIn;

	// declare parameters for VME
	matrix<uint4, 16, 2> costs = 0;
	vector<short, 2>
		mvPred = 0,
		mvPred2 = 0;
	//read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here
#if MRE
	vector<short, 2> mre2d = 0;
	uint posX = mbX / mre_width;
	uint posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred = mre2d * 4;
	mre2d = cm_abs<ushort>(mre2d);
	short mre = cm_max<short>(mre2d(0), mre2d(1));
#endif
	uchar x_r = 64;//48;
	uchar y_r = 32;//40;

	// load search path
	SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

	// M0.2
	VME_SET_UNIInput_SrcX(uniIn, x);
	VME_SET_UNIInput_SrcY(uniIn, y);

	// M0.3 various prediction parameters
	//VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 8x8 16x16
	//VME_SET_DWORD(uniIn, 0, 3, 0x76a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x76: 8x8 16x16
	VME_SET_DWORD(uniIn, 0, 3, 0x77a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x77: 8x8
											//VME_SET_UNIInput_BMEDisableFBR(uniIn);
											// M1.1 MaxNumMVs
	VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
	// M0.5 Reference Window Width & Height
	VME_SET_UNIInput_RefW(uniIn, x_r);//48);
	VME_SET_UNIInput_RefH(uniIn, y_r);//40);

									  // M0.0 Ref0X, Ref0Y
	vector_ref<short, 2> sourceXY = uniIn.row(0).format<short>().select<2, 1>(4);
	vector<uchar, 2> widthHeight;
	widthHeight[0] = (height >> 4) - 1;
	widthHeight[1] = (width >> 4);
	vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2, 1>(22);

	vector_ref<short, 2> ref0XY = uniIn.row(0).format<short>().select<2, 1>(0);
	SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

	// M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
	VME_SET_UNIInput_AdaptiveEn(uniIn);
	VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
	VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
	VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
	VME_SET_UNIInput_LenSP(uniIn, lenSp);
	//VME_SET_UNIInput_BiWeight(uniIn, 32);

	// M1.2 Start0X, Start0Y
	vector<int1, 2> start0 = searchWindow;
	start0 = ((start0 - 16) >> 3) & 0x0f;
	uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

	uniIn.row(1)[6] = 0x20;
	uniIn.row(1)[31] = 0x1;

	vector<short, 2>  ref0 = uniIn.row(0).format<short>().select<2, 1>(0);
#ifdef target_gen7_5
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#elif CMRT_EMU
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#else
	vector<ushort, 16> costCenter = uniIn.row(3).format<ushort>().select<16, 1>(0);
#endif

	VME_SET_UNIInput_EarlyImeSuccessEn(uniIn);
	matrix<short, 2, 4> mv8;
	vector<uint, 4> dist8;

	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	mv8 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	dist8 = imeOut.row(7).format<ushort>().select<4, 1>(4);
#if MRE
	vector<short, 2> diff = (cm_abs<short>(mvPred)) > 64;
	if (diff.any()) {
		mvPred = 0;
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
		matrix<short, 2, 4> mv8_0;
		vector<ushort, 4> dist8_0;
		run_vme_ime(uniIn, imeIn,
			VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
			SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
		mv8_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
		dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);


		mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
		dist8.merge(dist8_0, dist8_0 < dist8);
	}
#endif
	// distortions Integer search results
	// 8x8
	write(SURF_DIST8x8, mbX * DIST_SIZE * 2, mbY * 2, dist8.format<uint, 2, 2>());     //8x8 Forward SAD
	if (precision) {//QPEL
		VME_SET_UNIInput_SubPelMode(uniIn, 3);
		VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);
		SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors
		matrix<uchar, 7, 32> fbrOut8x8;
		VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
		VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
		VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 3);
		fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // motion vectors 8x8_0
		fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // motion vectors 8x8_1
		fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // motion vectors 8x8_2
		fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // motion vectors 8x8_3
		run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, 0, 0, fbrOut8x8);
		VME_GET_FBROutput_Rec0_8x8_4Mv(fbrOut8x8, mv8.format<uint>());
		VME_GET_FBROutput_Dist_8x8_Bi(fbrOut8x8, dist8);
	}

	// distortions actual complete distortion calculation
	// 8x8
	//write(SURF_DIST8x8  , mbX * DIST_SIZE * 2  , mbY * 2, dist8.format<uint,2,2>());     //8x8 Bidir distortions

	// motion vectors
	// 8x8
	write(SURF_MV8x8, mbX * MVDATA_SIZE * 2, mbY * 2, mv8);       //8x8mvs  Ref0
}

extern "C" _GENX_MAIN_
void MeP16bi_1MV2_MRE(
	SurfaceIndex SURF_CONTROL,
	SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_SRC_AND_REF2,
	SurfaceIndex SURF_DIST16x16, SurfaceIndex SURF_MV16x16,
	SurfaceIndex SURF_MV16x16_2, SurfaceIndex SURF_MRE1,
	SurfaceIndex SURF_MRE2, uint start_xy, uchar blSize,
	char forwardRefDist, char backwardRefDist){
	vector<uint, 1>
		start_mbXY = start_xy;
    uint mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0];
    uint mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1];
    uint x = mbX * blSize;
    uint y = mbY * blSize;

    vector<uchar, 96> control;
    read(SURF_CONTROL, 0, control);

    uchar maxNumSu    = control.format<uchar>()[56];
    uchar lenSp       = control.format<uchar>()[57];
    ushort width      = control.format<ushort>()[30];
    ushort height     = control.format<ushort>()[31];
	ushort mre_width  = control.format<ushort>()[33];
	ushort mre_height = control.format<ushort>()[34];
    ushort precision  = control.format<ushort>()[36];

	//cm_assert(x < width);
    // read MB record data
    UniIn uniIn = 0;
#if COMPLEX_BIDIR
	matrix<uchar, 9, 32> imeOut;
#else
	matrix<uchar, 11, 32> imeOut;
#endif
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<short, 2>
        mvPred  = 0,
        mvPred2 = 0;
    //read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here

#if MRE
	vector<short, 2> mre2d = 0;
	uint posX = mbX / mre_width;
	uint posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
    mvPred = mre2d * 4;
	/*read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
    mvPred2 = mre2d * 4 * refDist;*/
#endif
#if COMPLEX_BIDIR
	uchar x_r = 64;
	uchar y_r = 32;
#else
	uchar x_r = 32;//((mre == 0) * 16) + ((mre > 0) * 48);
	uchar y_r = 32;//((mre == 0) * 16) + ((mre > 0) * 40);
#endif

    // load search path
	SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
#if COMPLEX_BIDIR
	VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 8x8 16x16
#else
	VME_SET_DWORD(uniIn, 0, 3, 0x76a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x76: 8x8 16x16
#endif
	//VME_SET_UNIInput_BMEDisableFBR(uniIn);
    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
    // M0.5 Reference Window Width & Height
    VME_SET_UNIInput_RefW(uniIn, x_r);//48);
    VME_SET_UNIInput_RefH(uniIn, y_r);//40);

    // M0.0 Ref0X, Ref0Y
    vector_ref<short, 2> sourceXY = uniIn.row(0).format<short>().select<2,1>(4);
    vector<uchar, 2> widthHeight;
    widthHeight[0] = (height >> 4) - 1;
    widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);

    vector_ref<short, 2> ref0XY = uniIn.row(0).format<short>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

	vector_ref<short, 2> ref1XY = uniIn.row(0).format<short>().select<2,1>(2);

    // M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
    VME_SET_UNIInput_AdaptiveEn(uniIn);
    VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
    VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
    VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
    VME_SET_UNIInput_LenSP(uniIn, lenSp);
	//VME_SET_UNIInput_BiWeight(uniIn, 32);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[6] = 0x20;
    uniIn.row(1)[31] = 0x1;

    vector<short,2>  ref0        = uniIn.row(0).format<short>().select<2, 1> (0);
#ifdef target_gen7_5
    vector<ushort,4> costCenter  = uniIn.row(1).format<ushort>().select<4, 1> (8);
#elif CMRT_EMU
    vector<ushort,4> costCenter  = uniIn.row(1).format<ushort>().select<4, 1> (8);
#else
    vector<ushort,16> costCenter = uniIn.row(3).format<ushort>().select<16, 1> (0);
#endif
	VME_SET_UNIInput_EarlyImeSuccessEn(uniIn);
	vector<short,2> mv16, mv16_2;
	matrix<uint, 1, 1> dist16x16,  dist16x16_2;
#if COMPLEX_BIDIR
	run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16);

	mvPred2 = mv16 * backwardRefDist / forwardRefDist;
	SetRef(sourceXY, mvPred2, searchWindow, widthHeight, ref1XY);
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_2);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16_2);
#else
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);

	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16);

	VME_GET_IMEOutput_Rec1_16x16_Mv(imeOut, mv16_2);
	VME_GET_IMEOutput_Rec1_16x16_Distortion(imeOut, dist16x16_2);
#endif
#if MRE	
    vector<short,2> diff1     = cm_abs<short>(mvPred);
	vector<short,2> diff2     = cm_abs<short>(mvPred2);
    diff1 = diff1 > 64;
	diff2 = diff2 > 64;
    if (diff1.any() || diff2.any()) {
        mvPred = 0;
        SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref1XY);
		vector<short,2> mv16_0, mv16_02;
		matrix<uint,2,2> mv8_0, mv8_02;
		matrix<uint, 1, 1> dist16x16_0,  dist16x16_02;
        run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
		VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
		VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16_0);


		run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
		VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_02);
		VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16_02);


        mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16x16_0 < dist16x16);
		dist16x16.merge(dist16x16_0, dist16x16_0 < dist16x16);

		mv16_2.format<uint4>().merge(mv16_02.format<uint4>(), dist16x16_02 < dist16x16_2);
		dist16x16_2.merge(dist16x16_02, dist16x16_02 < dist16x16_2);
    }
#endif
	// distortions calculated before updates (subpel, bidir search)
	write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16); //16x16 Forward SAD

    if(precision)//QPEL
	    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    else
        VME_SET_UNIInput_SubPelMode(uniIn, 0);
	VME_SET_UNIInput_BiWeight(uniIn, 32);

	VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);
	SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors
	VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
	VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    if(precision)//QPEL
	    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 3);
    else
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);

	matrix<uchar, 7, 32> fbrOut16x16;
	fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0]; // motion vectors 16x16
	fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 1) = mv16_2.format<uint4>()[0];
	run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 170, fbrOut16x16);
	VME_GET_FBROutput_Rec0_16x16_Mv(fbrOut16x16, mv16);
	VME_GET_FBROutput_Rec1_16x16_Mv(fbrOut16x16, mv16_2);
	VME_GET_FBROutput_Dist_16x16_Bi(fbrOut16x16, dist16x16);


	// distortions Actual complete distortion
	//write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16);

	//printf("%i\t%i\t%i\t%i\t", dist8[0], dist8[1], dist8[2], dist8[3]);
	// motion vectors
	write(SURF_MV16x16  , mbX * MVDATA_SIZE    , mbY    , mv16);      //16x16mv Ref0
	write(SURF_MV16x16_2, mbX * MVDATA_SIZE    , mbY    , mv16_2);    //16x16mv Ref1
}
extern "C" _GENX_MAIN_
void MeP16bi_1MV2_MRE_ext(
	SurfaceIndex SURF_CONTROL,
	SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_SRC_AND_REF2,
	SurfaceIndex SURF_DIST16x16,SurfaceIndex SURF_MV16x16,
	SurfaceIndex SURF_MV16x16_2, SurfaceIndex SURF_MRE1,
	SurfaceIndex SURF_MRE2, uint start_xy, uchar blSize,
	char forwardRefDist, char backwardRefDist) {
	vector<uint, 1>
		start_mbXY = start_xy;
	uint mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0];
	uint mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1];
	uint x = mbX * blSize;
	uint y = mbY * blSize;

	vector<uchar, 96> control;
	read(SURF_CONTROL, 0, control);

	uchar maxNumSu = control.format<uchar>()[56];
	uchar lenSp = control.format<uchar>()[57];
	ushort width = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
	ushort mre_width = control.format<ushort>()[33];
	ushort mre_height = control.format<ushort>()[34];
	ushort precision = control.format<ushort>()[36];

	//cm_assert(x > width);
	vector<short, 2>
		quad1 = 0,
		quad2 = 0,
		quad3 = 0,
		quad4 = 0;

	// read MB record data
	UniIn uniIn = 0;
#if COMPLEX_BIDIR
	matrix<uchar, 9, 32> imeOut;
#else
	matrix<uchar, 11, 32> imeOut;
#endif
	matrix<uchar, 2, 32> imeIn = 0;
	matrix<uchar, 4, 32> fbrIn;

	// declare parameters for VME
	matrix<uint4, 16, 2> costs = 0;
	vector<short, 2>
		mvPred = 0,
		mvPred2 = 0;
	//read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here
#if COMPLEX_BIDIR
#if MRE
	vector<short, 2> mre2d = 0;
	uint posX = mbX / mre_width;
	uint posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred = mre2d * 4;
	mre2d = cm_abs<ushort>(mre2d);
	short mre = cm_max<short>(mre2d(0), mre2d(1));
	read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
	mvPred2 = mre2d * 4 /** refDist*/;
	mre2d = cm_abs<ushort>(mre2d);
	short mre2 = cm_max<short>(mre2d(0), mre2d(1));
	mre = cm_max<short>(mre, mre2);
	uchar x_r = ((mre == 0) * 32) + ((mre > 0) * 48);
	uchar y_r = ((mre == 0) * 32) + ((mre > 0) * 40);
#else
	uchar x_r = 48;
	uchar y_r = 40;
#endif
#else
#if MRE
	vector<short, 2>
		mre2d = 0;
	uint
		posX = mbX / mre_width,
		posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred = mre2d * 4;
	read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
	mvPred2 = mre2d * 4 * refDist;
#endif
	uchar x_r = 32;
	uchar y_r = 32;
#endif

	// load search path
	SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

	// M0.2
	VME_SET_UNIInput_SrcX(uniIn, x);
	VME_SET_UNIInput_SrcY(uniIn, y);

	// M0.3 various prediction parameters
	//VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 8x8 16x16
	VME_SET_DWORD(uniIn, 0, 3, 0x76a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x76: 8x8 16x16
	//VME_SET_DWORD(uniIn, 0, 3, 0x77a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x77: 8x8
											// M1.1 MaxNumMVs
	VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
	// M0.5 Reference Window Width & Height
	VME_SET_UNIInput_RefW(uniIn, x_r);//48);
	VME_SET_UNIInput_RefH(uniIn, y_r);//40);

									  // M0.0 Ref0X, Ref0Y
	vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2, 1>(4);
	vector<uchar, 2> widthHeight;
	widthHeight[0] = (height >> 4) - 1;
	widthHeight[1] = (width >> 4);
	vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2, 1>(22);

	quad1[0] = (((y_r - 16) / 2) - 1) * 4;// 2;
	quad1[1] = (((x_r - 16) / 2) - 1) * 4;// 2;
	quad2[0] = -(((y_r - 16) / 2) * 4);// 2);
	quad2[1] = (((x_r - 16) / 2) - 1) * 4;// 2;
	quad3[0] = -(((y_r - 16) / 2) * 4);// 2);
	quad3[1] = -(((x_r - 16) / 2) * 4);// 2);
	quad4[0] = (((y_r - 16) / 2) - 1) * 4;// 2;
	quad4[1] = -(((x_r - 16) / 2) * 4);// 2);


	vector_ref<short, 2> ref0XY = uniIn.row(0).format<short>().select<2, 1>(0);
	vector_ref<short, 2> ref1XY = uniIn.row(0).format<short>().select<2, 1>(2);

	// M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
	VME_SET_UNIInput_AdaptiveEn(uniIn);
	VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
	VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
	VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
	VME_SET_UNIInput_LenSP(uniIn, lenSp);
	//VME_SET_UNIInput_BiWeight(uniIn, 32);

	// M1.2 Start0X, Start0Y
	vector<int1, 2> start0 = searchWindow;
	start0 = ((start0 - 16) >> 3) & 0x0f;
	uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

	uniIn.row(1)[6] = 0x20;
	uniIn.row(1)[31] = 0x1;

	vector<short, 2>  ref0 = uniIn.row(0).format<short>().select<2, 1>(0);
#ifdef target_gen7_5
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#elif CMRT_EMU
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#else
	vector<ushort, 16> costCenter = uniIn.row(3).format<ushort>().select<16, 1>(0);
#endif
	VME_SET_UNIInput_EarlyImeSuccessEn(uniIn);
	vector<short, 2> mv16, mv16_2;
	matrix<uint, 1, 1> dist16x16, dist16x16_2;
#if COMPLEX_BIDIR
	//Quadrant 1
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16);

	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_2);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16_2);

	vector<short, 2> mv16_0, mv16_2_0;
	matrix<uint, 1, 1> dist16, dist16_2;
	//Quadrant 2
	SetRef(sourceXY, mvPred + quad2, searchWindow, widthHeight, ref0XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16);
	mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16 < dist16x16);
	dist16x16.merge(dist16x16, dist16 < dist16x16);

	SetRef(sourceXY, mvPred2 + quad2, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_2_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16_2);
	mv16_2.format<uint4>().merge(mv16_2.format<uint4>(), dist16_2 < dist16x16_2);
	dist16x16_2.merge(dist16x16_2, dist16_2 < dist16x16_2);

	//Quadrant 3
	SetRef(sourceXY, mvPred + quad3, searchWindow, widthHeight, ref0XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16);
	mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16 < dist16x16);
	dist16x16.merge(dist16x16, dist16 < dist16x16);

	SetRef(sourceXY, mvPred2 + quad3, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_2_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16_2);
	mv16_2.format<uint4>().merge(mv16_2.format<uint4>(), dist16_2 < dist16x16_2);
	dist16x16_2.merge(dist16x16_2, dist16_2 < dist16x16_2);

	//Quadrant 4
	SetRef(sourceXY, mvPred + quad4, searchWindow, widthHeight, ref0XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16);
	mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16 < dist16x16);
	dist16x16.merge(dist16x16, dist16 < dist16x16);

	SetRef(sourceXY, mvPred2 + quad4, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_2_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16_2);
	mv16_2.format<uint4>().merge(mv16_2.format<uint4>(), dist16_2 < dist16x16_2);
	dist16x16_2.merge(dist16x16_2, dist16_2 < dist16x16_2);
/*#if MRE
	vector<short, 2> diff = (cm_abs<short>(mvPred)) > 64;
	if (diff.any()) {
		mvPred = 0;
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
		matrix<short, 2, 4> mv16_0;
		vector<ushort, 4> dist8_0;
		run_vme_ime(uniIn, imeIn,
			VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
			SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
		mv16_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
		dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);

		mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist8_0 < dist8);
		dist8.merge(dist8_0, dist8_0 < dist8);
	}

	diff = (cm_abs<short>(mvPred2)) > 64;
	if (diff.any()) {
		mvPred = 0;
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref1XY);
		matrix<short, 2, 4> mv16_0;
		vector<ushort, 4> dist8_0;
		run_vme_ime(uniIn, imeIn,
			VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
			SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
		mv16_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
		dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);

		mv16_2.format<uint4>().merge(mv16_0.format<uint4>(), dist8_0 < dist8_2);
		dist8_2.merge(dist8_0, dist8_0 < dist8_2);
	}
#endif*/
#else
	//quadrant1
	SetRef(sourceXY, mvPred + quad1, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad1, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16);
	VME_GET_IMEOutput_Rec1_16x16_Mv(imeOut, mv16_2);
	VME_GET_IMEOutput_Rec1_16x16_Distortion(imeOut, dist16x16_2);

	vector<short, 2>
		mv16_0,
		mv16_2_0;
	matrix<uint, 1, 1>
		dist16_0,
		dist16_2_0;
	//Quadrant 2
	SetRef(sourceXY, mvPred + quad2, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad2, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16_0);
	VME_GET_IMEOutput_Rec1_16x16_Mv(imeOut, mv16_2_0);
	VME_GET_IMEOutput_Rec1_16x16_Distortion(imeOut, dist16_2_0);
	mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16x16);
	dist16x16.merge(dist16_0, dist16_0 < dist16x16);
	mv16_2.format<uint4>().merge(mv16_2_0.format<uint4>(), dist16_2_0 < dist16x16_2);
	dist16x16.merge(dist16x16_2, dist16_2_0 < dist16x16_2);
	//Quadrant 3
	SetRef(sourceXY, mvPred + quad3, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad3, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16_0);
	VME_GET_IMEOutput_Rec1_16x16_Mv(imeOut, mv16_2_0);
	VME_GET_IMEOutput_Rec1_16x16_Distortion(imeOut, dist16_2_0);
	mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16x16);
	dist16x16.merge(dist16_0, dist16_0 < dist16x16);
	mv16_2.format<uint4>().merge(mv16_2_0.format<uint4>(), dist16_2_0 < dist16x16_2);
	dist16x16.merge(dist16x16_2, dist16_2_0 < dist16x16_2);
	//Quadrant 4
	SetRef(sourceXY, mvPred + quad4, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad4, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16_0);
	VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16_0);
	VME_GET_IMEOutput_Rec1_16x16_Mv(imeOut, mv16_2_0);
	VME_GET_IMEOutput_Rec1_16x16_Distortion(imeOut, dist16_2_0);
	mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16x16);
	dist16x16.merge(dist16_0, dist16_0 < dist16x16);
	mv16_2.format<uint4>().merge(mv16_2_0.format<uint4>(), dist16_2_0 < dist16x16_2);
	dist16x16.merge(dist16x16_2, dist16_2_0 < dist16x16_2);
#endif
	// distortions Integer search results
	// distortions calculated before updates (subpel, bidir search)
	write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16); //16x16 Forward SAD

	if (precision)//QPEL
		VME_SET_UNIInput_SubPelMode(uniIn, 3);
	else
		VME_SET_UNIInput_SubPelMode(uniIn, 0);
	VME_SET_UNIInput_BiWeight(uniIn, 32);

	VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);
	SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors
	VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
	VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
	if (precision)//QPEL
		VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 3);
	else
		VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);

	matrix<uchar, 7, 32> fbrOut16x16;
	fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0]; // motion vectors 16x16
	fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 1) = mv16_2.format<uint4>()[0];
	run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 170, fbrOut16x16);
	VME_GET_FBROutput_Rec0_16x16_Mv(fbrOut16x16, mv16);
	VME_GET_FBROutput_Rec1_16x16_Mv(fbrOut16x16, mv16_2);
	VME_GET_FBROutput_Dist_16x16_Bi(fbrOut16x16, dist16x16);


	// distortions Actual complete distortion
	//write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16);

	//printf("%i\t%i\t%i\t%i\t", dist8[0], dist8[1], dist8[2], dist8[3]);
	// motion vectors
	write(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mv16);      //16x16mv Ref0
	write(SURF_MV16x16_2, mbX * MVDATA_SIZE, mbY, mv16_2);    //16x16mv Ref1
}

extern "C" _GENX_MAIN_
void MeP16bi_1MV2_MRE_8x8(
	SurfaceIndex SURF_CONTROL,
	SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_SRC_AND_REF2,
	SurfaceIndex SURF_DIST8x8, SurfaceIndex SURF_MV8x8,
	SurfaceIndex SURF_MV8x8_2, SurfaceIndex SURF_MRE1,
	SurfaceIndex SURF_MRE2, uint start_xy, uchar blSize,
	char forwardRefDist, char backwardRefDist){
	vector<uint, 1>
		start_mbXY = start_xy;
    uint
		mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
		mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
		x   = mbX * blSize,
		y   = mbY * blSize;

    vector<uchar, 96>
		control;
    read(SURF_CONTROL, 0, control);

    uchar
		maxNumSu   = control.format<uchar>()[56],
		lenSp      = control.format<uchar>()[57];
    ushort
		width      = control.format<ushort>()[30],
		height     = control.format<ushort>()[31],
		mre_width  = control.format<ushort>()[33],
		mre_height = control.format<ushort>()[34],
		precision  = control.format<ushort>()[36];
    // read MB record data
#if CMRT_EMU
	if (x >= width)
		return;
	cm_assert(x < width);
#endif
	UniIn
		uniIn      = 0;
#if COMPLEX_BIDIR
	matrix<uchar, 9, 32>
		imeOut;
#else
	matrix<uchar, 11, 32>
		imeOut;
#endif
    matrix<uchar, 2, 32>
		imeIn = 0;
    matrix<uchar, 4, 32>
		fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2>
		costs = 0;
    vector<short, 2>
        mvPred  = 0,
        mvPred2 = 0;
    //read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here
#if COMPLEX_BIDIR
#if MRE
	vector<short, 2> mre2d = 0;
	uint posX = mbX / mre_width;
	uint posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
    mvPred = mre2d * 4;
	mre2d = cm_abs<ushort>(mre2d);
	short mre = cm_max<short>(mre2d(0),mre2d(1));
	read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
    mvPred2 = mre2d * 4 /** refDist*/;
	mre2d = cm_abs<ushort>(mre2d);
	short mre2 = cm_max<short>(mre2d(0),mre2d(1));
	mre = cm_max<short>(mre, mre2);
	uchar x_r = ((mre == 0) * 32) + ((mre > 0) * 64);
	uchar y_r = ((mre == 0) * 32) + ((mre > 0) * 32);
#else
	uchar x_r = 48;
	uchar y_r = 40;
#endif
#else
#if MRE
	vector<short, 2>
		mre2d = 0;
	uint
		posX = mbX / mre_width,
	    posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred   = mre2d * 4;
	read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
	mvPred2  = mre2d * 4 * refDist;
#endif
	uchar
		x_r = 32,
		y_r = 32;
#endif

    // load search path
	SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
	//VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 8x8 16x16
	VME_SET_DWORD(uniIn, 0, 3, 0x76a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x76: 8x8 16x16
    //VME_SET_DWORD(uniIn, 0, 3, 0x77a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x77: 8x8
	//VME_SET_UNIInput_BMEDisableFBR(uniIn);
    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
    // M0.5 Reference Window Width & Height
    VME_SET_UNIInput_RefW(uniIn, x_r);//48);
    VME_SET_UNIInput_RefH(uniIn, y_r);//40);

    // M0.0 Ref0X, Ref0Y
    vector_ref<short, 2>
		sourceXY   = uniIn.row(0).format<short>().select<2,1>(4);
    vector<uchar, 2>
		widthHeight;
    widthHeight[0] = (height >> 4) - 1;
    widthHeight[1] = (width >> 4);
    vector_ref<int1, 2>
		searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);

    vector_ref<short, 2>
		ref0XY = uniIn.row(0).format<short>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

	vector_ref<short, 2>
		ref1XY = uniIn.row(0).format<short>().select<2,1>(2);
	SetRef(sourceXY, mvPred2, searchWindow, widthHeight, ref1XY);

    // M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
    VME_SET_UNIInput_AdaptiveEn(uniIn);
    VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
    VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
    VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
    VME_SET_UNIInput_LenSP(uniIn, lenSp);
	//VME_SET_UNIInput_BiWeight(uniIn, 32);

    // M1.2 Start0X, Start0Y
    vector<int1, 2>
		start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[6] = 0x20;
    uniIn.row(1)[31] = 0x1;

    vector<short,2>  ref0        = uniIn.row(0).format<short>().select<2, 1> (0);
#ifdef target_gen7_5
    vector<ushort,4> costCenter  = uniIn.row(1).format<ushort>().select<4, 1> (8);
#elif CMRT_EMU
    vector<ushort,4> costCenter  = uniIn.row(1).format<ushort>().select<4, 1> (8);
#else
    vector<ushort,16> costCenter = uniIn.row(3).format<ushort>().select<16, 1> (0);
#endif
	VME_SET_UNIInput_EarlyImeSuccessEn(uniIn);
#if COMPLEX_BIDIR
    matrix<short,2,4> mv8, mv8_2;
#else
	matrix<uint, 2, 2> mv8, mv8_2;
#endif
    vector<uint,4> dist8, dist8_2;
#if COMPLEX_BIDIR
	run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	mv8     = imeOut.row(8).format<short>().select<8,1>(8); // 4 MVs
    dist8   = imeOut.row(7).format<ushort>().select<4,1>(4);
	vector<short, 2>
		mv16;
	VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);

#if MRE
    vector<short,2> diff     = (cm_abs<short>(mvPred)) > 64;
    if (diff.any()) {
        mvPred = 0;
        SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
	    matrix<short,2,4> mv8_0;
        vector<ushort,4> dist8_0;
        run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	    mv8_0   = imeOut.row(8).format<short>().select<8,1>(8); // 4 MVs
        dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);


        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	    dist8.merge(dist8_0, dist8_0 < dist8);
    }
#endif
#if !INVERTMOTION
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	mv8_2   = imeOut.row(8).format<short>().select<8,1>(8); // 4 MVs
    dist8_2 = imeOut.row(7).format<ushort>().select<4,1>(4);

#if MRE
	diff = (cm_abs<short>(mvPred2)) > 64;
    if (diff.any()) {
        mvPred = 0;
        SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref1XY);
        matrix<short,2,4> mv8_0;
        vector<ushort,4> dist8_0;
		run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
		mv8_0   = imeOut.row(8).format<short>().select<8,1>(8); // 4 MVs
        dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);


        mv8_2.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8_2);
	    dist8_2.merge(dist8_0, dist8_0 < dist8_2);
    }
#endif
#else
	mvPred2 = mv16 * backwardRefDist / forwardRefDist;
	SetRef(sourceXY, mvPred2, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	mv8_2 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	dist8_2 = imeOut.row(7).format<ushort>().select<4, 1>(4);
	//mv8_2 = mv8 * -1;
#endif
#else
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);

	//VME_GET_IMEOutput_Rec0_16x16_Mv(imeOut, mv16);
	VME_GET_IMEOutput_Rec0_8x8_4Mv(imeOut, mv8);
	//VME_GET_IMEOutput_Rec0_16x16_Distortion(imeOut, dist16x16);
	VME_GET_IMEOutput_Rec0_8x8_4Distortion(imeOut, dist8);

	//VME_GET_IMEOutput_Rec1_16x16_Mv(imeOut, mv16_2);
	VME_GET_IMEOutput_Rec1_8x8_4Mv(imeOut, mv8_2);
	//VME_GET_IMEOutput_Rec1_16x16_Distortion(imeOut, dist16x16_2);
	VME_GET_IMEOutput_Rec1_8x8_4Distortion(imeOut, dist8_2);
#endif


	// distortions Integer search results
	// 8x8
	write(SURF_DIST8x8  , mbX * DIST_SIZE * 2  , mbY * 2, dist8.format<uint,2,2>());     //8x8 Forward SAD


    if(precision)//QPEL
	    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    else
        VME_SET_UNIInput_SubPelMode(uniIn, 0);
	VME_SET_UNIInput_BiWeight(uniIn, 32);
	VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);
	SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors
	matrix<uchar, 7, 32> fbrOut8x8;
	VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
	VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    if(precision)//QPEL
	    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 3);
    else
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // motion vectors 8x8_0
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // motion vectors 8x8_1
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // motion vectors 8x8_2
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // motion vectors 8x8_3
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 1) = mv8_2.format<uint>()[0]; // motion vectors 8x8_2_0
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 1) = mv8_2.format<uint>()[1]; // motion vectors 8x8_2_1
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 1) = mv8_2.format<uint>()[2]; // motion vectors 8x8_2_2
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 1) = mv8_2.format<uint>()[3]; // motion vectors 8x8_2_3
	run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, 0, 170, fbrOut8x8);
    VME_GET_FBROutput_Rec0_8x8_4Mv(fbrOut8x8, mv8.format<uint>());
	VME_GET_FBROutput_Rec1_8x8_4Mv(fbrOut8x8, mv8_2.format<uint>());
    VME_GET_FBROutput_Dist_8x8_Bi(fbrOut8x8, dist8);


	// distortions actual complete distortion calculation
	// 8x8
	//write(SURF_DIST8x8  , mbX * DIST_SIZE * 2  , mbY * 2, dist8.format<uint,2,2>());     //8x8 Bidir distortions

	// motion vectors
	// 8x8
	write(SURF_MV8x8    , mbX * MVDATA_SIZE * 2, mbY * 2, mv8);       //8x8mvs  Ref0
	write(SURF_MV8x8_2  , mbX * MVDATA_SIZE * 2, mbY * 2, mv8_2);     //8x8mvs  Ref1
}

extern "C" _GENX_MAIN_
void MeP16bi_1MV2_MRE_8x8_ext(
	SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF,
	SurfaceIndex SURF_SRC_AND_REF2, SurfaceIndex SURF_DIST8x8,
	SurfaceIndex SURF_MV8x8, SurfaceIndex SURF_MV8x8_2,
	SurfaceIndex SURF_MRE1, SurfaceIndex SURF_MRE2,
	uint start_xy, uchar blSize,
	char forwardRefDist, char backwardRefDist) {
	vector<uint, 1>
		start_mbXY = start_xy;
	uint mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0];
	uint mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1];
	uint x = mbX * blSize;
	uint y = mbY * blSize;

	/*vector<uchar, 96> control;
	read(SURF_CONTROL, 0, control);

	uchar maxNumSu = control.format<uchar>()[56];
	uchar lenSp = control.format<uchar>()[57];
	ushort width = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
	ushort mre_width = control.format<ushort>()[33];
	ushort mre_height = control.format<ushort>()[34];
	ushort precision = control.format<ushort>()[36];*/

	vector<uchar, 64> control;
	read(SURF_CONTROL, 0, control);

	uchar maxNumSu = control.format<uchar>()[16];
	uchar lenSp = control.format<uchar>()[17];
	ushort width = control.format<ushort>()[10];
	ushort height = control.format<ushort>()[11];
	ushort mre_width = control.format<ushort>()[13];
	ushort mre_height = control.format<ushort>()[14];
	ushort precision = control.format<ushort>()[16];

	vector<short, 2>
		quad1 = 0,
		quad2 = 0,
		quad3 = 0,
		quad4 = 0;

	// read MB record data
	UniIn uniIn = 0;
#if COMPLEX_BIDIR
	matrix<uchar, 9, 32> imeOut;
#else
	matrix<uchar, 11, 32> imeOut;
#endif
	matrix<uchar, 2, 32> imeIn = 0;
	matrix<uchar, 4, 32> fbrIn;

	// declare parameters for VME
	matrix<uint4, 16, 2> costs = 0;
	vector<short, 2>
		mvPred = 0,
		mvPred2 = 0;
	//read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here
#if COMPLEX_BIDIR
#if MRE
	vector<short, 2> mre2d = 0;
	uint posX = mbX / mre_width;
	uint posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred = mre2d * 4;
	mre2d = cm_abs<ushort>(mre2d);
	short mre = cm_max<short>(mre2d(0), mre2d(1));
	read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
	mvPred2 = mre2d * 4 /** refDist*/;
	mre2d = cm_abs<ushort>(mre2d);
	short mre2 = cm_max<short>(mre2d(0), mre2d(1));
	mre = cm_max<short>(mre, mre2);
	uchar x_r = ((mre == 0) * 32) + ((mre > 0) * 48);
	uchar y_r = ((mre == 0) * 32) + ((mre > 0) * 40);
#else
	uchar x_r = 48;
	uchar y_r = 40;
#endif
#else
#if MRE
	vector<short, 2>
		mre2d = 0;
	uint
		posX = mbX / mre_width,
	    posY = mbY / mre_height;
	read(SURF_MRE1, posX * MVDATA_SIZE, posY, mre2d);
	mvPred    = mre2d * 4;
	read(SURF_MRE2, posX * MVDATA_SIZE, posY, mre2d);
	mvPred2   = mre2d * 4 * refDist;
#endif
	uchar x_r = 32;
	uchar y_r = 32;
#endif

	// load search path
	SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);// 64);

	// M0.2
	VME_SET_UNIInput_SrcX(uniIn, x);
	VME_SET_UNIInput_SrcY(uniIn, y);

	// M0.3 various prediction parameters
	//VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 8x8 16x16
	//VME_SET_DWORD(uniIn, 0, 3, 0x76a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x76: 8x8 16x16
	VME_SET_DWORD(uniIn, 0, 3, 0x77a00000); // BMEDisableFBR=0 InterSAD=2 SubMbPartMask=0x77: 8x8
	// M1.1 MaxNumMVs
	VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
	// M0.5 Reference Window Width & Height
	VME_SET_UNIInput_RefW(uniIn, x_r);//48);
	VME_SET_UNIInput_RefH(uniIn, y_r);//40);

	// M0.0 Ref0X, Ref0Y
	vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2, 1>(4);
	vector<uchar, 2> widthHeight;
	widthHeight[0] = (height >> 4) - 1;
	widthHeight[1] = (width >> 4);
	vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2, 1>(22);

	//quad1[0] = (((y_r - 16) / 2) - 1) * 4;// 2;
	//quad1[1] = (((x_r - 16) / 2) - 1) * 4;// 2;
	//quad2[0] = -(((y_r - 16) / 2) * 4);// 2);
	//quad2[1] = (((x_r - 16) / 2) - 1) * 4;// 2;
	//quad3[0] = -(((y_r - 16) / 2) * 4);// 2);
	//quad3[1] = -(((x_r - 16) / 2) * 4);// 2);
	//quad4[0] = (((y_r - 16) / 2) - 1) * 4;// 2;
	//quad4[1] = -(((x_r - 16) / 2) * 4);// 2); 

	quad1[0] = 0;// 2;
	quad1[1] = (((x_r - 16) / 2) - 1) * 4;// 2;
	quad2[0] = 0;// 2);
	quad2[1] = (((x_r - 16) / 2) - 1) * 4;// 2;


	vector_ref<short, 2> ref0XY = uniIn.row(0).format<short>().select<2, 1>(0);
	vector_ref<short, 2> ref1XY = uniIn.row(0).format<short>().select<2, 1>(2);

	// M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
	VME_SET_UNIInput_AdaptiveEn(uniIn);
	VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
	VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
	VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
	VME_SET_UNIInput_LenSP(uniIn, lenSp);
	//VME_SET_UNIInput_BiWeight(uniIn, 32);

	// M1.2 Start0X, Start0Y
	vector<int1, 2> start0 = searchWindow;
	start0 = ((start0 - 16) >> 3) & 0x0f;
	uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

	uniIn.row(1)[6] = 0x20;
	uniIn.row(1)[31] = 0x1;

	vector<short, 2>  ref0 = uniIn.row(0).format<short>().select<2, 1>(0);
#ifdef target_gen7_5
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#elif CMRT_EMU
	vector<ushort, 4> costCenter = uniIn.row(1).format<ushort>().select<4, 1>(8);
#else
	vector<ushort, 16> costCenter = uniIn.row(3).format<ushort>().select<16, 1>(0);
#endif
	VME_SET_UNIInput_EarlyImeSuccessEn(uniIn);
#if COMPLEX_BIDIR
	matrix<short, 2, 4> mv8, mv8_2;
#else
	matrix<uint, 2, 2> mv8, mv8_2;
#endif
	vector<uint, 4> dist8, dist8_2;
#if COMPLEX_BIDIR
	//Quadrant 1
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	mv8 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	dist8 = imeOut.row(7).format<ushort>().select<4, 1>(4);

	matrix<short, 2, 4> mv8_0;
	vector<ushort, 4> dist8_0;
	//Quadrant 2
	SetRef(sourceXY, mvPred + quad2, searchWindow, widthHeight, ref0XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	mv8_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);
	mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	dist8.merge(dist8_0, dist8_0 < dist8);

	//Quadrant 3
	//SetRef(sourceXY, mvPred + quad3, searchWindow, widthHeight, ref0XY);
	//run_vme_ime(uniIn, imeIn,
	//	VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
	//	SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	//mv8_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	//dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);
	//mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	//dist8.merge(dist8_0, dist8_0 < dist8);

	//Quadrant 4
	//SetRef(sourceXY, mvPred + quad4, searchWindow, widthHeight, ref0XY);
	//run_vme_ime(uniIn, imeIn,
	//	VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
	//	SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
	//mv8_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	//dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);
	//mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	//dist8.merge(dist8_0, dist8_0 < dist8);

#if MRE
	vector<short, 2> diff = (cm_abs<short>(mvPred)) > 64;
	if (diff.any()) {
		mvPred = 0;
		SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
		matrix<short, 2, 4> mv8_0;
		vector<ushort, 4> dist8_0;
		run_vme_ime(uniIn, imeIn,
			VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
			SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);
		mv8_0 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
		dist8_0 = imeOut.row(7).format<ushort>().select<4, 1>(4);

		mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
		dist8.merge(dist8_0, dist8_0 < dist8);
	}

#endif
	mvPred2 = -(mv8.select<2, 1, 1, 1>(0,0) + mv8.select<2, 1, 1, 1>(0, 1) + mv8.select<2, 1, 1, 1>(0, 2) + mv8.select<2, 1, 1, 1>(0, 3)) * backwardRefDist / forwardRefDist;
	SetRef(sourceXY, mvPred2, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
		SURF_SRC_AND_REF2, ref1XY, NULL, costCenter, imeOut);
	mv8_2 = imeOut.row(8).format<short>().select<8, 1>(8); // 4 MVs
	dist8_2 = imeOut.row(7).format<ushort>().select<4, 1>(4);
#else
	//quadrant1
	SetRef(sourceXY, mvPred + quad1, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad1, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_8x8_4Mv(imeOut, mv8);
	VME_GET_IMEOutput_Rec0_8x8_4Distortion(imeOut, dist8);
	VME_GET_IMEOutput_Rec1_8x8_4Mv(imeOut, mv8_2);
	VME_GET_IMEOutput_Rec1_8x8_4Distortion(imeOut, dist8_2);

	matrix<uint, 2, 2>
		mv8_0,
		mv8_2_0;
	vector<ushort, 4>
		dist8_0,
		dist8_2_0;
	//Quadrant 2
	SetRef(sourceXY, mvPred + quad2, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad2, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_8x8_4Mv(imeOut, mv8_0);
	VME_GET_IMEOutput_Rec0_8x8_4Distortion(imeOut, dist8_0);
	VME_GET_IMEOutput_Rec1_8x8_4Mv(imeOut, mv8_2_0);
	VME_GET_IMEOutput_Rec1_8x8_4Distortion(imeOut, dist8_2_0);
	mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	dist8.merge(dist8_0, dist8_0 < dist8);
	mv8_2.format<uint4>().merge(mv8_2_0.format<uint4>(), dist8_2_0 < dist8_2);
	dist8.merge(dist8_2, dist8_2_0 < dist8_2);
/*	//Quadrant 3
	SetRef(sourceXY, mvPred + quad3, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad3, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_8x8_4Mv(imeOut, mv8_0);
	VME_GET_IMEOutput_Rec0_8x8_4Distortion(imeOut, dist8_0);
	VME_GET_IMEOutput_Rec1_8x8_4Mv(imeOut, mv8_2_0);
	VME_GET_IMEOutput_Rec1_8x8_4Distortion(imeOut, dist8_2_0);
	mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	dist8.merge(dist8_0, dist8_0 < dist8);
	mv8_2.format<uint4>().merge(mv8_2_0.format<uint4>(), dist8_2_0 < dist8_2);
	dist8.merge(dist8_2, dist8_2_0 < dist8_2);
	//Quadrant 4
	SetRef(sourceXY, mvPred + quad4, searchWindow, widthHeight, ref0XY);
	SetRef(sourceXY, mvPred2 + quad4, searchWindow, widthHeight, ref1XY);
	run_vme_ime(uniIn, imeIn,
		VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC,
		SURF_SRC_AND_REF, ref0XY, ref1XY, costCenter, imeOut);
	VME_GET_IMEOutput_Rec0_8x8_4Mv(imeOut, mv8_0);
	VME_GET_IMEOutput_Rec0_8x8_4Distortion(imeOut, dist8_0);
	VME_GET_IMEOutput_Rec1_8x8_4Mv(imeOut, mv8_2_0);
	VME_GET_IMEOutput_Rec1_8x8_4Distortion(imeOut, dist8_2_0);
	mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
	dist8.merge(dist8_0, dist8_0 < dist8);
	mv8_2.format<uint4>().merge(mv8_2_0.format<uint4>(), dist8_2_0 < dist8_2);
	dist8.merge(dist8_2, dist8_2_0 < dist8_2); */
#endif



	// distortions Integer search results
	// 8x8
	write(SURF_DIST8x8, mbX * DIST_SIZE * 2, mbY * 2, dist8.format<uint, 2, 2>());     //8x8 Forward SAD


	if (precision)//QPEL
		VME_SET_UNIInput_SubPelMode(uniIn, 3);
	else
		VME_SET_UNIInput_SubPelMode(uniIn, 0);
	VME_SET_UNIInput_BiWeight(uniIn, 32);
	VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);
	SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors
	matrix<uchar, 7, 32> fbrOut8x8;
	VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
	VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
	if (precision)//QPEL
		VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 3);
	else
		VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // motion vectors 8x8_0
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // motion vectors 8x8_1
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // motion vectors 8x8_2
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // motion vectors 8x8_3
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 1) = mv8_2.format<uint>()[0]; // motion vectors 8x8_2_0
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 1) = mv8_2.format<uint>()[1]; // motion vectors 8x8_2_1
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 1) = mv8_2.format<uint>()[2]; // motion vectors 8x8_2_2
	fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 1) = mv8_2.format<uint>()[3]; // motion vectors 8x8_2_3
	run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, 0, 170, fbrOut8x8);
	VME_GET_FBROutput_Rec0_8x8_4Mv(fbrOut8x8, mv8.format<uint>());
	VME_GET_FBROutput_Rec1_8x8_4Mv(fbrOut8x8, mv8_2.format<uint>());
	VME_GET_FBROutput_Dist_8x8_Bi(fbrOut8x8, dist8);


	// distortions actual complete distortion calculation
	// 8x8
	//write(SURF_DIST8x8  , mbX * DIST_SIZE * 2  , mbY * 2, dist8.format<uint,2,2>());     //8x8 Bidir distortions

	// motion vectors
	// 8x8
	write(SURF_MV8x8, mbX * MVDATA_SIZE * 2, mbY * 2, mv8);       //8x8mvs  Ref0
	write(SURF_MV8x8_2, mbX * MVDATA_SIZE * 2, mbY * 2, mv8_2);     //8x8mvs  Ref1
}